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
            << TableField("order_id", TableField::Integer).primaryKey(true)
            << TableField("pair_id", TableField::Integer).references("pairs", {"pair_id"})
            << TableField("owner_id", TableField::Integer).references("owners", {"owner_id"})
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
            << TableField("trade_id", TableField::Integer).primaryKey(true)
            << TableField("order_id", TableField::Integer).references("orders", {"order_id"})
            << TableField("owner_id", TableField::Integer).references("owners", {"owner_id"})
            << TableField("amount", TableField::Decimal, 14, 6).notNull()
            << TableField("created", TableField::Datetime).notNull()
            << "FOREIGN KEY(order_id) REFERENCES orders(order_id)"
            << "FOREIGN KEY(owner_id) REFERENCES owners(owner_id)"
               ;

    createSqls["ticker"]
            << TableField("pair_id", TableField::Integer).references("pairs", {"pair_id"})
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
        orderParams[":owner_id"] = qrand() % 10 + 1;
        orderParams[":type"] = isAsks?"asks":"bids";
        orderParams[":rate"] = QString::number(rate, 'f', p.decimal_places);
        orderParams[":start_amount"] = ownersPropotions[i];
        orderParams[":amount"] = orderParams[":start_amount"];
        orderParams[":status"] = "active";
        orderParams[":created"] = QDateTime::currentDateTime();

        performSql("insert order", ordersInsertQuery, orderParams, true);
    }
}

void populateTablesFromBtce(QSqlDatabase& db)
{
    QSqlQuery pairInsertQuery(db);
    QSqlQuery tickerInsertQuery(db);
    QSqlQuery ordersInsertQuery(db);
    QSqlQuery tradesInsertQuery(db);
    pairInsertQuery.prepare("insert into pairs (pair, decimal_places, min_price, max_price, min_amount, hidden, fee) values (:pair, :decimal_places, :min_price, :max_price, :min_amount, :hidden, :fee)");
    tickerInsertQuery.prepare("insert into ticker (pair_id, high, low, avg, vol, vol_cur, buy, sell, last, updated) values (:pair_id, :high, :low, :avg, :vol, :vol_cur, :buy, :sell, :last, :updated)");
    ordersInsertQuery.prepare("insert into orders (pair_id, owner_id,   type,   rate, start_amount, amount,   status, created)"
                                      "values (:pair_id, :owner_id,   :type,   :rate, :start_amount, :amount,   :status, :created)");
    tradesInsertQuery.prepare("insert into trades (trade_id, order_id, owner_id, amount, created) values (:trade_id, :order_id, :owner_id, :amount, :created)");

    QVariantMap pairParams;
    QVariantMap tickerParams;
    QVariantMap orderParams;

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
    }
}

void populateDatabase(QSqlDatabase& db)
{
    BtcPublicApi::Info btceInfo;
    BtcPublicApi::Ticker btceTicker;
    BtcPublicApi::Depth btceDepth(2000);

    btceInfo.performQuery();
    btceTicker.performQuery();
    btceDepth.performQuery();

    QSqlQuery query(db);

    bool inTransaction;
    inTransaction = db.transaction();
    if (!inTransaction)
    {
        std::clog << "fail to start transaction -- expect very slow inserts" << std::endl;
        if (!db.driver()->hasFeature(QSqlDriver::DriverFeature::Transactions))
            std::clog << "Transactions are not supported by sql driver" << std::endl;
        performSql("manual transaction start", query, "START TRANSACTION");
    }
    populateTablesFromCsv(db);
    populateTablesFromBtce(db);
    if (inTransaction)
        db.commit();
    else
        performSql("manual transaction commit", query, "COMMIT");
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString iniFilePath = QCoreApplication::applicationDirPath() + "/../data/emul.ini";
    if (!QFileInfo(iniFilePath).exists())
    {
        std::cerr << "*** No INI file!" << std::endl;
        return 0;
    }
    QSettings settings(iniFilePath, QSettings::IniFormat);

    QSqlDatabase db;
    connectDatabase(db, settings);
    prepareDatabase(db);
    populateDatabase(db);

    QSqlQuery query(db);

    {
        std::vector<std::unique_ptr<QObject>> tests;
        tests.emplace(tests.end(), new QueryParserTest);
        tests.emplace(tests.end(), new ResponceTest(db));
        tests.emplace(tests.end(), new InfoTest(db));
        tests.emplace(tests.end(), new  TickerTest(db));
        tests.emplace(tests.end(), new  DepthTest(db));
        for(std::unique_ptr<QObject>& ptr: tests)
        {
            int testReturnCode = QTest::qExec(ptr.get(), argc, argv);
#ifdef EXIT_ON_FAIL
            if (testReturnCode)
                return testReturnCode;
#else
            Q_UNUSED(testReturnCode)
#endif
        }
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

    FCGI_Request request(sock);
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
        var = getResponce(db, httpQuery, method);
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
