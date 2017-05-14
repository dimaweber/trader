#ifndef SQLCLIENT_H
#define SQLCLIENT_H

#include <QtCore/qglobal.h>
#include <QSqlQuery>

#include <memory>

class QSqlDatabase;

class SqlClient
{
    std::unique_ptr<QSqlQuery> getRandomKey;
    std::unique_ptr<QSqlQuery> getRandomKeyWithBalance;
    std::unique_ptr<QSqlQuery> getKeySecret;
public:
    SqlClient(QSqlDatabase& db);

    QByteArray randomKeyWithPermissions(bool info, bool trade, bool withdraw);
    QByteArray randomKeyForTrade(const QString& currency, double amount);
    QByteArray signWithKey(const QByteArray& message, const QByteArray& key);
    QByteArray secretForKey(const QByteArray& key);
};

#endif // SQLCLIENT_H
