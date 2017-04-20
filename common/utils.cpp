#include "utils.h"
#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlError>
#include <QElapsedTimer>

#include <openssl/sha.h>
#include <openssl/hmac.h>

std::ostream& operator << (std::ostream& stream, const QString& str)
{
    return stream << qPrintable(str);
}


QByteArray hmac_sha512(const QByteArray& message, const QByteArray& key)
{
    unsigned char* digest = nullptr;
    unsigned int digest_size = 0;

    digest = HMAC(EVP_sha512(), reinterpret_cast<const unsigned char*>(key.constData()), key.length(),
                          reinterpret_cast<const unsigned char*>(message.constData()), message.length(),
                          digest, &digest_size);

    return QByteArray(reinterpret_cast<char*>(digest), digest_size);
}



QByteArray hmac_sha384(const QByteArray& message, const QByteArray& key)
{
    unsigned char* digest = nullptr;
    unsigned int digest_size = 0;

    digest = HMAC(EVP_sha384(), reinterpret_cast<const unsigned char*>(key.constData()), key.length(),
                          reinterpret_cast<const unsigned char*>(message.constData()), message.length(),
                          digest, &digest_size);

    return QByteArray(reinterpret_cast<char*>(digest), digest_size);
}

