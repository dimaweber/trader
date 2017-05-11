#ifndef UNIT_TESTS_H
#define UNIT_TESTS_H

#include "responce.h"
#include "query_parser.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest>

class QueryParserTest : public QObject
{
    Q_OBJECT
private slots:
    void tst_stringParse()
    {
        QueryParser parser("http://localhost:81/api/3/ticker/btc_usd?");
        QCOMPARE(parser.method(), QString("ticker"));
        QVERIFY(parser.pairs().length() == 1);
    }

    void tst_method()
    {
        QueryParser parser("http", "localhost", "81", "/api/3/ticker/btc_usd-btc_usd", "ignore_invalid=0");
        QVERIFY (parser.method() == "ticker");
    }

    void tst_pairs_1()
    {
        QueryParser parser("http", "localhost", "81", "/api/3/ticker/btc_usd-btc_usd", "ignore_invalid=0");
        QCOMPARE(parser.pairs().length(), 2);
        QVERIFY(parser.pairs().contains("btc_usd"));
        QVERIFY(!parser.pairs().contains("usd-btc"));
    }

    void tst_pairs_2()
    {
        QueryParser parser("http", "localhost", "81", "/api/3/ticker/btc_usd-btc_eur", "ignore_invalid=0");
        QCOMPARE(parser.pairs().length(), 2);
        QVERIFY(parser.pairs().contains("btc_usd"));
        QVERIFY(parser.pairs().contains("btc_eur"));
    }

    void tst_limit()
    {
        QueryParser parser_10("http://localhost:81/tapi/method?param1=value1&limit=10");
        QCOMPARE(parser_10.limit(), 10);

        QueryParser parser_default("http://localhost:81/tapi/method?param1=value1");
        QCOMPARE(parser_default.limit(), 150);

        QueryParser parser_9000("http://localhost:81/tapi/method?param1=value1&limit=9000");
        QCOMPARE(parser_9000.limit(), 5000);
    }
};

