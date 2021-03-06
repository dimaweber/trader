#include "fcgi_request.h"
#include "query_parser.h"
#include "sqlclient.h"
//#include "sql_database.h"
#include "unit_tests.h"
#include "utils.h"

quint32 BtceEmulator_Test::nonce()
{
    static quint32 value = QDateTime::currentDateTime().toTime_t();
    return value++;
}

BtceEmulator_Test::BtceEmulator_Test(QSqlDatabase& db):client(new Responce(db)), sqlClient(new DirectSqlDataAccessor(db))
{}

void BtceEmulator_Test::FcgiRequest_httpGetQuery()
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

void BtceEmulator_Test::FcgiRequest_httpPostQuery()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost:81/api/3/ticker/btc_usd?param1=value1&param2=value2";
    in = "method=method&param=value";
    QByteArray key  = sqlClient->randomKeyWithPermissions(false, false, false);
    QByteArray sign = sqlClient->signWithKey(in, key);
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

void BtceEmulator_Test::QueryParser_urlParse()
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

void BtceEmulator_Test::QueryParser_methodNameRetrieving()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost:81/api/3/ticker/btc_usd-btc_usd?ignore_invalid=0";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);

    QVERIFY (parser.method() == "ticker");
}

void BtceEmulator_Test::QueryParser_duplicatingPairsRetrieving()
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

void BtceEmulator_Test::QueryParser_pairsRetrieving()
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

void BtceEmulator_Test::QueryParser_limitParameterRetrieving()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost:81/tapi/method?param1=value1&limit=10";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);

    QCOMPARE(parser.limit(), 10);
}

void BtceEmulator_Test::QueryParser_defaultLimitValue()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost:81/tapi/method?param1=value1";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);

    QCOMPARE(parser.limit(), 150);
}

void BtceEmulator_Test::QueryParser_limitOverflow()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost:81/tapi/method?param1=value1&limit=9000";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);

    QCOMPARE(parser.limit(), 5000);
}

void BtceEmulator_Test::Methods_invalidMethod()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/api/3/invalid";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce = client->getResponce( parser, method);

    QCOMPARE(method, Method::Invalid);
    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QCOMPARE(responce["error"].toString(), QString("Invalid method"));
}

void BtceEmulator_Test::Methods_publicInfo()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/api/3/info";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce = client->getResponce( parser, method);

    QCOMPARE(method, Method::PublicInfo);
}

void BtceEmulator_Test::Methods_publicTicker()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/api/3/ticker";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce = client->getResponce(parser, method);

    QCOMPARE(method, Method::PublicTicker);
}

void BtceEmulator_Test::Methods_publicDepth()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/api/3/depth";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce = client->getResponce(parser, method);

    QCOMPARE(method, Method::PublicDepth);
}

void BtceEmulator_Test::Methods_publicTrades()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/api/3/trades";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce = client->getResponce(parser, method);

    QCOMPARE(method, Method::PublicTrades);
}

void BtceEmulator_Test::Info_serverTimePresent()
{
    Method method;
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/api/3/info";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);

    QVariantMap responce = client->getResponce(parser, method);

    QVERIFY(responce.contains("server_time"));
    QVERIFY(QDateTime::fromTime_t(responce["server_time"].toInt()).date() == QDate::currentDate());
}

void BtceEmulator_Test::Info_pairsPresent()
{
    Method method;
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/api/3/info";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);

    QVariantMap responce = client->getResponce( parser, method);

    QVERIFY(responce.contains("pairs"));
    QVERIFY(responce["pairs"].canConvert(QVariant::Map));
    QVariantMap pairs = responce["pairs"].toMap();
    QVERIFY(pairs.contains("btc_usd"));
    QVERIFY(pairs["btc_usd"].canConvert(QVariant::Map));
    QVariantMap btc_usd = pairs["btc_usd"].toMap();
    QVERIFY(btc_usd.contains("decimal_places"));
}

void BtceEmulator_Test::Ticker_emptyList()
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

    QVariantMap responce = client->getResponce(parser, method);

    QVERIFY(!responce.isEmpty());
    QVERIFY(responce.contains("success"));
    QVERIFY(responce["success"].toInt() == 0);
    QVERIFY(responce.contains("error"));
    QVERIFY(responce["error"] == "Empty pair list");
}

