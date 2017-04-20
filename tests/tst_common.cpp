#include <QString>
#include <QtTest>

#include "utils.h"

class commonTest : public QObject
{
    Q_OBJECT

public:
    commonTest();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void hmac_sha512_test();
    void hmac_sha384_test();
};

commonTest::commonTest()
{
}

void commonTest::initTestCase()
{
}

void commonTest::cleanupTestCase()
{
}

void commonTest::hmac_sha512_test()
{
    QByteArray key = "AveCaesar";
    QByteArray input = {"Lorem ipsum sit dolor"};
    QByteArray expected_output = {"96a9c3fece48b566d1af4abb031b58682d3c1e5a2b0e2f0b373c61f822f0efd8dcfd6f7935a185759571dfd30ba691bfee202763400868bd33e984f14fbf92a3"};
    QByteArray output = hmac_sha512(input, key).toHex();
    QVERIFY2(output == expected_output, "Failure");
}

void commonTest::hmac_sha384_test()
{
    QByteArray key = "AveCaesar";
    QByteArray input = {"Lorem ipsum sit dolor"};
    QByteArray expected_output = {"db3ab031e72512d2e04757aea25891e442e20faca985db445f831d91e0427d648cb2a1acba96525145d916b1100210d3"};
    QByteArray output = hmac_sha384(input, key).toHex();
    QVERIFY2(output == expected_output, "Failure");
}

QTEST_APPLESS_MAIN(commonTest)

#include "tst_commontest.moc"
