#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <QVariantList>
#include <QDateTime>
#include <QVariantMap>
#include <QString>

#include <openssl/sha.h>
#include <openssl/hmac.h>

#include <stdexcept>

class QSqlQuery;

const QString key_field = "__key";

class MissingField : public std::runtime_error
{public : MissingField(const QString& msg): std::runtime_error(msg.toStdString()){}};

class BrokenJson : public std::runtime_error
{public : BrokenJson(const QString& msg): std::runtime_error(msg.toStdString()){}};

typedef const EVP_MD* (*HashFunction)();

QByteArray hmac_sha512(const QByteArray& message, const QByteArray& key, HashFunction func = EVP_sha512);

double read_double(const QVariantMap& map, const QString& name);
QString read_string(const QVariantMap& map, const QString& name);
qint64 read_long(const QVariantMap& map, const QString& name);
QVariantMap read_map(const QVariantMap& map, const QString& name);
QVariantList read_list(const QVariantMap& map, const QString& name);
QDateTime read_timestamp(const QVariantMap& map, const QString& name);

std::ostream& operator << (std::ostream& stream, const QString& str);

bool performSql(const QString& message, QSqlQuery& query, const QVariantMap& binds = QVariantMap(), bool silent=true);
bool performSql(const QString& message, QSqlQuery& query, const QString& sql, bool silent=true);
#endif // UTILS_H