void BtceEmulator_Test::Ticker_emptyList_withIgnore()
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

    QVariantMap responce = client->getResponce(parser, method);

    QVERIFY(!responce.isEmpty());
    QVERIFY(responce.contains("success"));
    QVERIFY(responce["success"].toInt() == 0);
    QVERIFY(responce.contains("error"));
    QVERIFY(responce["error"] == "Empty pair list");
}

void BtceEmulator_Test::Ticker_emptyPairnameHandle()
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

    QVariantMap responce = client->getResponce(parser, method);

    QVERIFY(!responce.contains("success"));
    QVERIFY(responce.size() == 1);
    QVERIFY(responce.contains("btc_usd"));
    QVERIFY(responce["btc_usd"].toMap().contains("high"));
    QVERIFY(responce["btc_usd"].toMap().contains("low"));
    QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
}

void BtceEmulator_Test::Ticker_onePair()
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

    QVariantMap responce = client->getResponce(parser, method);

    QVERIFY(!responce.contains("success"));
    QVERIFY(responce.size() == 1);
    QVERIFY(responce.contains("btc_usd"));
    QVERIFY(responce["btc_usd"].toMap().contains("high"));
    QVERIFY(responce["btc_usd"].toMap().contains("low"));
    QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
}

void BtceEmulator_Test::Ticker_twoPairs()
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

    QVariantMap responce = client->getResponce(parser, method);

    QVERIFY(!responce.contains("success"));
    QVERIFY(responce.size() == 2);
    QVERIFY(responce.contains("btc_usd"));
    QVERIFY(responce.contains("btc_eur"));
    QVERIFY(responce["btc_usd"].toMap().contains("high"));
    QVERIFY(responce["btc_usd"].toMap().contains("low"));
    QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
}

void BtceEmulator_Test::Ticker_ignoreInvalid()
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

    QVariantMap responce = client->getResponce(parser, method);

    QVERIFY(!responce.contains("success"));
    QVERIFY(responce.size() == 1);
    QVERIFY(responce.contains("btc_usd"));
    QVERIFY(responce["btc_usd"].toMap().contains("high"));
    QVERIFY(responce["btc_usd"].toMap().contains("low"));
    QVERIFY(responce["btc_usd"].toMap()["high"].toFloat() >= responce["btc_usd"].toMap()["low"].toFloat());
}

void BtceEmulator_Test::Ticker_ignoreInvalidWithouValue()
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

    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QVERIFY(responce["success"].toInt() == 0);
    QVERIFY(responce.contains("error"));
    QVERIFY(responce["error"] == "Invalid pair name: non_ext");
}

void BtceEmulator_Test::Ticker_ignoreInvalidInvalidSetToZero()
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

    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QVERIFY(responce["success"].toInt() == 0);
    QVERIFY(responce.contains("error"));
    QVERIFY(responce["error"] == "Invalid pair name: non_ext");
}

void BtceEmulator_Test::Ticker_invalidPair()
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

    QVariantMap responce = client->getResponce( parser, method);

    QVERIFY(responce.contains("success"));
    QVERIFY(responce["success"].toInt() == 0);
    QVERIFY(responce.contains("error"));
    QVERIFY(responce["error"] == "Invalid pair name: non_ext");
}

void BtceEmulator_Test::Ticker_InvalidPairPrevailOnDoublePair()
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

    QVariantMap responce = client->getResponce( parser, method);

    QVERIFY(responce.contains("success"));
    QVERIFY(responce["success"].toInt() == 0);
    QVERIFY(responce.contains("error"));
    QVERIFY(responce["error"] == "Invalid pair name: non_ext");
}

void BtceEmulator_Test::Ticker_reversePairName()
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

    QVariantMap responce = client->getResponce( parser, method);

    QVERIFY(responce.contains("success"));
    QVERIFY(responce["success"].toInt() == 0);
    QVERIFY(responce.contains("error"));
    QVERIFY(responce["error"] == "Invalid pair name: usd_btc");
}

void BtceEmulator_Test::Ticker_timestampFormat()
{
    Method method;
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost/api/3/ticker/btc_usd";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);

    QVariantMap responce = client->getResponce( parser, method);

    QVERIFY(responce["btc_usd"].toMap()["updated"].canConvert(QVariant::LongLong));
}

