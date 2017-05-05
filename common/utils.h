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
{public : explicit MissingField(const QString& msg): std::runtime_error(QString("required field '%1' is missing").arg(msg).toStdString()){}};

class BrokenJson : public std::runtime_error
{public : explicit BrokenJson(const QString& msg): std::runtime_error(msg.toStdString()){}};

class BadFieldValue: public std::runtime_error
{public: BadFieldValue(const QString& field, const QVariant& value): std::runtime_error(QString("Inappropriate value '%2' for field '%1'").arg(field).arg(value.toString()).toStdString()){}};

//typedef const EVP_MD* (*HashFunction)();

QByteArray hmac_sha512(const QByteArray& message, const QByteArray& key); // , HashFunction func = EVP_sha512
QByteArray hmac_sha384(const QByteArray& message, const QByteArray& key);

std::ostream& operator << (std::ostream& stream, const QString& str);

#endif // UTILS_H
