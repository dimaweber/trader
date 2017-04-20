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
    QByteArray input = {"Lorep ipsum sic dolor"};
    QByteArray expected_output = {"ac851d7312fb0b2891b00548cdb4d06b68333e76df384e349719203d37dbbbffa45f2f349ea7a7b596200822100fdd3540201d5570b3c7a0151400626c7d5f27"};
    QByteArray output = hmac_sha512(input, key);
    QVERIFY2(output == expected_output, "Failure");
}

void commonTest::hmac_sha384_test()
{
    QByteArray key = "AveCaesar";
    QByteArray input = {"Lorep ipsum sic dolor"};
    QByteArray expected_output = {"1cc5babc740bdb057bf64ca0790fc6feb7b9d06b1b70e62db6dcce7be407fcb411e60138c17fbde32fcc3c8f1f840b1c"};
    QByteArray output = hmac_sha384(input, key);
    QVERIFY2(output == expected_output, "Failure");
}

QTEST_APPLESS_MAIN(commonTest)

#include "tst_commontest.moc"
