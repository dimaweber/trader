#include "btce.h"
#include "fcgi_request.h"
#include "query_parser.h"
#include "sql_database.h"
#include "tablefield.h"
#include "unit_tests.h"
#include "utils.h"

#include <iostream>
#include <unistd.h>
#include <memory>

#include <QtConcurrent>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSettings>
#include <QtTest>


void prepareDatabase(QSqlDatabase& db)
{
    QMap<QString, QStringList> createSqls;
    QSqlQuery sql(db);

    sql.exec("drop database emul_debug");
    sql.exec("create database emul_debug");
    sql.exec("use emul_debug");

    createSqls["users"]
        << TableField("user_id").primaryKey(true)
        << TableField("name", TableField::Char, 128)
           << "CONSTRAINT uniq_name UNIQUE (name)"
        ;

    createSqls["currencies"]
            << TableField("currency_id").primaryKey(true)
            << TableField("currency", TableField::Char, 3)
            << "CONSTRAINT uniq_currency UNIQUE (currency)"
               ;

    createSqls["deposits"]
            << TableField("user_id").references("users", {"user_id"})
            << TableField("currency_id").references("currencies", {"currency_id"})
            << TableField("volume", TableField::Decimal, 14, 6).notNull().check("amount >= 0")
            << "FOREIGN KEY(currency_id) REFERENCES currencies(currency_id)"
            << "FOREIGN KEY(user_id) REFERENCES users(user_id)"
            << "CONSTRAINT uniq_usercurrency UNIQUE (user_id,currency_id)"
               ;

    createSqls["apikeys"]
            << TableField("user_id").references("users", {"user_id"})
            << TableField("apikey", TableField::Char, 128).notNull()
            << TableField("secret", TableField::Char, 128).notNull()
            << TableField("info", TableField::Boolean).notNull().defaultValue("FALSE")
            << TableField("trade", TableField::Boolean).notNull().defaultValue("FALSE")
            << TableField("withdraw", TableField::Boolean).notNull().defaultValue("FALSE")
            << TableField("nonce", TableField::Integer).notNull().defaultValue(1).check("nonce > 0")
            << "FOREIGN KEY(user_id) REFERENCES users(user_id)"
               ;

    createSqls["pairs"]
            << TableField("pair_id").primaryKey(true)
            << TableField("pair", TableField::Char, 7)
            << TableField("decimal_places", TableField::Integer).check("decimal_places > 0 and decimal_places <= 6")
            << TableField("min_price", TableField::Decimal, 14, 6).check("min_price > 0")
            << TableField("max_price", TableField::Decimal, 14, 6).check("max_price >= min_price")
            << TableField("min_amount", TableField::Decimal, 14, 6).check("min_amount > 0")
            << TableField("hidden", TableField::Boolean)
            << TableField("fee", TableField::Decimal, 6, 1).check("fee >= 0 and fee < 100")
            << "CONSTRAINT uniq_pair UNIQUE (pair)"
            ;

    createSqls["orders"]
            << TableField("order_id").primaryKey(true)
            << TableField("pair_id").references("pairs", {"pair_id"})
            << TableField("user_id").references("users", {"user_id"})
            << "type enum ('sell', 'buy') not null"
            << TableField("rate", TableField::Decimal, 14, 6).notNull().check("rate > 0")
            << TableField("start_amount", TableField::Decimal, 14, 6).notNull().check("start_amount > 0")
            << TableField("amount", TableField::Decimal, 14, 6).notNull().check("amount >= 0 and amount <= start_amount")
            << "status enum ('active', 'done', 'cancelled', 'part_done') not null"
            << TableField("created", TableField::Datetime).notNull()
            << "FOREIGN KEY(pair_id) REFERENCES pairs(pair_id)"
            << "FOREIGN KEY(user_id) REFERENCES users(user_id)"
               ;

    createSqls["trades"]
            << TableField("trade_id").primaryKey(true)
            << TableField("order_id").references("orders", {"order_id"})
            << TableField("user_id").references("users", {"user_id"})
            << TableField("amount", TableField::Decimal, 14, 6).notNull()
            << TableField("created", TableField::Datetime).notNull()
            << "FOREIGN KEY(order_id) REFERENCES orders(order_id)"
            << "FOREIGN KEY(user_id) REFERENCES users(user_id)"
               ;

    createSqls["ticker"]
            << TableField("pair_id").references("pairs", {"pair_id"})
            << TableField("high", TableField::Decimal, 14, 6).notNull()
            << TableField("low", TableField::Decimal, 14, 6).notNull()
            << TableField("avg", TableField::Decimal, 14, 6).notNull()
            << TableField("vol", TableField::Decimal, 14, 6).notNull()
            << TableField("vol_cur", TableField::Decimal, 14, 6).notNull()
            << TableField("last", TableField::Decimal, 14, 6).notNull()
            << TableField("buy", TableField::Decimal, 14, 6).notNull()
            << TableField("sell", TableField::Decimal, 14, 6).notNull()
            << TableField("updated", TableField::Datetime).notNull()
            << "FOREIGN KEY(pair_id) REFERENCES pairs(pair_id)"
               ;

    sql.exec("SET FOREIGN_KEY_CHECKS = 0");
    for (const QString& tableName : createSqls.keys())
    {
        QStringList& fields = createSqls[tableName];
        QString createSql = QString("CREATE TABLE IF NOT EXISTS %1 (%2) character set utf8 COLLATE utf8_general_ci")
                            .arg(tableName)
                            .arg(fields.join(','))
                            ;
        QString caption = QString("create %1 table").arg(tableName);

        performSql(caption, sql, createSql, true);
    }
    sql.exec("SET FOREIGN_KEY_CHECKS = 1");
}

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
#ifndef Q_OS_WIN
            usleep(1000 * 1000 * 5);