void BtceEmulator_Test::Ticker_duplicatePair()
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

    QVariantMap responce = client->getResponce( parser, method);

    QVERIFY(responce.contains("success"));
    QVERIFY(responce["success"].toInt() == 0);
    QVERIFY(responce.contains("error"));
    QVERIFY(responce["error"] == "Duplicated pair name: btc_usd");
}

void BtceEmulator_Test::Ticker_ignoreInvalidForDuplicatePair()
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

    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QVERIFY(responce["success"].toInt() == 0);
    QVERIFY(responce.contains("error"));
    QVERIFY(responce["error"] == "Duplicated pair name: btc_usd");
}

void BtceEmulator_Test::Authentication_noKey()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = "method=getInfo&nonce=1";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QCOMPARE(responce["error"].toString(), QString("api key not specified"));
}

void BtceEmulator_Test::Authentication_invalidKey()
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
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QCOMPARE(responce["error"].toString(), QString("invalid api key"));
}

void BtceEmulator_Test::Authentication_noSign()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = "method=getInfo&nonce=1";
    QByteArray key = sqlClient->randomKeyWithPermissions(false, false, false);
    headers["KEY"] = key;

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QCOMPARE(responce["error"].toString(), QString("invalid sign"));
}

void BtceEmulator_Test::Authentication_invalidSign()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    headers["SIGN"] = "SSSiiiGGGnnn";
    url = "http://loclahost:81/tapi";
    in = "method=getInfo&nonce=1";
    QByteArray key = sqlClient->randomKeyWithPermissions(false, false, false);
    headers["KEY"] = key;

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QCOMPARE(responce["error"].toString(), QString("invalid sign"));
}

void BtceEmulator_Test::Authentication_infoPermissionCheck()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=getInfo&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(false, true, true);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QCOMPARE(responce["error"].toString(), QString("api key dont have info permission"));
}

void BtceEmulator_Test::Authentication_tradePermissionCheck()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=Trade&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(true, false, true);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QCOMPARE(responce["error"].toString(), QString("api key dont have trade permission"));
}

void BtceEmulator_Test::Authentication_withdrawPermissionCheck()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=RedeemCupon&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(true, true, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QCOMPARE(responce["error"].toString(), QString("api key dont have withdraw permission"));
}

void BtceEmulator_Test::Authentication_noNonce()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = "method=getInfo";
    QByteArray key = sqlClient->randomKeyWithPermissions(true, false, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QRegExp rx("invalid nonce parameter; on key:[0-9]*, you sent:'', you should send:[0-9]*");
    int pos = rx.indexIn(responce["error"].toString());
    QCOMPARE(pos, 0);
}

void BtceEmulator_Test::Authentication_invalidNonce()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = "method=getInfo&nonce=1";
    QByteArray key = sqlClient->randomKeyWithPermissions(false, false, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QRegExp rx("invalid nonce parameter; on key:[0-9]*, you sent:'[0-9]+', you should send:[0-9]*");
    int pos = rx.indexIn(responce["error"].toString());
    QCOMPARE(pos, 0);
}

void BtceEmulator_Test::Method_privateGetInfo()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=getInfo&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(true, false, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QCOMPARE(method, Method::PrivateGetInfo);
}

void BtceEmulator_Test::Method_privateActiveOrders()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=ActiveOrders&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(true, false, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QCOMPARE(method, Method::PrivateActiveOrders);
}

void BtceEmulator_Test::Method_privateOrderInfo()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=OrderInfo&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(true, false, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QCOMPARE(method, Method::PrivateOrderInfo);
}

void BtceEmulator_Test::Method_privateTrade()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=Trade&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(false, true, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QCOMPARE(method, Method::PrivateTrade);
}

void BtceEmulator_Test::Method_privateCancelOrder()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=CancelOrder&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(false, true, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QCOMPARE(method, Method::PrivateCanelOrder);
}

void BtceEmulator_Test::Depth_emptyList()
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
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(!responce.isEmpty());
    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QCOMPARE(responce["error"].toString(), QString("Empty pair list"));
}

