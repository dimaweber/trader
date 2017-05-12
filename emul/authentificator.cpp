#include "authentificator.h"
#include "sql_database.h"
#include "utils.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

Authentificator::Authentificator(QSqlDatabase& db)
    :selectKey(db), updateNonceQuery(db)
{
    selectKey.prepare("select owner_id, secret, info, trade, withdraw, nonce from apikeys where apikey=:key");
    updateNonceQuery.prepare("update apikeys set nonce=:nonce where apikey=:key");
}

bool Authentificator::authOk(const QString& key, const QByteArray& sign, const QString& nonce, const QByteArray rawPostData, QString& message)
{
    if (key.isEmpty())
    {
        message = "api key not specified";
        return false;
    }
    if (!validateKey(key))
    {
        message = "invalid api key";
        return false;
    }

    if (sign.isEmpty())
    {
        message = "invalid sign";
        return false;
    }

    if (!validateSign(key, sign, rawPostData))
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
    if (!validateKey(key))
        return false;
    return cache[key]->info;
}

bool Authentificator::hasTrade(const QString &key)
{
    if (!validateKey(key))
        return false;
    return cache[key]->trade;
}

bool Authentificator::hasWithdraw(const QString& key)
{
    if (!validateKey(key))
        return false;
    return cache[key]->withdraw;
}

bool Authentificator::validateKey(const QString& key)
{
    if (cache.contains(key))
        return true;
    QVariantMap params;
    params[":key"] = key;
    if (performSql("select key", selectKey, params, true))
        if (selectKey.next())
        {
            std::unique_ptr<ApiKeyCacheItem> pItem = std::make_unique<ApiKeyCacheItem>();
            pItem->owner_id = selectKey.value(0).toUInt();
            pItem->secret = selectKey.value(1).toByteArray();
            pItem->info = selectKey.value(2).toBool();
            pItem->trade = selectKey.value(3).toBool();
            pItem->withdraw = selectKey.value(4).toBool();
            pItem->nonce = selectKey.value(5).toUInt();

            cache.insert(key, pItem.release());
            return true;
        }
    return false;
}

bool Authentificator::validateSign(const QString& key, const QByteArray& sign, const QByteArray& data)
{
    QByteArray secret = getSecret(key);
    QByteArray hmac = hmac_sha512(data, secret).toHex();
    return sign == hmac;
}

quint32 Authentificator::nonceOnKey(const QString& key)
{
    if (validateKey(key))
        return cache[key]->nonce;
    return static_cast<quint32>(-1);
}

bool Authentificator::checkNonce(const QString& key, quint32 nonce)
{
    if (nonce <= nonceOnKey(key))
        return false;
    return updateNonce(key, nonce);
}

QByteArray Authentificator::getSecret(const QString& key)
{
    if (validateKey(key))
        return cache[key]->secret;
    return QByteArray();
}

bool Authentificator::updateNonce(const QString& key, quint32 nonce)
{
    cache.remove(key);
    QVariantMap params;
    params[":key"] = key;
    params[":nonce"] = nonce;
    return performSql("update nonce", updateNonceQuery, params, true);

}