#else
            Sleep(1000);
#endif
        }
        else
        {
            std::clog << " ok" << std::endl;
        }
    }
    settings.endGroup();
}

void populateTablesFromCsv(QSqlDatabase& db)
{
    QSqlQuery query(db);
    for(const QString& tableName: db.tables())
    {
        QString fileName = QString("%1.csv").arg(tableName);
        QFile inputFile (fileName);
        if (inputFile.exists())
        {
            QString insertSqlTemplate ("insert into %1 (%2) values (%3)");
            QMap<int, QVariantList> valuesList;
            if (inputFile.open(QFile::ReadOnly))
            {
                QString inputLine = QString::fromUtf8(inputFile.readLine()).simplified();
                QStringList v = inputLine.split(';');
                QStringList v2 = v;
                v2.replaceInStrings(QRegExp("^.*$"), "?");
                QString str = insertSqlTemplate.arg(tableName, v.join(','), v2.join(','));
                std::clog << str << std::endl;
                if (!query.prepare(str))
                {
                    std::cerr << query.lastError().text();
                }
                while(!inputFile.atEnd())
                {
                    inputLine = QString::fromUtf8(inputFile.readLine()).simplified();
                    v = inputLine.split(';');
                    for (int i=0; i<v.size();i++)
                        valuesList[i] << v[i];
                }

                for (int key: valuesList.keys())
                    query.addBindValue(valuesList[key]);

                if (!query.execBatch())
                    std::cerr << query.lastError().text() << std::endl;
            }
            else
            {
                std::cerr << "Fail to open file " << fileName << " for read." << std::endl;
            }
        }
        else
        {
            std::clog << "No csv file for table " << tableName << " exists, skipping" << std::endl;
        }
    }
}

QString randomString(const int len)
{
    QString out(len);
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    for (int i = 0; i < len; ++i) {
        out[i] = alphanum[qrand() % (sizeof(alphanum) - 1)];
    }

    return out;
}

QString randomApiKey()
{
    QStringList parts;
    for(int i=0;i<5;i++)
        parts.append(randomString(8));
    return parts.join('-').toUpper();
}

QString randomSecret()
{
    constexpr int len = strlen("****************************************************************");
    return randomString(len).toLower();
}

static QVector<quint32> userIdCache;
static QSet<quint32> currencyIdCache;

quint32 get_random_user_id(QVector<quint32> except = QVector<quint32>())
{
    quint32 id;
    do
    {
        int rndIndex = qrand() % userIdCache.size();
        id = userIdCache[rndIndex];
    } while (except.contains(id));
    return id;
}