void BtceEmulator_Test::Depth_valid()
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
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(!responce.isEmpty());
    QVERIFY(!responce.contains("success"));
    QCOMPARE(responce.size(), 1);
    QVERIFY(responce.contains("btc_usd"));
    QVariantMap btc_usd = responce["btc_usd"].toMap();
    QVERIFY(btc_usd.contains("asks"));
    QVERIFY(btc_usd.contains("bids"));
}
void BtceEmulator_Test::Depth_twoPairs()
{
    // In:   https://btc-e.com/api/3/ticker/btc_usd-btc_eur
    // Out:  VALID json
    Method method;
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost/api/3/depth/btc_usd-btc_eur";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);

    QVariantMap responce = client->getResponce(parser, method);

    QVERIFY(responce.size() == 2);
    QVERIFY(responce.contains("btc_usd"));
    QVERIFY(responce.contains("btc_eur"));
}

void BtceEmulator_Test::Depth_sortedByRate()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost:81/api/3/depth/btc_usd";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVariantMap btc_usd = responce["btc_usd"].toMap();
    QVariantList asks = btc_usd["asks"].toList();
    QVariantList bids = btc_usd["bids"].toList();
    QVERIFY(asks.size() > 1 && bids.size() > 1);
    QVERIFY(asks[0].toList()[0].toFloat() < asks[1].toList()[0].toFloat());
    QVERIFY(bids[0].toList()[0].toFloat() > bids[1].toList()[0].toFloat());
    QVERIFY(bids[0].toList()[0].toFloat() < asks[0].toList()[0].toFloat());
}

void BtceEmulator_Test::Depth_ratesDecimalDigits()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost:81/api/3/depth/btc_usd";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

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

void BtceEmulator_Test::Depth_limit()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost:81/api/3/depth/btc_usd?limit=10";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVariantMap btc_usd = responce["btc_usd"].toMap();
    QVariantList asks = btc_usd["asks"].toList();
    QVariantList bids = btc_usd["bids"].toList();
    QVERIFY(asks.size() <= 10);
    QVERIFY(bids.size() <= 10);
}

void BtceEmulator_Test::Trades_emptyList()
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
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(!responce.isEmpty());
    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error"));
    QCOMPARE(responce["error"].toString(), QString("Empty pair list"));
}

void BtceEmulator_Test::Trades_valid()
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
    QVariantMap responce =client->getResponce(parser, method);

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
        QVERIFY(trade.contains("amount") && trade["amount"].toFloat() >= 0 );
        QVERIFY(trade.contains("tid") && trade["tid"].toUInt() > 0 );
        QVERIFY(trade.contains("timestamp") && trade["timestamp"].toUInt() > 0 );
    }
}

void BtceEmulator_Test::Trades_limit()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost:81/api/3/trades/btc_usd?limit=10";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVariantList btc_usd = responce["btc_usd"].toList();
    QVERIFY(btc_usd.size() <= 10);
}

void BtceEmulator_Test::Trades_sortedByTimestamp()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://localhost:81/api/3/trades/btc_usd";

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVariantList btc_usd = responce["btc_usd"].toList();
    for (int i=0; i<btc_usd.size()-1;i++)
    {
        QVariantMap a = btc_usd[i].toMap();
        QVariantMap b = btc_usd[i+1].toMap();
        QVERIFY ( a["tid"].toUInt() > b["tid"].toUInt());
    }
}

void BtceEmulator_Test::GetInfo_valid()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=getInfo&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(true, false, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 1);
    QVERIFY(responce.contains("return") && responce["return"].canConvert(QVariant::Map));
    QVariantMap result = responce["return"].toMap();
    QVERIFY(result.contains("funds"));
    QVERIFY(result.contains("rights"));
    QVERIFY(result.contains("open_orders"));
    QVERIFY(result.contains("server_time"));
}

void BtceEmulator_Test::ActiveOrders_valid()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=ActiveOrders&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(true, false, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

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

void BtceEmulator_Test::OrderInfo_missingOrderId()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=OrderInfo&nonce=%1").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(true, false, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error") && responce["error"].canConvert(QVariant::String));
    QString error = responce["error"].toString();
    QCOMPARE(error, QString("invalid parameter: order_id"));
}

