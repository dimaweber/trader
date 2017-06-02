#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <QVariantList>
#include <QDateTime>
#include <QVariantMap>
#include <QString>


#include <stdexcept>

#if defined(__CNUC__)
#if __GNUC__ < 5 && __GNU__MINOR__ < 8
#include <memory>
namespace std
{
template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}
}
#endif
#endif

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

class QSqlQuery;

bool performSql(QString message, QSqlQuery& query, const QVariantMap& binds = QVariantMap(), bool silent=false);
bool performSql(const QString& message, QSqlQuery& query, const QString& sql, bool silent=false);
bool prepareSql(QSqlQuery& query, const QString& sql);


#endif // UTILS_H
