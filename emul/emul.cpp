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

    createSqls["owners"]
        << TableField("owner_id").primaryKey(true)
        << TableField("name", TableField::Char, 128)
           << "CONSTRAINT uniq_name UNIQUE (name)"
        ;

    createSqls["currencies"]
            << TableField("currency_id").primaryKey(true)
            << TableField("currency", TableField::Char, 3)
            << "CONSTRAINT uniq_currency UNIQUE (currency)"
               ;

    createSqls["deposits"]
            << TableField("owner_id").references("owners", {"owner_id"})
            << TableField("currency_id").references("currencies", {"currency_id"})
            << TableField("volume", TableField::Decimal, 14, 6).notNull().check("amount >= 0")
            << "FOREIGN KEY(currency_id) REFERENCES currencies(currency_id)"
            << "FOREIGN KEY(owner_id) REFERENCES owners(owner_id)"
            << "CONSTRAINT uniq_ownercurrency UNIQUE (owner_id,currency_id)"
               ;

    createSqls["apikeys"]
            << TableField("owner_id").references("owners", {"owner_id"})
            << TableField("apikey", TableField::Char, 128).notNull()
            << TableField("secret", TableField::Char, 128).notNull()
            << TableField("info", TableField::Boolean).notNull().defaultValue("FALSE")
            << TableField("trade", TableField::Boolean).notNull().defaultValue("FALSE")
            << TableField("widthdraw", TableField::Boolean).notNull().defaultValue("FALSE")
            << TableField("nonce", TableField::Integer).notNull().defaultValue(1).check("nonce > 0")
            << "FOREIGN KEY(owner_id) REFERENCES owners(owner_id)"
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
            << TableField("owner_id").references("owners", {"owner_id"})
            << "type enum ('asks', 'bids') not null"
            << TableField("rate", TableField::Decimal, 14, 6).notNull().check("rate > 0")
            << TableField("start_amount", TableField::Decimal, 14, 6).notNull().check("start_amount > 0")
            << TableField("amount", TableField::Decimal, 14, 6).notNull().check("amount >= 0 and amount <= start_amount")
            << "status enum ('done', 'active', 'cancelled', 'part_done') not null"
            << TableField("created", TableField::Datetime).notNull()
            << "FOREIGN KEY(pair_id) REFERENCES pairs(pair_id)"
            << "FOREIGN KEY(owner_id) REFERENCES owners(owner_id)"
               ;

    createSqls["trades"]
            << TableField("trade_id").primaryKey(true)
            << TableField("order_id").references("orders", {"order_id"})
            << TableField("owner_id").references("owners", {"owner_id"})
            << TableField("amount", TableField::Decimal, 14, 6).notNull()
            << TableField("created", TableField::Datetime).notNull()
            << "FOREIGN KEY(order_id) REFERENCES orders(order_id)"
            << "FOREIGN KEY(owner_id) REFERENCES owners(owner_id)"
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
    int len = QString("****************************************************************").length();
    return randomString(len).toLower();
}

static QVector<quint32> ownerIdCache;
static QVector<quint32> currencyIdCache;

quint32 get_random_owner_id(quint32 except = 0)
{
    quint32 id;
    do
    {
        int rndIndex = qrand() % ownerIdCache.size();
        id = ownerIdCache[rndIndex];
    } while (id == except);
    return id;
}

