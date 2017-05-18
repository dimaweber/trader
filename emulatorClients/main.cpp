#include "btce.h"
#include "../emul/sqlclient.h"
#include "utils.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>

#include <iostream>
#include <memory>

#include <curl/curl.h>

#include <unistd.h>

void connectDatabase(QSqlDatabase& database, QSettings& settings)
{
    settings.beginGroup("database");
    while (!database.isOpen())
    {
        QString db_type = settings.value("type", "unknown").toString();
        if (db_type == "mysql")
        {
            std::clog << "use mysql database" << std::endl;
            QSqlDatabase::removeDatabase("emul_db");
            database = QSqlDatabase::addDatabase("QMYSQL", "trader_db");
        }
        else
        {
            throw std::runtime_error("unsupported database type");
        }
        database.setHostName(settings.value("host", "localhost").toString());
        database.setUserName(settings.value("user", "user").toString());
        database.setPassword(settings.value("password", "password").toString());
        database.setDatabaseName(settings.value("database", "db").toString());
        database.setPort(settings.value("port", 3306).toInt());
        QString optionsString = QString("MYSQL_OPT_RECONNECT=%1").arg(settings.value("options.reconnect", true).toBool()?"TRUE":"FALSE");
        database.setConnectOptions(optionsString);

        std::clog << "connecting to database ... ";
        if (!database.open())
        {
            std::clog << " FAIL. " << database.lastError().text() << std::endl;
            usleep(1000 * 1000 * 5);
        }
        else
        {
            std::clog << " ok" << std::endl;
        }
    }
    settings.endGroup();
}

struct ClientThreadData
{
    int id;
    QSqlDatabase* pDb;
};

quint32 nonce()
{
    static QAtomicInt value = QDateTime::currentDateTime().toTime_t();
    return value++;
}

class DummyStorage : public IKeyStorage
{
    SqlClient client;
    QByteArray key;
    QByteArray sec;
public:
    DummyStorage(QSqlDatabase& db):client(db){}
    virtual void setPassword(const QByteArray& pwd) override { throw std::runtime_error("not implemented");}
    virtual bool setCurrent(int id) override { throw std::runtime_error("not implemented");}
    virtual const QByteArray& apiKey() override { return key; }
    virtual const QByteArray& secret() override { return sec;}

    virtual void changePassword() override { throw std::runtime_error("not implemented");}
    virtual QList<int> allKeys() override { throw std::runtime_error("not implemented");}

    bool loadKeyForCurrencyBalance(const QString& currency, double balance)
    {
        key = client.randomKeyForTrade(currency, balance);
        if (key.isEmpty())
        {
            std::cerr << "fail to load key" << std::endl;
            return false;
        }
        sec = client.secretForKey(key);
        if (sec.isEmpty())
        {
            std::cerr << "fail to load sec" << std::endl;
            return false;
        }

        return true;
    }
};

static void* clienthread(void* data)
{
    ClientThreadData* pData = static_cast<ClientThreadData*>(data);
    QString threadName = QString("client-thread-%1").arg(pData->id);
    QString dbConnectionName = QString("client-db-%1").arg(pData->id);
    std::unique_ptr<QSqlDatabase> db = std::make_unique<QSqlDatabase>(QSqlDatabase::cloneDatabase(*pData->pDb, dbConnectionName));
    db->open();

    delete pData;

    std::unique_ptr<DummyStorage> storage = std::make_unique<DummyStorage>(*db);
    BtcObjects::Funds funds;

    while(true)
    {
        bool isSell = qrand() % 2;
        double amount = (qrand() % 1000) / 1000.0 + 0.01;
        double rate;
        double balance;
        QString currency;
        rate = 1750 + (qrand() % 100) / 10.0 - 5;
        if (isSell)
        {
            balance = amount;
            currency = "btc";
        }
        else
        {
            balance = rate * amount;
            currency = "usd";
        }

        if (!storage->loadKeyForCurrencyBalance(currency, balance))
            continue;

        BtcTradeApi::Trade trade(*storage, funds, "btc_usd", isSell?BtcObjects::Order::Type::Sell:BtcObjects::Order::Type::Buy, rate, amount);
        try
        {
            performTradeRequest("trade", trade, false);
        }
        catch(std::runtime_error& e)
        {
            std::cerr << e.what() << std::endl;
        }
        //usleep(1500);
    }

    storage.reset();
    db->close();
    db.reset();

    QSqlDatabase::removeDatabase(dbConnectionName);
}

int main(int argc, char *argv[])
{
    CurlWrapper wrapper;

    QCoreApplication a(argc, argv);
    QString iniFilePath = QCoreApplication::applicationDirPath() + "/../data/emul.ini";
    if (!QFileInfo(iniFilePath).exists())
    {
        std::cerr << "*** No INI file!" << std::endl;
        return 0;
    }
    QSettings settings(iniFilePath, QSettings::IniFormat);

    QString serverAddress = settings.value("emulator/server_address", "http://example.com").toString();
    BtcPublicApi::Api::setServer(serverAddress);
    BtcTradeApi::Api::setServer(serverAddress);

    QSqlDatabase db;
    connectDatabase(db, settings);

    const quint32 THREAD_COUNT = settings.value("debug/client_threads_count", 8).toUInt();
    pthread_t id[THREAD_COUNT];

    for (size_t i=0; i<THREAD_COUNT-1; i++)
    {
        ClientThreadData* pData = new ClientThreadData;
        pData->pDb = &db;
        pData->id=i;
        pthread_create(&id[i], nullptr, clienthread, pData);
    }

    ClientThreadData* pData = new ClientThreadData;
    pData->pDb = &db;
    pData->id=THREAD_COUNT-1;
    clienthread(pData);

    for (size_t i=0; i<THREAD_COUNT-1; i++)
        pthread_join(id[i], nullptr);

    return 0;
}