class TickerTest : public QObject
{
    Q_OBJECT
    QSqlDatabase& database;
public:
    TickerTest(QSqlDatabase& db):database(db)
    {}
private slots:
    void tst_emptyList()
    {
        // In:    https://btc-e.com/api/3/ticker
        // Out:   {"success":0, "error":"Empty pair list"}
        Method method;
        QVariantMap responce = getResponce(database, "http://localhost:81/api/3/ticker", method);
        QVERIFY(!responce.isEmpty());
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Empty pair list");
    }
    void tst_emptyList_withIgnore()
    {
        // In:   https://btc-e.com/api/3/ticker?ignore_invalid=1
        // Out:  {"success":0, "error":"Empty pair list"}
        Method method;
        QVariantMap responce = getResponce(database, "http://localhost:81/api/3/ticker?ignore_invalid=1", method);
        QVERIFY(!responce.isEmpty());
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Empty pair list");
    }
    void tst_valid_1()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-
        // Out:  VALID json
        Method method;
        QVariantMap responce = getResponce(database, "http://localhost/api/3/ticker/btc_usd-", method);
        QVERIFY(!responce.contains("success"));
        QVERIFY(responce.size() == 1);
        QVERIFY(responce.contains("btc_usd"));
        QVERIFY(responce["btc_usd"].toMap().contains("high"));
        QVERIFY(responce["btc_usd"].toMap().contains("low"));
        QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
    }
    void tst_valid_2()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd
        // Out:  VALID json
        Method method;
        QVariantMap responce = getResponce(database, "http://localhost/api/3/ticker/btc_usd", method);
        QVERIFY(!responce.contains("success"));
        QVERIFY(responce.size() == 1);
        QVERIFY(responce.contains("btc_usd"));
        QVERIFY(responce["btc_usd"].toMap().contains("high"));
        QVERIFY(responce["btc_usd"].toMap().contains("low"));
        QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
    }
    void tst_valid_3()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-btc_eur
        // Out:  VALID json
        Method method;
        QVariantMap responce = getResponce(database, "http://localhost/api/3/ticker/btc_usd-btc_eur", method);
        QVERIFY(!responce.contains("success"));
        QVERIFY(responce.size() == 2);
        QVERIFY(responce.contains("btc_usd"));
        QVERIFY(responce.contains("btc_eur"));
        QVERIFY(responce["btc_usd"].toMap().contains("high"));
        QVERIFY(responce["btc_usd"].toMap().contains("low"));
        QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
    }
    void tst_valid_4()
    {
        // In:   http://localhost:81/api/3/tick1er/btc_usd-non_ext?ignore_invalid=1
        // Out:  VALID json
        Method method;
        QVariantMap responce = getResponce(database, "http://localhost/api/3/ticker/btc_usd-non_ext?ignore_invalid=1", method);
        QVERIFY(!responce.contains("success"));
        QVERIFY(responce.size() == 1);
        QVERIFY(responce.contains("btc_usd"));
        QVERIFY(responce["btc_usd"].toMap().contains("high"));
        QVERIFY(responce["btc_usd"].toMap().contains("low"));
        QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
    }

    void tst_invalidPair_1()
    {
        // In:   http://localhost:81/api/3/tick1er/btc_usd-non_ext?ignore_invalid
        // Out:  {"success":0, "error":"Invalid pair name: non_ext"}
        Method method;
        QVariantMap responce = getResponce(database, "http://localhost/api/3/ticker/btc_usd-non_ext?ignore_invalid", method);
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: non_ext");
    }

    void tst_invalidPair_2()
    {
        // In:   http://localhost:81/api/3/tick1er/btc_usd-non_ext?ignore_invalid=0
        // Out:  {"success":0, "error":"Invalid pair name: non_ext"}
        Method method;
        QVariantMap responce = getResponce(database, "http://localhost/api/3/ticker/btc_usd-non_ext?ignore_invalid=0", method);
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: non_ext");
    }

    void tst_invalidPair_3()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-non_ext
        // Out:  {"success":0, "error":"Invalid pair name: non_ext"}
        Method method;
        QVariantMap responce = getResponce(database, "http://localhost/api/3/ticker/btc_usd-non_ext", method);
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: non_ext");
    }

    void tst_InvalidPairPrevailOnDoublePair()
    {
        // In:   https://btc-e.com/api/3/ticker/non_ext-non_ext
        // Out:  {"success":0, "error":"Invalid pair name: non_ext"}
        QueryParser parser("http://localhost/api/3/ticker/non_ext-non_ext");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: non_ext");
    }

    void tst_invalidPair_4()
    {
        // In:   https://btc-e.com/api/3/ticker/usd_btc
        // Out:  {"success":0, "error":"Invalid pair name: usd_btc"}
        QueryParser parser("http://localhost/api/3/ticker/usd_btc");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: usd_btc");
    }

    void tst_duplicatePair_1()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-btc_usd
        // Out:  {"success":0, "error":"Duplicated pair name: btc_usd"}
        QueryParser parser("http://localhost/api/3/ticker/btc_usd-btc_usd");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Duplicated pair name: btc_usd");
    }

    void tst_duplicatePair_2()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-btc_usd?ignore_invalid=1
        // Out:  {"success":0, "error":"Duplicated pair name: btc_usd"}
        QueryParser parser("http://localhost/api/3/ticker/btc_usd-btc_usd?ignore_invalid=1");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Duplicated pair name: btc_usd");
    }
};

class InfoTest : public QObject
{
    Q_OBJECT
    QSqlDatabase& db;
public:
    InfoTest(QSqlDatabase& database):db(database){}
private slots:
    void tst_serverTime()
    {
        QueryParser parser("http://loclahost:81/api/3/info");
        Method method;
        QVariantMap responce = getResponce(db, parser, method);
        QVERIFY(responce.contains("server_time"));
        QVERIFY(QDateTime::fromTime_t(responce["server_time"].toInt()).date() == QDate::currentDate());
    }
    void tst_Pairs()
    {
        QueryParser parser("http://loclahost:81/api/3/info");
        Method method;
        QVariantMap responce = getResponce(db, parser, method);
        QVERIFY(responce.contains("pairs"));
        QVERIFY(responce["pairs"].canConvert(QVariant::Map));
        QVariantMap pairs = responce["pairs"].toMap();
        QVERIFY(pairs.contains("btc_usd"));
        QVERIFY(pairs["btc_usd"].canConvert(QVariant::Map));
        QVariantMap btc_usd = pairs["btc_usd"].toMap();
        QVERIFY(btc_usd.contains("decimal_places"));
    }
};


