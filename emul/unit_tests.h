#ifndef UNIT_TESTS_H
#define UNIT_TESTS_H

#include "fcgi_request.h"
#include "responce.h"
#include "query_parser.h"
#include "utils.h"

#include <QtTest>

class BtceEmulator_Test : public QObject
{
    Q_OBJECT
    quint32 nonce()
    {
        static quint32 value = QDateTime::currentDateTime().toTime_t();
        return value++;
    }

private slots:
    void FcgiRequest_httpGetQuery()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd?param1=value1&param2=value2";

        FcgiRequest request(url , headers, in);
        QCOMPARE(request.getParam("REQUEST_METHOD"), QString("GET"));
        QCOMPARE(request.getParam("REQUEST_SCHEME"), QString("http"));
        QCOMPARE(request.getParam("SERVER_ADDR"), QString("localhost"));
        QCOMPARE(request.getParam("SERVER_PORT"), QString("81"));
        QCOMPARE(request.getParam("DOCUMENT_URI"), QString("/api/3/ticker/btc_usd"));
        QCOMPARE(request.getParam("QUERY_STRING"), QString("param1=value1&param2=value2"));
        QCOMPARE(request.postData().length(), 0);
    }
    void FcgiRequest_httpPostQuery()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd?param1=value1&param2=value2";
        in = "method=method&param=value";
        QByteArray key = getRandomValidKey(false, false, false);
        QByteArray sign = signWithKey(in, key);
        headers["KEY"] = key;
        headers["SIGN"] = sign;

        FcgiRequest request(url , headers, in);
        QCOMPARE(request.getParam("REQUEST_METHOD"), QString("POST"));
        QCOMPARE(request.getParam("REQUEST_SCHEME"), QString("http"));
        QCOMPARE(request.getParam("SERVER_ADDR"), QString("localhost"));
        QCOMPARE(request.getParam("SERVER_PORT"), QString("81"));
        QCOMPARE(request.getParam("DOCUMENT_URI"), QString("/api/3/ticker/btc_usd"));
        QCOMPARE(request.authHeaders()["Key"], QString::fromUtf8(key));
        QCOMPARE(request.authHeaders()["Sign"], QString::fromUtf8(sign));
        QCOMPARE(request.postData().length(), in.length());
    }

    void QueryParser_urlParse()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd?";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.method(), QString("ticker"));
        QVERIFY(parser.pairs().length() == 1);
    }

    void QueryParser_methodNameRetrieving()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd-btc_usd?ignore_invalid=0";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVERIFY (parser.method() == "ticker");
    }

    void QueryParser_duplicatingPairsRetrieving()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd-btc_usd?ignore_invalid=0";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.pairs().length(), 2);
        QVERIFY(parser.pairs().contains("btc_usd"));
        QVERIFY(!parser.pairs().contains("usd-btc"));
    }

    void QueryParser_pairsRetrieving()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker/btc_usd-btc_eur?ignore_invalid=0";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.pairs().length(), 2);
        QVERIFY(parser.pairs().contains("btc_usd"));
        QVERIFY(parser.pairs().contains("btc_eur"));
    }

    void QueryParser_limitParameterRetrieving()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/tapi/method?param1=value1&limit=10";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.limit(), 10);
    }
    void QueryParser_defaultLimitValue()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/tapi/method?param1=value1";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.limit(), 150);
    }

    void QueryParser_limitOverflow()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/tapi/method?param1=value1&limit=9000";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QCOMPARE(parser.limit(), 5000);
    }

    void Methods_invalidMethod()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/invalid";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce( parser, method);

        QCOMPARE(method, Method::Invalid);
        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("Invalid method"));
    }
    void Methods_publicInfo()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/info";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce( parser, method);

        QCOMPARE(method, Method::PublicInfo);
    }
    void Methods_publicTicker()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/ticker";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PublicTicker);
    }
    void Methods_publicDepth()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/depth";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PublicDepth);
    }
    void Methods_publicTrades()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/trades";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PublicTrades);
    }

    void Info_serverTimePresent()
    {
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/info";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("server_time"));
        QVERIFY(QDateTime::fromTime_t(responce["server_time"].toInt()).date() == QDate::currentDate());
    }
    void Info_pairsPresent()
    {
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/api/3/info";

        FcgiRequest request(url , headers, in);
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

    void Ticker_emptyList()
    {
        // In:    https://btc-e.com/api/3/ticker
        // Out:   {"success":0, "error":"Empty pair list"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(!responce.isEmpty());
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Empty pair list");
    }
    void Ticker_emptyList_withIgnore()
    {
        // In:   https://btc-e.com/api/3/ticker?ignore_invalid=1
        // Out:  {"success":0, "error":"Empty pair list"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/ticker?ignore_invalid=1";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(!responce.isEmpty());
        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Empty pair list");
    }
    void Ticker_emptyPairnameHandle()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-
        // Out:  VALID json
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(!responce.contains("success"));
        QVERIFY(responce.size() == 1);
        QVERIFY(responce.contains("btc_usd"));
        QVERIFY(responce["btc_usd"].toMap().contains("high"));
        QVERIFY(responce["btc_usd"].toMap().contains("low"));
        QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
    }
    void Ticker_onePair()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd
        // Out:  VALID json
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(!responce.contains("success"));
        QVERIFY(responce.size() == 1);
        QVERIFY(responce.contains("btc_usd"));
        QVERIFY(responce["btc_usd"].toMap().contains("high"));
        QVERIFY(responce["btc_usd"].toMap().contains("low"));
        QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
    }
    void Ticker_twoPairs()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-btc_eur
        // Out:  VALID json
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-btc_eur";

        FcgiRequest request(url , headers, in);
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
    void Ticker_ignoreInvalid()
    {
        // In:   http://localhost:81/api/3/tick1er/btc_usd-non_ext?ignore_invalid=1
        // Out:  VALID json
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-non_ext?ignore_invalid=1";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(!responce.contains("success"));
        QVERIFY(responce.size() == 1);
        QVERIFY(responce.contains("btc_usd"));
        QVERIFY(responce["btc_usd"].toMap().contains("high"));
        QVERIFY(responce["btc_usd"].toMap().contains("low"));
        QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
    }

    void Ticker_ignoreInvalidWithouValue()
    {
        // In:   http://localhost:81/api/3/tick1er/btc_usd-non_ext?ignore_invalid
        // Out:  {"success":0, "error":"Invalid pair name: non_ext"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-non_ext?ignore_invalid";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: non_ext");
    }

    void Ticker_ignoreInvalidInvalidSetToZero()
    {
        // In:   http://localhost:81/api/3/tick1er/btc_usd-non_ext?ignore_invalid=0
        // Out:  {"success":0, "error":"Invalid pair name: non_ext"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-non_ext?ignore_invalid=0";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: non_ext");
    }

    void Ticker_invalidPair()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-non_ext
        // Out:  {"success":0, "error":"Invalid pair name: non_ext"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-non_ext";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce( parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: non_ext");
    }

    void Ticker_InvalidPairPrevailOnDoublePair()
    {
        // In:   https://btc-e.com/api/3/ticker/non_ext-non_ext
        // Out:  {"success":0, "error":"Invalid pair name: non_ext"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/non_ext-non_ext";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce( parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: non_ext");
    }

    void Ticker_reversePairName()
    {
        // In:   https://btc-e.com/api/3/ticker/usd_btc
        // Out:  {"success":0, "error":"Invalid pair name: usd_btc"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "https://btc-e.com/api/3/ticker/usd_btc";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce( parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Invalid pair name: usd_btc");
    }

    void Ticker_duplicatePair()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-btc_usd
        // Out:  {"success":0, "error":"Duplicated pair name: btc_usd"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-btc_usd";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce( parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Duplicated pair name: btc_usd");
    }

    void Ticker_ignoreInvalidForDuplicatePair()
    {
        // In:   https://btc-e.com/api/3/ticker/btc_usd-btc_usd?ignore_invalid=1
        // Out:  {"success":0, "error":"Duplicated pair name: btc_usd"}
        Method method;
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost/api/3/ticker/btc_usd-btc_usd?ignore_invalid=1";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);

        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QVERIFY(responce["success"].toInt() == 0);
        QVERIFY(responce.contains("error"));
        QVERIFY(responce["error"] == "Duplicated pair name: btc_usd");
    }

    void Authentication_noKey()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = "method=getInfo&nonce=1";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("api key not specified"));
    }

    void Authentication_invalidKey()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        headers["KEY"] = "KKKK-EEEE-YYYY";
        url = "http://loclahost:81/tapi";
        in = "method=getInfo&nonce=1";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("invalid api key"));
    }

    void Authentication_noSign()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = "method=getInfo&nonce=1";
        QByteArray key = getRandomValidKey(false, false, false);
        headers["KEY"] = key;

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("invalid sign"));
    }

    void Authentication_invalidSign()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        headers["SIGN"] = "SSSiiiGGGnnn";
        url = "http://loclahost:81/tapi";
        in = "method=getInfo&nonce=1";
        QByteArray key = getRandomValidKey(false, false, false);
        headers["KEY"] = key;

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("invalid sign"));
    }
    void Authentication_infoPermissionCheck()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=getInfo&nonce=%1").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(false, true, true);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("api key dont have info permission"));
    }
    void Authentication_tradePermissionCheck()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=Trade&nonce=%1").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(true, false, true);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("api key dont have trade permission"));
    }
    void Authentication_withdrawPermissionCheck()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=RedeemCupon&nonce=%1").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(true, true, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("api key dont have withdraw permission"));
    }

    void Authentication_noNonce()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = "method=getInfo";
        QByteArray key = getRandomValidKey(true, false, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
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

    void Authentication_invalidNonce()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = "method=getInfo&nonce=1";
        QByteArray key = getRandomValidKey(false, false, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
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

    void Method_privateGetInfo()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=getInfo&nonce=%1").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(true, false, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PrivateGetInfo);
    }

    void Method_privateActiveOrders()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=ActiveOrders&nonce=%1").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(true, false, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PrivateActiveOrders);
    }
    void Method_privateOrderInfo()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=OrderInfo&nonce=%1").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(true, false, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PrivateOrderInfo);
    }
    void Method_privateTrade()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=Trade&nonce=%1").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(false, true, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(method, Method::PrivateTrade);
    }

    void Depth_emptyList()
    {
        // In:   https://btc-e.com/api/3/depth
        // Out:  {"success":0, "error":"Empty pair list"}
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/depth";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(!responce.isEmpty());
        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("Empty pair list"));
    }
    void Depth_valid()
    {
        // In:   https://btc-e.com/api/3/depth/btc_usd
        // Out:  VALID json

        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/depth/btc_usd";

        FcgiRequest request(url , headers, in);
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
    void Depth_sortedByRate()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/depth/btc_usd";

        FcgiRequest request(url , headers, in);
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
    void Depth_ratesDecimalDigits()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/depth/btc_usd";

        FcgiRequest request(url , headers, in);
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
    void Depth_limit()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/depth/btc_usd?limit=10";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVariantMap btc_usd = responce["btc_usd"].toMap();
        QVariantList asks = btc_usd["asks"].toList();
        QVariantList bids = btc_usd["bids"].toList();
        QVERIFY(asks.size() <= 10);
        QVERIFY(bids.size() <= 10);
    }
    void Trades_emptyList()
    {
        // In:   https://btc-e.com/api/3/trades
        // Out:  {"success":0, "error":"Empty pair list"}
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/trades";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(!responce.isEmpty());
        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error"));
        QCOMPARE(responce["error"].toString(), QString("Empty pair list"));
    }
    void Trades_valid()
    {
        // In:   https://btc-e.com/api/3/trades/btc_usd
        // Out:  VALID json
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/trades/btc_usd";

        FcgiRequest request(url , headers, in);
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
    void Trades_limit()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/trades/btc_usd?limit=10";

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVariantList btc_usd = responce["btc_usd"].toList();
        QVERIFY(btc_usd.size() <= 10);
    }
    void Trades_sortedByTimestamp()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://localhost:81/api/3/trades/btc_usd";

        FcgiRequest request(url , headers, in);
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
    void GetInfo_valid()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=getInfo&nonce=%1").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(true, false, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 1);
        QVERIFY(responce.contains("return") && responce["return"].canConvert(QVariant::Map));
        QVariantMap result = responce["return"].toMap();
        QVERIFY(result.contains("funds"));
        QVERIFY(result.contains("rights"));
        QVERIFY(result.contains("open_orders"));
        QVERIFY(result.contains("server_time"));
    }
    void ActiveOrders_valid()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=ActiveOrders&nonce=%1").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(true, false, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 1);
        QVERIFY(responce.contains("return") && responce["return"].canConvert(QVariant::Map));
        QVariantMap result = responce["return"].toMap();
        QVERIFY(result.size() > 0);
        QVERIFY(result.first().canConvert(QVariant::Map));
        QVariantMap order = result.first().toMap();
        QVERIFY(order.contains("amount") && order["amount"].canConvert(QVariant::Double) && order["amount"].toDouble() > 0);
        QVERIFY(order.contains("status") && order["status"].canConvert(QVariant::Int) && order["status"].toInt() == 0);
    }
    void OrderInfo_missingOrderId()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=OrderInfo&nonce=%1").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(true, false, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error") && responce["error"].canConvert(QVariant::String));
        QString error = responce["error"].toString();
        QCOMPARE(error, QString("invalid parameter: order_id"));
    }
    void OrderInfo_wrongOrderId()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=OrderInfo&nonce=%1&order_id=6553600").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(true, false, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 0);
        QVERIFY(responce.contains("error") && responce["error"].canConvert(QVariant::String));
        QString error = responce["error"].toString();
        QCOMPARE(error, QString("invalid order"));
    }
    void OrderInfo_valid()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=OrderInfo&nonce=%1&order_id=345").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(true, false, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QVERIFY(responce.contains("success"));
        QCOMPARE(responce["success"].toInt(), 1);
        QVERIFY(responce.contains("return") && responce["return"].canConvert(QVariant::Map));
        QVariantMap result = responce["return"].toMap();
        QVERIFY(result.size() > 0);
        QVERIFY(result.first().canConvert(QVariant::Map));
        QVariantMap order = result.first().toMap();
        QVERIFY(order.contains("amount") && order["amount"].canConvert(QVariant::Double) && order["amount"].toDouble() > 0);
        QVERIFY(order.contains("start_amount") && order["start_amount"].canConvert(QVariant::Double) && order["start_amount"].toDouble() > 0);
        QVERIFY(order.contains("status") && order["status"].canConvert(QVariant::Int));
        QVERIFY(order["start_amount"].toDouble() >= order["start_amount"].toDouble());
    }

    void Trade_parameterPairPresenceCheck()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=Trade&nonce=%1&rate=100&amount=100&type=buy").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(false, true, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(responce["error"].toString(), QString("You incorrectly entered one of fields."));
    }
    void Trade_parameterPairValidityCheck()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=Trade&nonce=%1&rate=100&amount=100&type=sell&pair=usd_btc").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(false, true, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);

        QCOMPARE(responce["error"].toString(), QString("You incorrectly entered one of fields."));
    }
    void Trade_parameterTypeCheck()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=Trade&nonce=%1&rate=100&amount=0.00001&type=bid&pair=btc_usd").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(false, true, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);
