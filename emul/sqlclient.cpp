#include "sqlclient.h"
#include "utils.h"
#include <QVariant>

SqlClient::SqlClient(QSqlDatabase& db)
{
    getRandomKey.reset(new QSqlQuery(db));
    getRandomKeyWithBalance.reset(new QSqlQuery(db));
    getKeySecret.reset(new QSqlQuery(db));

    bool ok = true
    &&  prepareSql(*getRandomKey, "select apikey from apikeys a left join users o on o.user_id = a.user_id where o.user_type=1 and a.info=:info and a.trade=:trade and a.withdraw = :withdraw order by rand() limit 1")
    &&  prepareSql(*getRandomKeyWithBalance, "select apikey from apikeys a left join deposits d  on d.user_id = a.user_id left join currencies c on d.currency_id = c.currency_id  left join users o on o.user_id = a.user_id where o.user_type = 1 and a.trade=true  and c.currency=:currency and d.volume>:amount order by rand() limit 1")
    &&  prepareSql(*getKeySecret, "select secret from apikeys where apikey=:key")
    ;
}

QByteArray SqlClient::randomKeyWithPermissions( bool info, bool trade, bool withdraw)
{
    QVariantMap params;
    params[":info"] = info;
    params[":trade"] = trade;
    params[":withdraw"] = withdraw;

    if (!performSql("get apikey", *getRandomKey, params, true))
        return QByteArray();

    if (!getRandomKey->next())
        return QByteArray();

    return getRandomKey->value(0).toByteArray();
}

/// Return random key that has more then 'amount' of 'currency'
QByteArray SqlClient::randomKeyForTrade(const QString& currency, double amount)
{
    QVariantMap params;
    params[":currency"] = currency;
    params[":amount"] = amount;

    if (!performSql("get apikey", *getRandomKeyWithBalance, params, true))
        return QByteArray();

    if (!getRandomKeyWithBalance->next())
        return QByteArray();

    return getRandomKeyWithBalance->value(0).toByteArray();
}

QByteArray SqlClient::signWithKey(const QByteArray& message, const QByteArray& key)
{
    QByteArray secret = secretForKey(key);
    if (!secret.isEmpty())
        return hmac_sha512(message, secret).toHex();
    return QByteArray();
}

QByteArray SqlClient::secretForKey(const QByteArray& key)
{
    QVariantMap params;
    params[":key"] = key;

    if (!performSql("sign data", *getKeySecret, params, true))
         return QByteArray();
    if (!getKeySecret->next())
        return QByteArray();
    return getKeySecret->value(0).toByteArray();
}
