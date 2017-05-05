#include <QtMath>
#include <QString>
#include <QtTest>

#include "utils.h"
#include "btce.h"

class CommonTest : public QObject
{
    Q_OBJECT

public:
    CommonTest();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void hmac_sha512_test();
    void hmac_sha384_test();
    void btce_api_success_trade_info_test();
    void btce_api_failed_trade_info_test();
private:
    std::unique_ptr<IKeyStorage> keyStorage;
    std::unique_ptr<BtcObjects::Funds> funds;
};

CommonTest::CommonTest()
{
}

void CommonTest::initTestCase()
{
    keyStorage = std::make_unique<FileKeyStorage>("/dev/null");
    funds = std::make_unique<BtcObjects::Funds>();
}

void CommonTest::cleanupTestCase()
{
}

void CommonTest::init()
{
    (*funds)["usd"] = 100;
    (*funds)["btc"] = 2;
}

void CommonTest::hmac_sha512_test()
{
    QByteArray key = "AveCaesar";
    QByteArray input = {"Lorem ipsum sit dolor"};
    QByteArray expected_output = {"96a9c3fece48b566d1af4abb031b58682d3c1e5a2b0e2f0b373c61f822f0efd8dcfd6f7935a185759571dfd30ba691bfee202763400868bd33e984f14fbf92a3"};
    QByteArray output = hmac_sha512(input, key).toHex();
    QVERIFY2(output == expected_output, "Failure");
}

void CommonTest::hmac_sha384_test()
{
    QByteArray key = "AveCaesar";
    QByteArray input = {"Lorem ipsum sit dolor"};
    QByteArray expected_output = {"db3ab031e72512d2e04757aea25891e442e20faca985db445f831d91e0427d648cb2a1acba96525145d916b1100210d3"};
    QByteArray output = hmac_sha384(input, key).toHex();
    QVERIFY2(output == expected_output, "Failure");
}

void CommonTest::btce_api_success_trade_info_test()
{
    QString json =
    "{"
       "\"success\":1,"
        "\"return\":{"
            "\"funds\":{"
                "\"usd\":325,"
                "\"btc\":23.998,"
                "\"ltc\":0"
            "},"
            "\"rights\":{"
                "\"info\":1,"
                "\"trade\":0,"
                "\"withdraw\":0"
            "},"
            "\"transaction_count\":0,"
            "\"open_orders\":1,"
            "\"server_time\":1342123547"
        "}"
    "}";
    std::unique_ptr<BtcTradeApi::Api>info = std::make_unique<BtcTradeApi::Info>(*keyStorage, *funds);

    bool parse_success = info->parse(json.toUtf8());

    QVERIFY2(parse_success, "parse failed");
    QVERIFY(info->isSuccess());
    QVERIFY2(qFuzzyCompare((*funds)["usd"], 325), "USD funds parsed wrong");
    QVERIFY2(qFuzzyCompare((*funds)["btc"], 23.998), "BTC funds parsed wrong");
}

void CommonTest::btce_api_failed_trade_info_test()
{
    QString json = "{\"success\":0, \"error\": \"malformed request\"}";
    std::unique_ptr<BtcTradeApi::Api>info = std::make_unique<BtcTradeApi::Info>(*keyStorage, *funds);

    bool parse_success;
    parse_success = info->parse(json.toUtf8());

    QVERIFY(parse_success);
    QVERIFY(!info->isSuccess());
    QVERIFY(info->error() == "malformed request");
    QVERIFY2(qFuzzyCompare((*funds)["usd"], 100), "USD funds parsed wrong");
    QVERIFY2(qFuzzyCompare((*funds)["btc"], 2), "BTC funds parsed wrong");
}



QTEST_APPLESS_MAIN(CommonTest)

#include "tst_common.moc"
