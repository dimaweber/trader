#ifndef UNIT_TESTS_H
#define UNIT_TESTS_H

#include "fcgi_request.h"
#include "responce.h"
#include "query_parser.h"
#include "utils.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtTest>

#define VALID_KEY "3WTYG4ZB-I32T5T2V-S7UOKP61-9H8AMAAZ-R5H0BIVL"
#define VALID_SECRET "rarjtfm024i4axtfxqufhwjiqc2d9zle0m0yf6tbvzyzb87a2rnpvy3bvawlcrlo"

class FCGI_RequestTest : public QObject
{
    Q_OBJECT
private slots:
    void tst_GetQuery()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd?param1=value1&param2=value2";

        FCGI_Request request(url , headers, in);
        QCOMPARE(request.getParam("REQUEST_METHOD"), QString("GET"));
        QCOMPARE(request.getParam("REQUEST_SCHEME"), QString("http"));
        QCOMPARE(request.getParam("SERVER_ADDR"), QString("localhost"));
        QCOMPARE(request.getParam("SERVER_PORT"), QString("81"));
        QCOMPARE(request.getParam("DOCUMENT_URI"), QString("/api/3/ticker/btc_usd"));
        QCOMPARE(request.getParam("QUERY_STRING"), QString("param1=value1&param2=value2"));
        QCOMPARE(request.postData().length(), 0);
    }
    void tst_PostQuery()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        headers["KEY"] = VALID_KEY;
        headers["SIGN"] = VALID_SECRET;
        url = "http://localhost:81/api/3/ticker/btc_usd?param1=value1&param2=value2";
        in = "method=method&param=value";

        FCGI_Request request(url , headers, in);
        QCOMPARE(request.getParam("REQUEST_METHOD"), QString("POST"));
        QCOMPARE(request.getParam("REQUEST_SCHEME"), QString("http"));
        QCOMPARE(request.getParam("SERVER_ADDR"), QString("localhost"));
        QCOMPARE(request.getParam("SERVER_PORT"), QString("81"));
        QCOMPARE(request.getParam("DOCUMENT_URI"), QString("/api/3/ticker/btc_usd"));
        QCOMPARE(request.authHeaders()["Key"], QString(VALID_KEY));
        QCOMPARE(request.authHeaders()["Sign"], QString(VALID_SECRET));
        QCOMPARE(request.postData().length(), in.length());
    }
};

class QueryParserTest : public QObject
{
    Q_OBJECT
private slots:
    void tst_stringParse()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd?";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.method(), QString("ticker"));
        QVERIFY(parser.pairs().length() == 1);
    }

    void tst_method()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd-btc_usd?ignore_invalid=0";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVERIFY (parser.method() == "ticker");
    }

    void tst_pairs_1()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd-btc_usd?ignore_invalid=0";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.pairs().length(), 2);
        QVERIFY(parser.pairs().contains("btc_usd"));
        QVERIFY(!parser.pairs().contains("usd-btc"));
    }

    void tst_pairs_2()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd-btc_eur?ignore_invalid=0";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.pairs().length(), 2);
        QVERIFY(parser.pairs().contains("btc_usd"));
        QVERIFY(parser.pairs().contains("btc_eur"));
    }

    void tst_limit()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/tapi/method?param1=value1&limit=10";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.limit(), 10);
    }
    void tst_defaultLimit()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/tapi/method?param1=value1";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.limit(), 150);
    }

    void tst_limitOverflow()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/tapi/method?param1=value1&limit=9000";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.limit(), 5000);
    }

};

class TickerTest : public QObject
{
    Q_OBJECT
private slots:
    void tst_emptyList()
    {
        // In:    https://btc-e.com/api/3/ticker
        // Out:   {"success":0, "error":"Empty pair list"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker?ignore_invalid=1";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-btc_eur";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-non_ext?ignore_invalid=1";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-non_ext?ignore_invalid";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-non_ext?ignore_invalid=0";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-non_ext";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce( parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: non_ext");
    }

    void tst_InvalidPairPrevailOnDoublePair()
    {
        // In:   https://btc-e.com/api/3/ticker/non_ext-non_ext
        // Out:  {"success":0, "error":"Invalid pair name: non_ext"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/non_ext-non_ext";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce( parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: non_ext");
    }

    void tst_invalidPair_4()
    {
        // In:   https://btc-e.com/api/3/ticker/usd_btc
        // Out:  {"success":0, "error":"Invalid pair name: usd_btc"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "https://btc-e.com/api/3/ticker/usd_btc";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce( parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: usd_btc");
    }

    void tst_duplicatePair_1()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-btc_usd
        // Out:  {"success":0, "error":"Duplicated pair name: btc_usd"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-btc_usd";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce( parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Duplicated pair name: btc_usd");
    }

    void tst_duplicatePair_2()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-btc_usd?ignore_invalid=1
        // Out:  {"success":0, "error":"Duplicated pair name: btc_usd"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-btc_usd?ignore_invalid=1";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Duplicated pair name: btc_usd");
    }
};