void buildOrdersFromDepth(const BtcObjects::Depth::Position& position, const BtcObjects::Pair& p, QSqlQuery& ordersInsertQuery, bool isAsks, QVariantMap& orderParams)
{
    float amount = position.amount;
    float rate = position.rate;

    int usersCount = 0;
    QMap<int, float> usersPropotions;
    if (amount < p.min_amount * 10)
    {
        // force only one user for tiny amounts
        usersCount = 1;
    }
    else
    {
        int dice = qrand() % 10;
        if (dice < 5) usersCount = 1;
        else if (dice < 7) usersCount = 2;
        else if (dice < 9) usersCount = 3;
        else usersCount = 4;
    }
    if (usersCount == 1)
        usersPropotions[0] = amount;
    else
    {
        float rest = amount;
        // first n-1 users take thei parts
        for(int i=0; i<usersCount-1; i++)
        {
            float part = (qrand() % 1000) / 1000.0;
            usersPropotions[i] = rest * part;
            rest = rest * (1-part);
        }
        // last one take whole rest
        usersPropotions[usersCount-1] = rest;
    }

    for (int i=0; i < usersCount; i++)
    {
        orderParams[":user_id"] = get_random_user_id(QVector<quint32>() << EXCHNAGE_USER_ID);
        orderParams[":type"] = isAsks?"sell":"buy";
        orderParams[":rate"] = QString::number(rate, 'f', p.decimal_places);
        orderParams[":start_amount"] = usersPropotions[i];
        orderParams[":amount"] = orderParams[":start_amount"];
        orderParams[":status"] = "active";
        orderParams[":created"] = QDateTime::currentDateTime();

        performSql("insert order", ordersInsertQuery, orderParams, true);
    }
}

void buildTradesFromBtce(const BtcObjects::Trade& trade, QSqlQuery& tradesInsertQuery, QSqlQuery& selectOrdersWithGivenPairRate, QSqlQuery& updateOrderIncreaseStartAmount, QSqlQuery& ordersInsertQuery, QVariantMap& orderParams)
{
    QVariantMap tradesParams;
    QVariant order_id;
    quint32 orderuser_id = 0;

    selectOrdersWithGivenPairRate.bindValue(":rate", trade.price);
    selectOrdersWithGivenPairRate.bindValue(":type", (trade.type==BtcObjects::Trade::Type::Bid)?"sell":"buy");
    if (selectOrdersWithGivenPairRate.exec())
    {
        if (selectOrdersWithGivenPairRate.next())
        {
            order_id = selectOrdersWithGivenPairRate.value(0);
            orderuser_id = selectOrdersWithGivenPairRate.value(1).toUInt();
            updateOrderIncreaseStartAmount.bindValue(":order_id", order_id);
            updateOrderIncreaseStartAmount.bindValue(":increase", trade.amount);
            updateOrderIncreaseStartAmount.bindValue(":created", trade.timestamp.addSecs(-qrand() % 65534 + 1));
            updateOrderIncreaseStartAmount.exec();
        }
    }

    if (!order_id.isValid())
    {
        orderuser_id = get_random_user_id(QVector<quint32>() << EXCHNAGE_USER_ID);
        orderParams[":user_id"] = orderuser_id;
        orderParams[":type"] = (trade.type == BtcObjects::Trade::Type::Bid)?"buy":"sell";
        orderParams[":rate"] = QString::number(trade.price, 'f', 6); // TODO: is rate precision is always 3 ?
        orderParams[":start_amount"] = trade.amount;
        orderParams[":amount"] = 0;
        orderParams[":status"] = "done";
        orderParams[":created"] = trade.timestamp.addSecs(-qrand() % 65534 + 1);

        performSql("insert done order", ordersInsertQuery, orderParams, true);

        order_id = ordersInsertQuery.lastInsertId();
    }

    tradesParams[":trade_id"] = trade.id;
    tradesParams[":user_id"] = get_random_user_id(QVector<quint32>() << EXCHNAGE_USER_ID << orderuser_id);
    tradesParams[":order_id"] = order_id;
    tradesParams[":amount"] = trade.amount;
    tradesParams[":created"] = trade.timestamp;

    performSql("insert trade", tradesInsertQuery, tradesParams, true);
}