class ResponceTest : public QObject
{
    Q_OBJECT
    QSqlDatabase& db;
public:
    ResponceTest(QSqlDatabase& database):db(database)
    {}
private slots:
    void tst_invalidMethod()
    {
        QueryParser parser("http://loclahost:81/api/3/invalid");
        Method method;
        QVariantMap responce = getResponce(db, parser, method);
        QCOMPARE(method, Method::Invalid);
        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("Invalid method"));
    }
    void tst_infoMethod()
    {
        QueryParser parser("http://localhost:81/api/3/info");
        Method method;
        QVariantMap responce = getResponce(db, parser, method);
        QCOMPARE(method, Method::PublicInfo);
    }
    void tst_tickerMethod()
    {
        QueryParser parser("http://localhost:81/api/3/ticker");
        Method method;
        QVariantMap responce = getResponce(db, parser, method);
        QCOMPARE(method, Method::PublicTicker);
    }
    void tst_depthMethod()
    {
        QueryParser parser("http://localhost:81/api/3/depth");
        Method method;
        QVariantMap responce = getResponce(db, parser, method);
        QCOMPARE(method, Method::PublicDepth);
    }
    void tst_tradesMethod()
    {
        QueryParser parser("http://localhost:81/api/3/trades");
        Method method;
        QVariantMap responce = getResponce(db, parser, method);
        QCOMPARE(method, Method::PublicTrades);
    }
    void tst_privateMethodsAuth()
    {
        QueryParser parser("http://localhost:81/tapi/getInfo");
        Method method;
        QVariantMap responce = getResponce(db, parser, method);
        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("api key not specified"));
    }

    void tst_privateGetInfoMethod()
    {
        QueryParser parser("http://localhost:81/tapi/getInfo");
        Method method;
        QVariantMap responce = getResponce(db, parser, method);
        QCOMPARE(method, Method::PrivateGetInfo);
    }

};


class DepthTest : public QObject
{
    Q_OBJECT
    QSqlDatabase& database;
public:
    DepthTest(QSqlDatabase& db):database(db)
    {}
private slots:
    void tst_emptyList()
    {
        // In:   https://btc-e.com/api/3/depth
        // Out:  {"success":0, "error":"Empty pair list"}
        QueryParser parser("http://localhost:81/api/3/depth");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVERIFY(!responce.isEmpty());
        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("Empty pair list"));
    }
    void tst_valid()
    {
        // In:   https://btc-e.com/api/3/depth/btc_usd
        // Out:  VALID json
        QueryParser parser("http://localhost:81/api/3/depth/btc_usd");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVERIFY(!responce.isEmpty());
        QVERIFY(!responce.contains("success"));
        QCOMPARE(responce.size(), 1);
        QVERIFY(responce.contains("btc_usd"));
        QVariantMap btc_usd = responce["btc_usd"].toMap();
        QVERIFY(btc_usd.contains("asks"));
        QVERIFY(btc_usd.contains("bids"));
    }
    void tst_validRateSorted()
    {
        QueryParser parser("http://localhost:81/api/3/depth/btc_usd");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVariantMap btc_usd = responce["btc_usd"].toMap();
        QVariantList asks = btc_usd["asks"].toList();
        QVariantList bids = btc_usd["bids"].toList();
        QVERIFY(asks.size() > 1 && bids.size() > 1);
        QVERIFY(asks[0].toList()[0].toFloat() < asks[1].toList()[0].toFloat());
        QVERIFY(bids[0].toList()[0].toFloat() > bids[1].toList()[0].toFloat());
        QVERIFY(bids[0].toList()[0].toFloat() < asks[0].toList()[0].toFloat());
    }
    void tst_validDecimalDigits()
    {
        QueryParser parser("http://localhost:81/api/3/depth/btc_usd");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVariantMap btc_usd = responce["btc_usd"].toMap();
        QVariantList asks = btc_usd["asks"].toList();
        QVariantList bids = btc_usd["bids"].toList();
        for (QVariant& v: asks)
        {
            QString sRate = v.toList()[0].toString();
            int dotPosition = sRate.indexOf('.');
            if (dotPosition > -1)
                QVERIFY(sRate.length() - dotPosition <= 4);
        }
    }
    void tst_limit()
    {
        QueryParser parser("http://localhost:81/api/3/depth/btc_usd?limit=10");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVariantMap btc_usd = responce["btc_usd"].toMap();
        QVariantList asks = btc_usd["asks"].toList();
        QVariantList bids = btc_usd["bids"].toList();
        QVERIFY(asks.size() <= 10);
        QVERIFY(bids.size() <= 10);
    }
};