void BtceEmulator_Test::OrderInfo_wrongOrderId()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=OrderInfo&nonce=%1&order_id=6553600").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(true, false, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 0);
    QVERIFY(responce.contains("error") && responce["error"].canConvert(QVariant::String));
    QString error = responce["error"].toString();
    QCOMPARE(error, QString("invalid order"));
}

void BtceEmulator_Test::OrderInfo_valid()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=OrderInfo&nonce=%1&order_id=58320").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(true, false, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QVERIFY(responce.contains("success"));
    QCOMPARE(responce["success"].toInt(), 1);
    QVERIFY(responce.contains("return") && responce["return"].canConvert(QVariant::Map));
    QVariantMap result = responce["return"].toMap();
    QVERIFY(result.size() > 0);
    QVERIFY(result.first().canConvert(QVariant::Map));
    QVariantMap order = result.first().toMap();
    QVERIFY(order.contains("amount") && order["amount"].canConvert(QVariant::Double) && order["amount"].toDouble() >= 0);
    QVERIFY(order.contains("start_amount") && order["start_amount"].canConvert(QVariant::Double) && order["start_amount"].toDouble() > 0);
    QVERIFY(order.contains("status") && order["status"].canConvert(QVariant::Int));
    QVERIFY(order["start_amount"].toDouble() >= order["amount"].toDouble());
}

void BtceEmulator_Test::Trade_parameterPairPresenceCheck()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=Trade&nonce=%1&rate=100&amount=100&type=buy").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(false, true, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QCOMPARE(responce["error"].toString(), QString("You incorrectly entered one of fields."));
}

void BtceEmulator_Test::Trade_parameterPairValidityCheck()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=Trade&nonce=%1&rate=100&amount=100&type=sell&pair=usd_btc").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(false, true, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QCOMPARE(responce["error"].toString(), QString("You incorrectly entered one of fields."));
}

void BtceEmulator_Test::Trade_parameterTypeCheck()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=Trade&nonce=%1&rate=100&amount=0.00001&type=bid&pair=btc_usd").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(false, true, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);
    //        if (responce["success"].toInt() == 0)
    //            std::clog << responce["error"].toString() << std::endl;

    QCOMPARE(responce["success"].toInt(), 0);
    QCOMPARE(responce["error"].toString(), QString("You incorrectly entered one of fields."));
}

void BtceEmulator_Test::Trade_parameterAmountMinValueCheck()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=Trade&nonce=%1&rate=100&amount=0.00001&type=buy&pair=btc_usd").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyForTrade("usd", Amount(0.2));
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);
    //        if (responce["success"].toInt() == 0)
    //            std::clog << responce["error"].toString() << std::endl;

    QCOMPARE(responce["success"].toInt(), 0);
    QString actual = responce["error"].toString();
    QString expected = QString("Value USD must be greater than 0.001000 USD.");
    QCOMPARE(actual, expected);
}

void BtceEmulator_Test::Trade_parameterRateMinValueCheck()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    in = QString("method=Trade&nonce=%1&rate=0.000001&amount=1&type=sell&pair=btc_usd").arg(nonce()).toUtf8();
    QByteArray key = sqlClient->randomKeyWithPermissions(false, true, false);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);
    //        if (responce["success"].toInt() == 0)
    //            std::clog << responce["error"].toString() << std::endl;

    QCOMPARE(responce["success"].toInt(), 0);
    QCOMPARE(responce["error"].toString(), QString("Price per BTC must be greater than 0.100 USD."));
}

void BtceEmulator_Test::Trade_buy()
{
    QVariantMap balanceBefore = client->exchangeBalance();

    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    Amount amount (0.2);
    Rate rate (1850);
    Amount balance;
    QString currency;
    balance = rate * amount;
    currency = "usd";
    in = QString("method=Trade&nonce=%1&rate=%4&amount=%3&type=%2&pair=btc_usd").arg(nonce()).arg("buy").arg(dec2qstr(amount, 6)).arg(dec2qstr(rate, 6)).toUtf8();
    QByteArray key = sqlClient->randomKeyForTrade(currency, balance);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    if (responce["success"].toInt() == 0)
        std::clog << responce["error"].toString() << std::endl;
    QCOMPARE(responce["success"].toInt(), 1);
    QVERIFY(responce.contains("return") && responce["return"].canConvert(QVariant::Map));
    QVariantMap ret = responce["return"].toMap();
    Amount received = qvar2dec<7>(ret["received"]);
    Amount remains = qvar2dec<7>(ret["remains"]);
    quint32 order_id = ret["order_id"].toUInt();
    QVERIFY(received /(Fee(1)-Fee(0.002)) + remains == amount);
    QVERIFY((remains > Amount(0) and order_id != 0) or (remains == Amount(0) and order_id == 0));

    QVariantMap balanceAfter = client->exchangeBalance();
    QCOMPARE(balanceBefore, balanceAfter);
}