void populateTablesFromBtce(QSqlDatabase& database)
{
    //for (const QString& pair: BtcObjects::Pairs::ref().keys())
    QStringList currencies;
    QMutex currenciesAccess;

    auto func = [&database, &currencies, &currenciesAccess](const QString& pair) -> void
    {
        QString connectionName = QString("orders-%1-%2").arg((uintptr_t)QThread::currentThreadId()).arg(QDateTime::currentDateTime().toTime_t());
        {
            QSqlDatabase db = QSqlDatabase::cloneDatabase(database, connectionName);
            if (db.open())
            {
                db.exec("START TRANSACTION");
                QSqlQuery pairInsertQuery(db);
                QSqlQuery tickerInsertQuery(db);
                QSqlQuery ordersInsertQuery(db);
                QSqlQuery tradesInsertQuery(db);
                QSqlQuery selectOrdersWithGivenPairRate(db);
                QSqlQuery updateOrderIncreaseStartAmount(db);
                QSqlQuery insertCurrencyQuery(db);

                pairInsertQuery.prepare("insert into pairs (pair, decimal_places, min_price, max_price, min_amount, hidden, fee) values (:pair, :decimal_places, :min_price, :max_price, :min_amount, :hidden, :fee)");
                tickerInsertQuery.prepare("insert into ticker (pair_id, high, low, avg, vol, vol_cur, buy, sell, last, updated) values (:pair_id, :high, :low, :avg, :vol, :vol_cur, :buy, :sell, :last, :updated)");
                ordersInsertQuery.prepare("insert into orders (pair_id, user_id,   type,   rate, start_amount, amount,   status, created)"
                                          "values (:pair_id, :user_id,   :type,   :rate, :start_amount, :amount,   :status, :created)");
                tradesInsertQuery.prepare("insert into trades (trade_id, order_id, user_id, amount, created) values (:trade_id, :order_id, :user_id, :amount, :created)");
                selectOrdersWithGivenPairRate.prepare("select order_id, user_id, created from orders o where pair_id=:pair_id and o.rate=:rate and type=:type order by created asc");
                updateOrderIncreaseStartAmount.prepare("update orders set start_amount = start_amount + :increase, created = :created where order_id=:order_id");
                insertCurrencyQuery.prepare("insert into currencies (currency) values (:currency)");

                QVariantMap pairParams;
                QVariantMap tickerParams;
                QVariantMap orderParams;

                std::clog << "create pair " <<pair << " record " << std::endl;
                BtcObjects::Pair& p = BtcObjects::Pairs::ref(pair);
                pairParams[":pair"] = pair;
                pairParams[":decimal_places"] = p.decimal_places;
                pairParams[":min_price"] =  p.min_price;
                pairParams[":max_price"] = p.max_price;
                pairParams[":min_amount"] = p.min_amount;
                pairParams[":hidden"] = p.hidden;
                pairParams[":fee"] = p.fee;
                performSql("insert pair", pairInsertQuery, pairParams, true);

                quint32 pair_id = pairInsertQuery.lastInsertId().toUInt();

                QStringList c = pair.split('_');
                currenciesAccess.lock();
                QVariantMap currencyParams;
                for(QString& cu: c)
                {
                    if (!currencies.contains(cu))
                    {
                        currencyParams[":currency"] = cu;
                        performSql("insert currency", insertCurrencyQuery, currencyParams, true);
                        currencyIdCache.insert( insertCurrencyQuery.lastInsertId().toUInt());
                        currencies.append(cu);
                        std::clog << '[' << QDateTime::currentDateTime().toString(Qt::ISODate) << "] currency " << cu << " added " << std::endl;
                    }
                }
                currenciesAccess.unlock();

                tickerParams[":pair_id"] = pair_id;
                tickerParams[":high"] = p.ticker.high;
                tickerParams[":low"] = p.ticker.low;
                tickerParams[":avg"] = p.ticker.avg;
                tickerParams[":vol"] = p.ticker.vol;
                tickerParams[":vol_cur"] = p.ticker.vol_cur;
                tickerParams[":buy"] = p.ticker.buy;
                tickerParams[":sell"] = p.ticker.sell;
                tickerParams[":last"] = p.ticker.last;
                tickerParams[":updated"] = p.ticker.updated;
                performSql("insert ticker", tickerInsertQuery, tickerParams, true);
                std::clog << '[' << QDateTime::currentDateTime().toString(Qt::ISODate) << "] ticker record for pair " << pair << " created " << std::endl;

                orderParams[":pair_id"] = pair_id;
                quint32 cnt = 0;
                for(const BtcObjects::Depth::Position& position: p.depth.asks)
                {
                    buildOrdersFromDepth(position, p, ordersInsertQuery, true, orderParams);
                    cnt++;
                }
                std::clog << '[' << QDateTime::currentDateTime().toString(Qt::ISODate) << "] for pair " << pair << " " << cnt << " sell orders created " << std::endl;

                cnt = 0;
                for(const BtcObjects::Depth::Position& position: p.depth.bids)
                {
                    buildOrdersFromDepth(position, p, ordersInsertQuery, false, orderParams);
                    cnt++;
                }
                std::clog << '[' << QDateTime::currentDateTime().toString(Qt::ISODate) << "] for pair " << pair << " " << cnt << " buy orders created " << std::endl;

                selectOrdersWithGivenPairRate.bindValue(":pair_id", pair_id);
                cnt = 0;
                for (const BtcObjects::Trade& trade: p.trades)
                {
                    buildTradesFromBtce(trade, tradesInsertQuery, selectOrdersWithGivenPairRate, updateOrderIncreaseStartAmount, ordersInsertQuery, orderParams);
                    cnt ++;
                }
                std::clog << '[' << QDateTime::currentDateTime().toString(Qt::ISODate) << "] for pair " << pair << " " << cnt << " complete trades created " << std::endl;

                db.exec("COMMIT");
                db.close();
            }
            else
                std::cerr << db.lastError().text() << std::endl;

        }
        QSqlDatabase::removeDatabase(connectionName);
    };

    QList<QString> keys = BtcObjects::Pairs::ref().keys();
    QtConcurrent::blockingMap(keys, func);
}

