#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <QVariantList>
#include <QDateTime>
#include <QVariantMap>
#include <QString>


#include <stdexcept>

const QString key_field = "__key";

class MissingField : public std::runtime_error
{public : explicit MissingField(const QString& msg): std::runtime_error(msg.toStdString()){}};

class BrokenJson : public std::runtime_error
{public : explicit BrokenJson(const QString& msg): std::runtime_error(msg.toStdString()){}};

//typedef const EVP_MD* (*HashFunction)();

QByteArray hmac_sha512(const QByteArray& message, const QByteArray& key); // , HashFunction func = EVP_sha512
QByteArray hmac_sha384(const QByteArray& message, const QByteArray& key);

std::ostream& operator << (std::ostream& stream, const QString& str);

#endif // UTILS_H
