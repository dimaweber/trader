#include "utils.h"
#include "fcgi_request.h"
#include "query_parser.h"
#include "unit_tests.h"

#include <iostream>
#include <unistd.h>
#include <memory>

#include <QDateTime>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest>

int main(int argc, char *argv[])
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("btce_emul.db");
    db.open();

    QSqlQuery query(db);
    query.exec("CREATE TABLE pairs (id int primary key, pair char(7), decimal_places int, min_price decimal, max_price decimal, min_amount decimal, hidden boolean, fee decimal)");
    query.exec("insert into pairs values (0, 'btc_usd', 3, .01, 10000, .01, 0, .2)");
    query.exec("insert into pairs values (1, 'btc_eur', 3, .01, 10000, .01, 0, .2)");
    query.exec("insert into pairs values (2, 'btc_rur', 3, .01, 100000, .01, 0, .2)");

    query.exec("CREATE TABLE ticker (pair char(7) unique, high decimal, low decimal, avg decimal, vol decimal, vol_cur decimal, last decimal, buy decimal, sell decimal, updated int)");
    query.exec("insert into ticker values ('btc_usd',109.88, 91.14, 100.51, 1632898.2249, 16541.51969, 101.773, 101.9, 101.773, cast(strftime ('%s', 'now') AS INT))");
    query.exec("insert into ticker values ('btc_eur',109.88, 91.14, 100.51, 1632898.2249, 16541.51969, 101.773, 101.9, 101.773, cast(strftime ('%s', 'now') AS INT))");
    query.exec("insert into ticker values ('btc_rur',109.88, 91.14, 100.51, 1632898.2249, 16541.51969, 101.773, 101.9, 101.773, cast(strftime ('%s', 'now') AS INT))");

    query.exec("CREATE TABLE active_orders (pair char(7), type char(4), rate decimal, amount decimal, start_amount decimal, status int, order_id INTEGER primary key, owner_id INTEGER)");
    query.exec("insert into active_orders values('btc_usd', 'asks', 100.00, 0.0, 0.10, 0, 1, 1),"
                                               "('btc_usd', 'asks', 101.02, 0.0, 0.34, 0, 2, 1),"
                                               "('btc_usd', 'bids',  99.80, 0.4, 0.67, 0, 3, 2)");

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
            if (testReturnCode)
                return testReturnCode;
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
        var = getResponce(db, httpQuery, method);
        QJsonDocument doc = QJsonDocument::fromVariant(var);
        json = doc.toJson().constData();

        request.put ( "Content-type: application/json\r\n\r\n");
        request.put ( json);

        request.finish();
        std::clog << "[FastCGI] Request finished" << std::endl;
    }

    return 0;
}