void createSecrets(QSqlDatabase& database)
{
    QAtomicInt dbCounter = 0;
    auto func = [&database, &dbCounter](quint32 user_id)
    {
        qsrand(dbCounter++);
        QString connectionName = QString("secrets-%1").arg(++dbCounter);
        {
            QSqlDatabase db = QSqlDatabase::cloneDatabase(database, connectionName);
            if (db.open())
            {
                db.exec("START TRANSACTION");
                QSqlQuery insertSecret(db);
                insertSecret.prepare("insert into apikeys (user_id,apikey,secret,info,trade, withdraw) values (:user_id, :apikey, :secret, :info, :trade, :withdraw)");
                QVariantMap apikeyParams;

                for (int info=0; info<2;info++)
                    for (int trade=0; trade<2;trade++)
                        for (int withdraw=0; withdraw<2;withdraw++)
                        {
                            apikeyParams[":user_id"] = user_id;
                            apikeyParams[":apikey"] = randomApiKey();
                            apikeyParams[":secret"] = randomSecret();
                            apikeyParams[":info"] = (bool)info;
                            apikeyParams[":trade"] = (bool)trade;
                            apikeyParams[":withdraw"] = (bool)withdraw;

                            performSql("insert apikey", insertSecret, apikeyParams, true) ;
                        }

                db.exec("COMMIT");
                db.close();
            }
            else
                std::cerr << db.lastError().text() << std::endl;
        }
        QSqlDatabase::removeDatabase(connectionName);
    };

    QtConcurrent::blockingMap(userIdCache, func);
}

