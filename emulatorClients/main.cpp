#include "btce.h"
#include "../emul/sqlclient.h"
#include "utils.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QReadWriteLock>
#include <QRegExp>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
#include <QVector>

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
    unsigned int id;
    QSqlDatabase* pDb;
};

quint32 nonce(quint32 s = 0)
{
    static QAtomicInt value = QDateTime::currentDateTime().toTime_t();
    if (!s)
    {
        return value++;
    }
    else
    {
        value = s;
        return 0;
    }
}

class DummyStorage : public IKeyStorage
{
    std::shared_ptr<AbstractDataAccessor> client;
    QByteArray key;
    QByteArray sec;
public:
    DummyStorage(QSqlDatabase& db):client(std::make_shared<DirectSqlDataAccessor>(db)){}
    virtual void setPassword(const QByteArray& ) override { throw std::runtime_error("not implemented");}
    virtual bool setCurrent(int ) override { throw std::runtime_error("not implemented");}
    virtual const QByteArray& apiKey() override { return key; }
    virtual const QByteArray& secret() override { return sec;}

    virtual void changePassword() override { throw std::runtime_error("not implemented");}
    virtual QList<int> allKeys() override { throw std::runtime_error("not implemented");}

    bool loadKeyForCurrencyBalance(const QString& currency, Amount balance)
    {
        key = client->randomKeyForTrade(currency, balance);
        if (key.isEmpty())
        {
            std::cerr << "fail to load key" << std::endl;
            return false;
        }
        sec = client->secretForKey(key);
        if (sec.isEmpty())
        {
            std::cerr << "fail to load sec" << std::endl;
            return false;
        }

        return true;
    }
};

double trend = 1.00;

static void* clienthread(void* data)
{
    ClientThreadData* pData = static_cast<ClientThreadData*>(data);
    unsigned int id = pData->id;
    QString dbConnectionName = QString("client-db-%1").arg(id);
    std::unique_ptr<QSqlDatabase> db = std::make_unique<QSqlDatabase>(QSqlDatabase::cloneDatabase(*pData->pDb, dbConnectionName));
    db->open();

    delete pData;

    qsrand(id);
    std::unique_ptr<DummyStorage> storage = std::make_unique<DummyStorage>(*db);
    BtcObjects::Funds funds;
    QRegExp rx("you should send:([0-9]*)");

    static QReadWriteLock tickerAccess;
    BtcPublicApi::Info btceInfo;
    BtcPublicApi::Ticker btceTicker;

    if (BtcObjects::Pairs::ref().size() == 0)
    {
        QWriteLocker wlock(&tickerAccess);
        if (BtcObjects::Pairs::ref().size() == 0)
            btceInfo.performQuery();
    }

    while(true)
    {
        try {
            if (id == 0)
            {
                QWriteLocker wlock(&tickerAccess);
                btceTicker.performQuery();
            }
        }
        catch(BadFieldValue& e)
        {
            continue;
        }
        catch (HttpError& e)
        {
            usleep(150);
            continue;
        }

        bool isSell = qrand() % 2;
        QVector<QString> pairs;
        pairs << "btc_usd" << "btc_eur" << "eth_btc" << "eth_eur" << "eth_usd" << "ltc_usd" << "ltc_eur";
        QString pair = pairs [ qrand() % pairs.size()];
        tickerAccess.lockForRead();
        double last = isSell?BtcObjects::Pairs::ref(pair).ticker.sell:BtcObjects::Pairs::ref(pair).ticker.buy;
        double base_price = last * trend;
        tickerAccess.unlock();

        Amount amount ((qrand() % 1000) / 1000.0 + 0.01);
        Rate rate (base_price + (qrand() % 100) / 10.0 - 5);
        Amount balance;
        QString currency;
        if (isSell)
        {
            balance = amount;
            currency = pair.left(3);
        }
        else
        {
            balance = rate * amount;
            currency = pair.right(3);
        }

        if (!storage->loadKeyForCurrencyBalance(currency, balance))
            continue;

        BtcTradeApi::Trade trade(*storage, funds, pair, isSell?BtcObjects::Order::Type::Sell:BtcObjects::Order::Type::Buy, rate.getAsDouble(), amount.getAsDouble());
        try
        {
            performTradeRequest("trade", trade, false);
        }
        catch(std::runtime_error& e)
        {
            std::cerr << e.what() << std::endl;
            if (rx.indexIn(QString::fromStdString(e.what())) > 0)
            {
                nonce(rx.cap(1).toUInt());
            }
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

    for (size_t i=0; i<THREAD_COUNT; i++)
    {
        ClientThreadData* pData = new ClientThreadData;
        pData->pDb = &db;
        pData->id=i;
        pthread_create(&id[i], nullptr, clienthread, pData);
    }


    while (true)
    {
        trend = trend + (qrand() % 100) / 1000.0;
        sleep(100);
    }

    for (size_t i=0; i<THREAD_COUNT-1; i++)
        pthread_join(id[i], nullptr);

    return 0;
}