class TradesTest : public QObject
{
    Q_OBJECT
    QSqlDatabase& database;
public:
    TradesTest(QSqlDatabase& db):database(db)
    {}
private slots:
    void tst_emptyList()
    {
        // In:   https://btc-e.com/api/3/trades
        // Out:  {"success":0, "error":"Empty pair list"}
        QueryParser parser("http://localhost:81/api/3/trades");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVERIFY(!responce.isEmpty());
        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("Empty pair list"));
    }
    void tst_valid()
    {
        // In:   https://btc-e.com/api/3/trades/btc_usd
        // Out:  VALID json
        QueryParser parser("http://localhost:81/api/3/trades/btc_usd");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVERIFY(!responce.isEmpty());
        QVERIFY(!responce.contains("success"));
        QCOMPARE(responce.size(), 1);
        QVERIFY(responce.contains("btc_usd"));
        QVERIFY(responce["btc_usd"].canConvert(QVariant::List));
        QVariantList btc_usd = responce["btc_usd"].toList();
        QVERIFY(btc_usd.size() > 0);
        for (const QVariant& v: btc_usd)
        {
            QVERIFY(v.canConvert(QVariant::Map));
            QVariantMap trade = v.toMap();
            QVERIFY(trade.contains("type") && (trade["type"].toString() == "bid" || trade["type"].toString() == "ask"));
            QVERIFY(trade.contains("price") && trade["price"].toFloat() > 0 );
            QVERIFY(trade.contains("amount") && trade["amount"].toFloat() > 0 );
            QVERIFY(trade.contains("tid") && trade["tid"].toUInt() > 0 );
            QVERIFY(trade.contains("timestamp") && trade["timestamp"].toUInt() > 0 );
        }
    }
    void tst_limit()
    {
        QueryParser parser("http://localhost:81/api/3/trades/btc_usd?limit=10");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVariantList btc_usd = responce["btc_usd"].toList();
        QVERIFY(btc_usd.size() <= 10);
    }
    void tst_validTimestampSorted()
    {
        QueryParser parser("http://localhost:81/api/3/trades/btc_usd");
        Method method;
        QVariantMap responce = getResponce(database, parser, method);
        QVariantList btc_usd = responce["btc_usd"].toList();
        for (int i=0; i<btc_usd.size()-1;i++)
        {
            QVariantMap a = btc_usd[i].toMap();
            QVariantMap b = btc_usd[i+1].toMap();
            QVERIFY ( a["tid"].toUInt() > b["tid"].toUInt());
        }
    }
};

class PrivateGetInfoTest:public QObject
{
    Q_OBJECT
    QSqlDatabase& database;
public:
    PrivateGetInfoTest(QSqlDatabase& db):database(db){}
private slots:
    void tst_noAuth()
    {

    }
};

#endif // UNIT_TESTS_H