void createDeposits(QSqlDatabase& database)
{
    QAtomicInt dbCounter = 0;
    auto func = [&database, &dbCounter](quint32 user_id)
    {
        qsrand(dbCounter++);
        QString connectionName =  QString("deposits-%1").arg(++dbCounter);
        {
            QSqlDatabase db = QSqlDatabase::cloneDatabase(database,connectionName);
            if (db.open())
            {
                db.exec("START TRANSACTION");
                QSqlQuery insertDepositQuery(db);
                insertDepositQuery.prepare("insert into deposits (user_id, currency_id, volume) values (:user_id, :currency_id, :volume)");
                QVariantMap depositsParams;
                depositsParams[":user_id"] = user_id;
                for(quint32 pair_id: currencyIdCache)
                {
                    depositsParams[":currency_id"] = pair_id;
                    if (qrand() % 3 == 0)
                    {
                        depositsParams[":volume"] = (((static_cast<long>(qrand()) << 30) + qrand()) % 1000000) / 100.0;
                    }
                    else
                        depositsParams[":volume"] = 0;

                    performSql("insert deposit", insertDepositQuery, depositsParams, true);
                }
                db.exec("COMMIT");
                db.close();
            }
            else
                std::cerr << db.lastError().text() << std::endl;
        }
        QSqlDatabase::removeDatabase(connectionName);
    };
    QtConcurrent::blockingMap(userIdCache, func);
}

void populateDatabase(QSqlDatabase& db, int trades_limit, int depth_limit)
{
    BtcPublicApi::Info btceInfo;
    BtcPublicApi::Ticker btceTicker;
    BtcPublicApi::Depth btceDepth(depth_limit);
    BtcPublicApi::Trades btceTrades(trades_limit);

    btceInfo.performQuery();
    btceTicker.performQuery();
    btceDepth.performQuery();
    btceTrades.performQuery();


    bool inTransaction;
    inTransaction = db.transaction();
    if (!inTransaction)
    {
        std::clog << "fail to start transaction -- expect very slow inserts" << std::endl;
        if (!db.driver()->hasFeature(QSqlDriver::DriverFeature::Transactions))
            std::clog << "Transactions are not supported by sql driver" << std::endl;
        //performSql("manual transaction start", query, "START TRANSACTION", true);
    }
    QSqlQuery query(db);
    populateTablesFromCsv(db);

    performSql("add user_type column", query, "alter table users add user_type int not null default 1", true); // 0 - special (exchannge), 1 - emulated, 2 - regular

    performSql("create EXCHANGE user", query, QString("insert into users (user_id, name, user_type) values (%1, 'EXCHANGE', 0)").arg(EXCHNAGE_USER_ID), true);

    performSql("fill usersIdDache", query, "select user_id from users", true);
    while (query.next())
        userIdCache.append( query.value(0).toUInt());

    populateTablesFromBtce(db);
    createSecrets(db);
    createDeposits(db);

    if (inTransaction)
        db.commit();
   // else
      //  performSql("manual transaction commit", query, "COMMIT", true);
}

struct FcgiThreadData
{
    int id;
    int sock;
    QSqlDatabase* pDb;
};

static pthread_mutex_t acceptAccessMutex = PTHREAD_MUTEX_INITIALIZER;

QAtomicInt processed_total = 0;

static void* fcgiThread(void* data)
{
    FcgiThreadData* pData = static_cast<FcgiThreadData*>(data);
    QString threadName = QString("fcgi-thread-%1").arg(pData->id);
    QString dbConnectionName = QString("fcgi-db-%1").arg(pData->id);
    FcgiRequest request(pData->sock);
    std::clog << "[FastCGI " << threadName << "] Request initialized, ready to work" << std::endl;

    std::unique_ptr<QSqlDatabase> db = std::make_unique<QSqlDatabase>(QSqlDatabase::cloneDatabase(*pData->pDb, dbConnectionName));
    db->open();
    std::unique_ptr<Responce> responce = std::make_unique<Responce>(*db);

    delete pData;

    while(true)
    {
        pthread_mutex_lock(&acceptAccessMutex);
        int rc = request.accept();
        pthread_mutex_unlock(&acceptAccessMutex);

        if (rc < 0)
            break;


//        std::clog << "[FastCGI " << threadName << "] New request accepted. Processing" << std::endl;

        QueryParser httpQuery(request);

        QString json;
        QVariantMap var;
        Method method;
        QElapsedTimer timer;
        timer.start();
        var = responce->getResponce(httpQuery, method);
        QJsonDocument doc = QJsonDocument::fromVariant(var);
        json = doc.toJson().constData();
        quint32 elapsed = timer.elapsed();

        request.put ( "Content-type: application/json\r\n");
        request.put ( "XXX-Emulator: true\r\n");
        request.put ( QString("XXX-Emulator-DbTime: %1\r\n").arg(elapsed));
        request.put ("\r\n");
        request.put ( json);

        processed_total ++;
        request.finish();
//        std::clog << "[FastCGI " << threadName << "] Request finished" << std::endl;
    }

    responce.reset();
    db->close();
    db.reset();

    QSqlDatabase::removeDatabase(dbConnectionName);

    return NULL;
}