void BtceEmulator_Test::Trade_sell()
{
    QVariantMap balanceBefore = client->exchangeBalance();

    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    bool isSell = true;
    Amount amount (0.2);
    Rate rate;
    Amount balance;
    Fee fee  (0.002);
    QString currency;
    if (isSell)
    {
        rate = Rate(0.1);
        balance = amount;
        currency = "btc";
    }
    else
    {
        rate = Rate(1850);
        balance = rate * amount;
        currency = "usd";
    }
    in = QString("method=Trade&nonce=%1&rate=%4&amount=%3&type=%2&pair=btc_usd").arg(nonce()).arg(isSell?"sell":"buy").arg(dec2qstr(amount, 6)).arg(dec2qstr(rate, 6)).toUtf8();
    QByteArray key = sqlClient->randomKeyForTrade(currency, balance);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;
    QVariantMap responce =client->getResponce(parser, method);

    QCOMPARE(responce["success"].toInt(), 1);
    QVERIFY(responce.contains("return") && responce["return"].canConvert(QVariant::Map));
    QVariantMap ret = responce["return"].toMap();
    Amount received = qvar2dec<7>(ret["received"]);
    Amount remains = qvar2dec<7>(ret["remains"]);
    quint32 order_id = ret["order_id"].toUInt();
    QVERIFY(received / (Fee(1)-fee) + remains == amount);
    QVERIFY((remains > Amount(0) and order_id != 0) or (remains == Amount(0) and order_id == 0));

    QVariantMap balanceAfter = client->exchangeBalance();
    QCOMPARE(balanceBefore, balanceAfter);

}

void BtceEmulator_Test::Trade_depositValid_sell()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    Amount amount  {0.2};
    Rate rate (0.1);
    Fee fee (.002);

    QString currency  = "btc";

    in = QString("method=Trade&nonce=%1&rate=%4&amount=%3&type=%2&pair=btc_usd").arg(nonce()).arg("sell").arg(dec2qstr(amount, 6)).arg(dec2qstr(rate, 6)).toUtf8();

    QByteArray key = sqlClient->randomKeyForTrade(currency, amount);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;

    Amount btc_before = sqlClient->getDepositCurrencyVolume(key, "btc");
    Amount usd_before = sqlClient->getDepositCurrencyVolume(key, "usd");

    QVariantMap responce =client->getResponce(parser, method);

    Amount btc_after = sqlClient->getDepositCurrencyVolume(key, "btc");
    Amount usd_after = sqlClient->getDepositCurrencyVolume(key, "usd");

    QCOMPARE(responce["success"].toInt(), 1);
    QVERIFY(responce.contains("return") && responce["return"].canConvert(QVariant::Map));
    QVariantMap ret = responce["return"].toMap();
    Amount received = Amount(ret["received"].toString().toStdString());
    Amount remains = Amount(ret["remains"].toString().toStdString());
    quint32 order_id = ret["order_id"].toUInt();
    QVERIFY(received/(Amount(1)-fee) + remains == amount);
    QVERIFY(   (remains > Amount(0) and order_id != 0)
            or (remains == Amount(0) and order_id == 0));

    QVERIFY(btc_before - btc_after == amount);
    QVERIFY(usd_after - usd_before  >= received * rate);
}

