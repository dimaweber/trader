#include "authentificator.h"
#include "sql_database.h"
#include "utils.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

//QCache<QString, ApiKeyCacheItem::Ptr> Authentificator::cache;
//QMutex Authentificator::accessMutex;

Authentificator::Authentificator(std::shared_ptr<AbstractDataAccessor>& dataAccessor)
    :dataAccessor(dataAccessor)
{
}

bool Authentificator::authOk(const QString& key, const QByteArray& sign, const QString& nonce, const QByteArray rawPostData, QString& message)
{
    if (key.isEmpty())
    {
        message = "api key not specified";
        return false;
    }

    ApikeyInfo::Ptr item = validateKey(key);
    if (!item)
    {
        message = "invalid api key";
        return false;
    }

    if (sign.isEmpty())
    {
        message = "invalid sign";
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
    ApikeyInfo::Ptr item = validateKey(key);
    if (!item)
        return false;
    return item->info;
}

bool Authentificator::hasTrade(const QString &key)
{
    ApikeyInfo::Ptr item = validateKey(key);
    if (!item)
        return false;
    return item->trade;
}

bool Authentificator::hasWithdraw(const QString& key)
{
    ApikeyInfo::Ptr item = validateKey(key);
    if (!item)
        return false;
    return item->withdraw;
}

ApikeyInfo::Ptr Authentificator::validateKey(const QString& key)
{
    ApikeyInfo::Ptr item = dataAccessor->apikeyInfo(key);
    return item;
}

bool Authentificator::validateSign(const ApikeyInfo::Ptr& pkey, const QByteArray& sign, const QByteArray& data)
{
    if (!pkey)
        return false;
    QByteArray secret = pkey->secret;
    QByteArray hmac = hmac_sha512(data, secret).toHex();
    return sign == hmac;
}

quint32 Authentificator::nonceOnKey(const QString& key)
{
    ApikeyInfo::Ptr item = validateKey(key);
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
    ApikeyInfo::Ptr item = validateKey(key);
    if (!item)
        return QByteArray();
    return item->secret;
}

bool Authentificator::updateNonce(const QString& key, quint32 nonce)
{
    return dataAccessor->updateNonce(key, nonce);
}