class InfoTest : public QObject
{
    Q_OBJECT
private slots:
    void tst_serverTime()
    {
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/info";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("server_time"));
        QVERIFY(QDateTime::fromTime_t(responce["server_time"].toInt()).date() == QDate::currentDate());
    }
    void tst_Pairs()
    {
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/info";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce( parser, method);

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
private slots:
    void tst_invalidMethod()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/invalid";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce( parser, method);

        QCOMPARE(method, Method::Invalid);
        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("Invalid method"));
    }
    void tst_infoMethod()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/info";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce( parser, method);

        QCOMPARE(method, Method::PublicInfo);
    }
    void tst_tickerMethod()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/ticker";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PublicTicker);
    }
    void tst_depthMethod()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/depth";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PublicDepth);
    }
    void tst_tradesMethod()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/trades";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PublicTrades);
    }
    void tst_authNoKey()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = "method=getInfo&nonce=1";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("api key not specified"));
    }

    void tst_authInvalidKey()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        headers["KEY"] = "KKKK-EEEE-YYYY";
        url = "http://loclahost:81/tapi";
        in = "method=getInfo&nonce=1";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("invalid api key"));
    }

    void tst_NoSign()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        headers["KEY"] = VALID_KEY;
        url = "http://loclahost:81/tapi";
        in = "method=getInfo&nonce=1";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("invalid sign"));
    }

    void tst_InvalidSign()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        headers["KEY"] = VALID_KEY;
        headers["SIGN"] = "SSSiiiGGGnnn";
        url = "http://loclahost:81/tapi";
        in = "method=getInfo&nonce=1";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("invalid sign"));
    }

    void tst_noNonce()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = "method=getInfo";
        headers["KEY"] = VALID_KEY;
        headers["SIGN"] = hmac_sha512(in, VALID_SECRET).toHex();

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QRegExp rx("invalid nonce parameter; on key:[0-9]*, you sent:'', you should send:[0-9]*");
        int pos = rx.indexIn(responce["error"].toString());
        QCOMPARE(pos, 0);
    }

    void tst_invalidNonce()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = "method=getInfo&nonce=1";
        headers["KEY"] = VALID_KEY;
        headers["SIGN"] = hmac_sha512(in, VALID_SECRET).toHex();

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QRegExp rx("invalid nonce parameter; on key:[0-9]*, you sent:'[0-9]+', you should send:[0-9]*");
        int pos = rx.indexIn(responce["error"].toString());
        QCOMPARE(pos, 0);
    }

    void tst_privateGetInfoMethod()
    {
        QTest::qSleep(1000); // sleep for 1 sec for nonce update
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=getInfo&nonce=%1").arg(QDateTime::currentDateTime().toTime_t()).toUtf8();
        headers["KEY"] = VALID_KEY;
        headers["SIGN"] = hmac_sha512(in, VALID_SECRET).toHex();

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PrivateGetInfo);
    }

    void tst_privateActiveOrdersMethod()
    {
        QTest::qSleep(1000); // sleep for 1 sec for nonce update
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=ActiveOrders&nonce=%1").arg(QDateTime::currentDateTime().toTime_t()).toUtf8();
        headers["KEY"] = VALID_KEY;
        headers["SIGN"] = hmac_sha512(in, VALID_SECRET).toHex();

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PrivateActiveOrders);
    }
};


class DepthTest : public QObject
{
    Q_OBJECT
private slots:
    void tst_emptyList()
    {
        // In:   https://btc-e.com/api/3/depth
        // Out:  {"success":0, "error":"Empty pair list"}
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/depth";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

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

        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/depth/btc_usd";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/depth/btc_usd";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/depth/btc_usd";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/depth/btc_usd?limit=10";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

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
private slots:
    void tst_emptyList()
    {
        // In:   https://btc-e.com/api/3/trades
        // Out:  {"success":0, "error":"Empty pair list"}
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/trades";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/trades/btc_usd";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

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
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/trades/btc_usd?limit=10";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVariantList btc_usd = responce["btc_usd"].toList();
        QVERIFY(btc_usd.size() <= 10);
    }
    void tst_validTimestampSorted()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/trades/btc_usd";

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

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
private slots:
    void init()
    {
        QTest::qSleep(1000); // sleep for 1 sec for nonce update
    }

    void tst_no()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=getInfo&nonce=%1").arg(QDateTime::currentDateTime().toTime_t()).toUtf8();
        headers["KEY"] = VALID_KEY;
        headers["SIGN"] = hmac_sha512(in, VALID_SECRET).toHex();

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 1);
        QVERIFY(responce.contains("result") && responce["result"].canConvert(QVariant::Map));
        QVariantMap result = responce["result"].toMap();
        QVERIFY(result.contains("funds"));
        QVERIFY(result.contains("rights"));
        QVERIFY(result.contains("open_orders"));
        QVERIFY(result.contains("server_time"));
    }
};

class PrivateActiveOrdersTest:public QObject
{
    Q_OBJECT
private slots:
    void init()
    {
        QTest::qSleep(1000); // sleep for 1 sec for nonce update
    }

    void tst_no()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=ActiveOrders&nonce=%1").arg(QDateTime::currentDateTime().toTime_t()).toUtf8();
        headers["KEY"] = VALID_KEY;
        headers["SIGN"] = hmac_sha512(in, VALID_SECRET).toHex();

        FCGI_Request request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 1);
        QVERIFY(responce.contains("result") && responce["result"].canConvert(QVariant::Map));
        QVariantMap result = responce["result"].toMap();
        QVERIFY(result.size() > 0);
        QVERIFY(result.first().canConvert(QVariant::Map));
        QVariantMap order = result.first().toMap();
        QVERIFY(order.contains("amount") && order["amount"].canConvert(QVariant::Double) && order["amount"].toDouble() > 0);
        QVERIFY(order.contains("status") && order["status"].canConvert(QVariant::Int) && order["status"].toInt() == 0);
    }
};

#endif // UNIT_TESTS_H