void BtceEmulator_Test::Trade_depositValid_buy()
{
    QByteArray in;
    QMap<QString, QString> headers;
    QUrl url;
    url = "http://loclahost:81/tapi";
    Amount amount  {0.2};
    Rate rate (2000);
    QString currency  = "usd";

    in = QString("method=Trade&nonce=%1&rate=%4&amount=%3&type=%2&pair=btc_usd").arg(nonce()).arg("buy").arg(dec2qstr(amount, 6)).arg(dec2qstr(rate, 6)).toUtf8();

    QByteArray key = sqlClient->randomKeyForTrade(currency, amount*rate);
    headers["KEY"] = key;
    headers["SIGN"] = sqlClient->signWithKey(in, key);

    FcgiRequest request(url , headers, in);
    QueryParser parser(request);
    Method method;

    Amount btc_before = sqlClient->getDepositCurrencyVolume(key, "btc");
    Amount usd_before = sqlClient->getDepositCurrencyVolume(key, "usd");

    QVariantMap responce =client->getResponce(parser, method);

    Amount btc_after = sqlClient->getDepositCurrencyVolume(key, "btc");
    Amount usd_after = sqlClient->getDepositCurrencyVolume(key, "usd");

    QCOMPARE(responce["success"].toInt(), 1);
    QVERIFY(responce.contains("return") && responce["return"].canConvert(QVariant::Map));
    QVariantMap ret = responce["return"].toMap();
    Amount received = Amount(ret["received"].toString().toStdString());
    Amount remains = Amount(ret["remains"].toString().toStdString());
    quint32 order_id = ret["order_id"].toUInt();
    Amount contra_fee = Amount(1) - Amount(.002);
    QVERIFY(received/contra_fee + remains == amount);
    QVERIFY(   (remains > Amount(0) and order_id != 0)
            or (remains == Amount(0) and order_id == 0));

    QVERIFY(btc_after - btc_before == received);
    std::clog << usd_before - usd_after << " USD" << std::endl
              << amount * rate << " USD" << std::endl
              << received * rate << std::endl;
    QVERIFY(usd_before - usd_after  <= amount * rate);
}

void BtceEmulator_Test::Trade_exchangeTotalBalanceValid()
{
    QVariantMap balanceBefore = client->exchangeBalance();

    for (int i=0; i<4; i++)
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        bool isSell = qrand() % 2;
        Amount amount ((qrand() % 1000) / 100.0 + 0.01);
        Rate rate (1750 + (qrand() % 100) / 10.0 - 5);
        Amount balance;
        QString currency;
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
        in = QString("method=Trade&nonce=%1&rate=%4&amount=%3&type=%2&pair=btc_usd").arg(nonce()).arg(isSell?"sell":"buy").arg(dec2qstr(amount, 6)).arg(dec2qstr(rate, 6)).toUtf8();
        QByteArray key = sqlClient->randomKeyForTrade(currency, balance);
        if (key.isEmpty())
            continue;
        headers["KEY"] = key;
        headers["SIGN"] = sqlClient->signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QVariantMap responce =client->getResponce(parser, method);

        if (responce["success"].toInt() == 0)
            std::clog << responce["error"].toString() << std::endl;
        QCOMPARE(responce["success"].toInt(), 1);

        QVariantMap balanceAfter = client->exchangeBalance();
        QCOMPARE(balanceBefore, balanceAfter);
    }
}

void BtceEmulator_Test::Trade_tradeBenchmark()
{
    QVariantMap balanceBefore = client->exchangeBalance();

    for (int i=0; i<4; i++)
    {
        QByteArray in;
        QMap<QString, QString> headers;
        QUrl url;
        url = "http://loclahost:81/tapi";
        bool isSell = qrand() % 2;
        Amount amount ((qrand() % 1000) / 100.0 + 0.01);
        Rate rate (1750 + (qrand() % 100) / 10.0 - 5);
        Amount balance;
        QString currency;
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
        in = QString("method=Trade&nonce=%1&rate=%4&amount=%3&type=%2&pair=btc_usd").arg(nonce()).arg(isSell?"sell":"buy").arg(dec2qstr(amount, 6)).arg(dec2qstr(rate, 6)).toUtf8();
        QByteArray key = sqlClient->randomKeyForTrade(currency, balance);
        if (key.isEmpty())
            continue;
        headers["KEY"] = key;
        headers["SIGN"] = sqlClient->signWithKey(in, key);

        FcgiRequest request(url , headers, in);
        QueryParser parser(request);
        Method method;
        QBENCHMARK(client->getResponce(parser, method));
    }
}