void buildOrdersFromDepth(const BtcObjects::Depth::Position& position, const BtcObjects::Pair& p, QSqlQuery& ordersInsertQuery, bool isAsks, QVariantMap& orderParams)
{
    float amount = position.amount;
    float rate = position.rate;

    int ownersCount = 0;
    QMap<int, float> ownersPropotions;
    if (amount < p.min_amount * 10)
    {
        // force only one owner for tiny amounts
        ownersCount = 1;
    }
    else
    {
        int dice = qrand() % 10;
        if (dice < 5) ownersCount = 1;
        else if (dice < 7) ownersCount = 2;
        else if (dice < 9) ownersCount = 3;
        else ownersCount = 4;
    }
    if (ownersCount == 1)
        ownersPropotions[0] = amount;
    else
    {
        float rest = amount;
        // first n-1 owners take thei parts
        for(int i=0; i<ownersCount-1; i++)
        {
            float part = (qrand() % 1000) / 1000.0;
            ownersPropotions[i] = rest * part;
            rest = rest * (1-part);
        }
        // last one take whole rest
        ownersPropotions[ownersCount-1] = rest;
    }

    for (int i=0; i < ownersCount; i++)
    {
        orderParams[":owner_id"] = get_random_owner_id();
        orderParams[":type"] = isAsks?"asks":"bids";
        orderParams[":rate"] = QString::number(rate, 'f', p.decimal_places);
        orderParams[":start_amount"] = ownersPropotions[i];
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
    quint32 orderOwner_id = 0;

    selectOrdersWithGivenPairRate.bindValue(":rate", trade.price);
    if (selectOrdersWithGivenPairRate.exec())
    {
        if (selectOrdersWithGivenPairRate.next())
        {
            order_id = selectOrdersWithGivenPairRate.value(0);
            orderOwner_id = selectOrdersWithGivenPairRate.value(1).toUInt();
            updateOrderIncreaseStartAmount.bindValue(":order_id", order_id);
            updateOrderIncreaseStartAmount.bindValue(":increase", trade.amount);
            updateOrderIncreaseStartAmount.bindValue(":created", trade.timestamp.addSecs(-qrand() % 65534 + 1));
            updateOrderIncreaseStartAmount.exec();
        }
    }

    if (!order_id.isValid())
    {
        orderOwner_id = get_random_owner_id();
        orderParams[":owner_id"] = orderOwner_id;
        orderParams[":type"] = (trade.type == BtcObjects::Trade::Type::Bid)?"asks":"bids";
        orderParams[":rate"] = QString::number(trade.price, 'f', 3);
        orderParams[":start_amount"] = trade.amount;
        orderParams[":amount"] = 0;
        orderParams[":status"] = "done";
        orderParams[":created"] = trade.timestamp.addSecs(-qrand() % 65534 + 1);

        performSql("insert done order", ordersInsertQuery, orderParams, true);

        order_id = ordersInsertQuery.lastInsertId();
    }

    tradesParams[":trade_id"] = trade.id;
    tradesParams[":owner_id"] = get_random_owner_id(orderOwner_id);
    tradesParams[":order_id"] = order_id;
    tradesParams[":amount"] = trade.amount;
    tradesParams[":created"] = trade.timestamp;

    performSql("insert trade", tradesInsertQuery, tradesParams, true);
}

void populateTablesFromBtce(QSqlDatabase& db)
{
    QSqlQuery pairInsertQuery(db);
    QSqlQuery tickerInsertQuery(db);
    QSqlQuery ordersInsertQuery(db);
    QSqlQuery tradesInsertQuery(db);
    QSqlQuery selectOrdersWithGivenPairRate(db);
    QSqlQuery updateOrderIncreaseStartAmount(db);
    QSqlQuery insertCurrencyQuery(db);

    pairInsertQuery.prepare("insert into pairs (pair, decimal_places, min_price, max_price, min_amount, hidden, fee) values (:pair, :decimal_places, :min_price, :max_price, :min_amount, :hidden, :fee)");
    tickerInsertQuery.prepare("insert into ticker (pair_id, high, low, avg, vol, vol_cur, buy, sell, last, updated) values (:pair_id, :high, :low, :avg, :vol, :vol_cur, :buy, :sell, :last, :updated)");
    ordersInsertQuery.prepare("insert into orders (pair_id, owner_id,   type,   rate, start_amount, amount,   status, created)"
                                      "values (:pair_id, :owner_id,   :type,   :rate, :start_amount, :amount,   :status, :created)");
    tradesInsertQuery.prepare("insert into trades (trade_id, order_id, owner_id, amount, created) values (:trade_id, :order_id, :owner_id, :amount, :created)");
    selectOrdersWithGivenPairRate.prepare("select order_id, owner_id, created from orders o where pair_id=:pair_id and o.rate=:rate and type= order by created asc");
    updateOrderIncreaseStartAmount.prepare("update orders set start_amount = start_amount + :increase, created = :created where order_id=:order_id");
    insertCurrencyQuery.prepare("insert into currencies (currency) values (:currency)");

    QVariantMap pairParams;
    QVariantMap tickerParams;
    QVariantMap orderParams;
    QStringList currencies;

    for (const QString& pair: BtcObjects::Pairs::ref().keys())
    {
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
        QString currency;
        currency = pair.left(3);
        QVariantMap currencyParams;
        if (!currencies.contains(currency))
        {
            currencyParams[":currency"] = currency;
            performSql("insert currency", insertCurrencyQuery, currencyParams, true);
            currencyIdCache.append( insertCurrencyQuery.lastInsertId().toUInt());
            currencies.append(currency);
        }
        currency = pair.right(3);
        if (!currencies.contains(currency))
        {
            currencyParams[":currency"] = currency;
            performSql("insert currency", insertCurrencyQuery, currencyParams, true);
            currencyIdCache.append( insertCurrencyQuery.lastInsertId().toUInt());
            currencies.append(currency);
        }

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

        orderParams[":pair_id"] = pair_id;
        for(const BtcObjects::Depth::Position& position: p.depth.asks)
            buildOrdersFromDepth(position, p, ordersInsertQuery, true, orderParams);
        for(const BtcObjects::Depth::Position& position: p.depth.bids)
            buildOrdersFromDepth(position, p, ordersInsertQuery, false, orderParams);

        selectOrdersWithGivenPairRate.bindValue(":pair_id", pair_id);
        for (const BtcObjects::Trade& trade: p.trades)
        {
            buildTradesFromBtce(trade, tradesInsertQuery, selectOrdersWithGivenPairRate, updateOrderIncreaseStartAmount, ordersInsertQuery, orderParams);
        }
    }
}

void createSecrets(QSqlDatabase& db)
{
    QSqlQuery insertSecret(db);
    insertSecret.prepare("insert into apikeys (owner_id,apikey,secret,info,trade) values (:owner_id, :apikey, :secret, :info, :trade)");
    QVariantMap apikeyParams;
    for(quint32 owner_id: ownerIdCache)
    {
        apikeyParams[":owner_id"] = owner_id;
        apikeyParams[":apikey"] = randomApiKey();
        apikeyParams[":secret"] = randomSecret();
        apikeyParams[":info"] = true;
        apikeyParams[":trade"] = true;

        performSql("insert apikey", insertSecret, apikeyParams, true) ;

        apikeyParams[":owner_id"] = owner_id;
        apikeyParams[":apikey"] = randomApiKey();
        apikeyParams[":secret"] = randomSecret();
        apikeyParams[":info"] = false;
        apikeyParams[":trade"] = true;

        performSql("insert apikey", insertSecret, apikeyParams, true) ;

        apikeyParams[":owner_id"] = owner_id;
        apikeyParams[":apikey"] = randomApiKey();
        apikeyParams[":secret"] = randomSecret();
        apikeyParams[":info"] = true;
        apikeyParams[":trade"] = false;

        performSql("insert apikey", insertSecret, apikeyParams, true) ;

        apikeyParams[":owner_id"] = owner_id;
        apikeyParams[":apikey"] = randomApiKey();
        apikeyParams[":secret"] = randomSecret();
        apikeyParams[":info"] = false;
        apikeyParams[":trade"] = false;

        performSql("insert apikey", insertSecret, apikeyParams, true) ;

    }
}

void createDeposits(QSqlDatabase& db)
{
    QSqlQuery insertDepositQuery(db);
    insertDepositQuery.prepare("insert into deposits (owner_id, currency_id, volume) values (:owner_id, :currency_id, :volume)");
    QVariantMap depositsParams;
    for(quint32 owner_id : ownerIdCache)
    {
        depositsParams[":owner_id"] = owner_id;
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
    }
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

    QSqlQuery query(db);

    bool inTransaction;
    inTransaction = db.transaction();
    if (!inTransaction)
    {
        std::clog << "fail to start transaction -- expect very slow inserts" << std::endl;
        if (!db.driver()->hasFeature(QSqlDriver::DriverFeature::Transactions))
            std::clog << "Transactions are not supported by sql driver" << std::endl;
        performSql("manual transaction start", query, "START TRANSACTION", true);
    }
    populateTablesFromCsv(db);

    performSql("fill ownersIdDache", query, "select owner_id from owners", true);
    while (query.next())
        ownerIdCache.append( query.value(0).toUInt());

    populateTablesFromBtce(db);
    createSecrets(db);
    createDeposits(db);

    if (inTransaction)
        db.commit();
    else
        performSql("manual transaction commit", query, "COMMIT", true);
}

int main(int argc, char *argv[])
{
    bool recreateDatabase = false;
    bool runTests = false;
    bool failTestExit = true;
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
    depth_limit = settings.value("btce/depth_limit", 150).toInt();
    trades_limit = settings.value("btce/trades_limit", 150).toInt();

    QSqlDatabase db;
    connectDatabase(db, settings);
    if (recreateDatabase)
    {
        prepareDatabase(db);
        populateDatabase(db, trades_limit, depth_limit); //TODO: run this in thread
    }

    if (!initialiazeResponce(db))
    {
        std::cerr << "fail to initialaze responce subsystem. stop" << std::endl;
        return 3;
    }

    if (runTests)
    {
        BtceEmulator_Test test;
        int testReturnCode = QTest::qExec(&test, argc, argv);

        if (failTestExit && testReturnCode)
            return testReturnCode;
    }

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

    FcgiRequest request(sock);
    std::clog << "[FastCGI] Request initialized, ready to work" << std::endl;

    while (request.accept() >= 0)
    {
        std::clog << "[FastCGI] New request accepted. Processing" << std::endl;

        QueryParser httpQuery(request);

        QString json;
        QVariantMap var;
        Method method;
        QElapsedTimer timer;
        timer.start();
        var = getResponce(httpQuery, method);
        QJsonDocument doc = QJsonDocument::fromVariant(var);
        json = doc.toJson().constData();
        quint32 elapsed = timer.elapsed();

        request.put ( "Content-type: application/json\r\n");
        request.put ( "XXX-Emulator: true\r\n");
        request.put ( QString("XXX-Emulator-DbTime: %1\r\n").arg(elapsed));
        request.put ("\r\n");
        request.put ( json);

        request.finish();
        std::clog << "[FastCGI] Request finished" << std::endl;
    }

    return 0;
}