int main(int argc, char *argv[])
{
    bool recreateDatabase = false;
    bool runTests = false;
    bool failTestExit = true;
    bool justTests;
    int depth_limit = 150;
    int trades_limit = 150;

    QCoreApplication app(argc, argv);
    QString iniFilePath = QCoreApplication::applicationDirPath() + "/../data/emul.ini";
    if (!QFileInfo(iniFilePath).exists())
    {
        std::cerr << "*** No INI file!" << std::endl;
        return 0;
    }
    QSettings settings(iniFilePath, QSettings::IniFormat);
    recreateDatabase = settings.value("debug/recreate_database", true).toBool();
    runTests = settings.value("debug/run_tests", true).toBool();
    failTestExit = settings.value("debug/exit_on_test_fail", false).toBool();
    justTests = settings.value("debug/just_tests", false).toBool();
    depth_limit = settings.value("btce/depth_limit", 150).toInt();
    trades_limit = settings.value("btce/trades_limit", 150).toInt();

    BtcPublicApi::Api::setServer("https://btc-e.com");
    BtcTradeApi::Api::setServer("https://btc-e.com");

    QSqlDatabase db;
    connectDatabase(db, settings);
    if (recreateDatabase)
    {
        prepareDatabase(db);
        populateDatabase(db, trades_limit, depth_limit);

        settings.setValue("debug/recreate_database", false);
        settings.sync();
    }

    if (runTests)
    {
        BtceEmulator_Test test(db);
        int testReturnCode = QTest::qExec(&test, argc, argv);

        if (failTestExit && testReturnCode)
            return testReturnCode;
    }
    if (justTests)
        return 0;

    int ret;
    int sock;
    ret = FCGX_Init();
    if (ret < 0)
    {
        std::cerr << "[FastCGI] Fail to initialize" << std::endl;
        return 1;
    }
    std::clog << "[FastCGI] Initilization done" << std::endl;

    sock = FCGX_OpenSocket(":5123", 10);
    if (sock < 1)
    {
        std::cerr << "[FastCGI] Fail to open socket" << std::endl;
        return 2;
    }
    std::clog << "[FastCGI]  Socket opened" << std::endl;

    const quint32 THREAD_COUNT = settings.value("emulator/threads_count", 8).toUInt();
    std::vector<pthread_t> id(THREAD_COUNT);

    for (size_t i=0; i<THREAD_COUNT; i++)
    {
        FcgiThreadData* pData = new FcgiThreadData;
        pData->sock = sock;
        pData->pDb = &db;
        pData->id=i;
        pthread_create(&id[i], nullptr, fcgiThread, pData);
    }


    Responce r(db);
    QVariantMap initialBalance = r.exchangeBalance();
    QElapsedTimer timer;
    timer.start();
    while (true)
    {
        QVariantMap balance = r.exchangeBalance();

        if (balance != initialBalance)
        {
            std::cerr << "***** ERROR **** : balance mismatch " << std::endl;
            throw 1;
        }
        else
            std::clog << "Balance ok" << std::endl;

        OrderInfo::List lst = r.negativeAmountOrders();
        if (lst.size() > 0)
        {
            std::cerr << "Broken database: orders with negative amount!" << std::endl;
            throw 2;
        }

        r.updateTicker();

        sleep(30);

        int proc = processed_total.fetchAndStoreRelaxed(0);
        quint32 elaps = timer.restart();
        std::clog << "processed " << proc << " in " << elaps << " ms (" << proc / (elaps / 1000) << " rps)"<< std::endl;
    }

    for (size_t i=0; i<THREAD_COUNT; i++)
        pthread_join(id[i], nullptr);

    return 0;
}