//        if (responce["success"].toInt() == 0)
//            std::clog << responce["error"].toString() << std::endl;

        QCOMPARE(responce["success"].toInt(), 0);
        QCOMPARE(responce["error"].toString(), QString("You incorrectly entered one of fields."));
    }

    void Trade_parameterAmountMinValueCheck()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=Trade&nonce=%1&rate=100&amount=0.00001&type=buy&pair=btc_usd").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(false, true, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);
//        if (responce["success"].toInt() == 0)
//            std::clog << responce["error"].toString() << std::endl;

        QCOMPARE(responce["success"].toInt(), 0);
        QString actual = responce["error"].toString();
        QString expected = QString("Value USD must be greater than 0.001 USD.");
        QCOMPARE(actual, expected);
    }
    void Trade_parameterRateMinValueCheck()
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        in = QString("method=Trade&nonce=%1&rate=0.000001&amount=1&type=sell&pair=btc_usd").arg(nonce()).toUtf8();
        QByteArray key = getRandomValidKey(false, true, false);
        headers["KEY"] = key;
        headers["SIGN"] = signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce = getResponce(parser, method);
//        if (responce["success"].toInt() == 0)
//            std::clog << responce["error"].toString() << std::endl;

        QCOMPARE(responce["success"].toInt(), 0);
        QCOMPARE(responce["error"].toString(), QString("Price per BTC must be greater than 0.1 USD."));
    }
};
#endif // UNIT_TESTS_H
