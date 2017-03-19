#include "utils.h"
#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlError>
#include <QElapsedTimer>

std::ostream& operator << (std::ostream& stream, const QString& str)
{
    return stream << qPrintable(str);
}


QByteArray hmac_sha512(const QByteArray& message, const QByteArray& key, HashFunction func)
{
    unsigned char* digest = nullptr;
    unsigned int digest_size = 0;

    digest = HMAC(func(), reinterpret_cast<const unsigned char*>(key.constData()), key.length(),
                          reinterpret_cast<const unsigned char*>(message.constData()), message.length(),
                          digest, &digest_size);

    return QByteArray(reinterpret_cast<char*>(digest), digest_size);
}



