#ifndef AUTHENTIFICATOR_H
#define AUTHENTIFICATOR_H

#include <QtCore/qglobal.h>
#include <QCache>
#include <QMutex>
#include <QSqlDatabase>
#include <QSqlQuery>

struct ApiKeyCacheItem
{
    quint32 owner_id;
    QByteArray secret;
    bool info;
    bool trade;
    bool withdraw;
    quint32 nonce;
};

class Authentificator
{
    QSqlQuery selectKey;
    QSqlQuery updateNonceQuery;
    static QCache<QString, ApiKeyCacheItem> cache;
    static QMutex accessMutex;

public:
    Authentificator(QSqlDatabase& db);
    bool authOk(const QString& key, const QByteArray& sign, const QString& nonce, const QByteArray rawPostData, QString& message);
    bool hasInfo(const QString& key);
    bool hasTrade(const QString& key);
    bool hasWithdraw(const QString& key);
private:
    bool validateKey(const QString& key);
    bool validateSign(const QString& key, const QByteArray& sign, const QByteArray& data);
    quint32 nonceOnKey(const QString& key);
    bool checkNonce(const QString& key, quint32 nonce);

    QByteArray getSecret(const QString& key);
    bool updateNonce(const QString& key, quint32 nonce);
};

#endif // AUTHENTIFICATOR_H
