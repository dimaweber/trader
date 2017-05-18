#include "authentificator.h"
#include "sql_database.h"
#include "utils.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

QCache<QString, ApiKeyCacheItem::Ptr> Authentificator::cache;
QMutex Authentificator::accessMutex;

Authentificator::Authentificator(QSqlDatabase& db)
    :selectKey(db), updateNonceQuery(db)
{
    selectKey.prepare("select user_id, secret, info, trade, withdraw, nonce from apikeys where apikey=:key");
    updateNonceQuery.prepare("update apikeys set nonce=:nonce where apikey=:key");
}

bool Authentificator::authOk(const QString& key, const QByteArray& sign, const QString& nonce, const QByteArray rawPostData, QString& message)
{
    if (key.isEmpty())
    {
        message = "api key not specified";
        return false;
    }

    if (sign.isEmpty())
    {
        message = "invalid sign";
        return false;
    }

    ApiKeyCacheItem::Ptr item = validateKey(key);
    if (!item)
    {
        message = "invalid api key";
        return false;
    }

    if (!validateSign(item, sign, rawPostData))
    {
        message = "invalid sign";
        return false;
    }

    quint32 n = 0;
    quint32 currentNonce = nonceOnKey(key);
    bool ok = false;
    n = nonce.toUInt(&ok);

    if (ok)
    {
        ok = checkNonce(key, n);
    }

    if (!ok)
    {
        message = QString("invalid nonce parameter; on key:%2, you sent:'%1', you should send:%3").arg(nonce).arg(currentNonce).arg(currentNonce+1);
        return false;
    }
    return true;
}

bool Authentificator::hasInfo(const QString& key)
{
    ApiKeyCacheItem::Ptr item = validateKey(key);
    if (!item)
        return false;
    return item->info;
}

bool Authentificator::hasTrade(const QString &key)
{
    ApiKeyCacheItem::Ptr item = validateKey(key);
    if (!item)
        return false;
    return item->trade;
}

bool Authentificator::hasWithdraw(const QString& key)
{
    ApiKeyCacheItem::Ptr item = validateKey(key);
    if (!item)
        return false;
    return item->withdraw;
}

ApiKeyCacheItem::Ptr Authentificator::validateKey(const QString& key)
{
    QMutexLocker lock(&accessMutex);
    ApiKeyCacheItem::Ptr* item =Authentificator::cache.object(key);
    if (item)
        return *item;

    QVariantMap params;
    params[":key"] = key;
    if (performSql("select key", selectKey, params, true))
        if (selectKey.next())
        {
            item = new ApiKeyCacheItem::Ptr (new ApiKeyCacheItem);
            (*item)->user_id = selectKey.value(0).toUInt();
            (*item)->secret = selectKey.value(1).toByteArray();
            (*item)->info = selectKey.value(2).toBool();
            (*item)->trade = selectKey.value(3).toBool();
            (*item)->withdraw = selectKey.value(4).toBool();
            (*item)->nonce = selectKey.value(5).toUInt();

            cache.insert(key, item);
            return *item;
        }
    return nullptr;
}

bool Authentificator::validateSign(const ApiKeyCacheItem::Ptr& pkey, const QByteArray& sign, const QByteArray& data)
{
    if (!pkey)
        return false;
    QByteArray secret = pkey->secret;
    QByteArray hmac = hmac_sha512(data, secret).toHex();
    return sign == hmac;
}

quint32 Authentificator::nonceOnKey(const QString& key)
{
    ApiKeyCacheItem::Ptr item = validateKey(key);
    if (!item)
        return static_cast<quint32>(-1);
    return item->nonce;
}

bool Authentificator::checkNonce(const QString& key, quint32 nonce)
{
    if (nonce <= nonceOnKey(key))
        return false;
    return updateNonce(key, nonce);
}

QByteArray Authentificator::getSecret(const QString& key)
{
    ApiKeyCacheItem::Ptr item = validateKey(key);
    if (!item)
        return QByteArray();
    return item->secret;
}

bool Authentificator::updateNonce(const QString& key, quint32 nonce)
{
    QMutexLocker lock(&Authentificator::accessMutex);
    cache.remove(key);
    QVariantMap params;
    params[":key"] = key;
    params[":nonce"] = nonce;
    return performSql("update nonce", updateNonceQuery, params, true);
}
