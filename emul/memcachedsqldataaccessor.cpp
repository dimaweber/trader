#include "memcachedsqldataaccessor.h"

#include <QDataStream>

#include <iostream>

MemcachedSqlDataAccessor::MemcachedSqlDataAccessor(QSqlDatabase& db)
    :DirectSqlDataAccessor (db)
{
    memcached_return rc;
    memc = memcached_create(nullptr);
    servers = memcached_server_list_append(servers, "localhost", 11211, &rc);
    rc = memcached_server_push(memc, servers);

    if (rc == MEMCACHED_SUCCESS)
    {
        std::clog << "connected to memcached" << std::endl;
    }
    else
        std::clog << "fail to connect to memcached: " << memcached_strerror(memc, rc) << std::endl;
}

MemcachedSqlDataAccessor::~MemcachedSqlDataAccessor()
{
    memcached_free(memc);
}

template <class INFO>
typename INFO::Ptr MemcachedSqlDataAccessor::cache_get(const QString& key)
{
    typename INFO::Ptr info;
    memcached_return rc;
    QByteArray value;
    QByteArray k;
    k = key.toUtf8();
    char* data = nullptr;
    size_t value_length;
    data = memcached_get(memc, k.constData(), k.length(), &value_length, 0, &rc);
    if (rc == MEMCACHED_SUCCESS)
    {
        value.setRawData(data, value_length);
        info = std::make_shared<INFO>();
        info->unpack(value);
    }
    return info;
}

template<class INFOPtr>
void MemcachedSqlDataAccessor::cache_put(const QString &key, const INFOPtr &info, int timeout)
{
    QByteArray value = info->pack();
    QByteArray k = key.toUtf8();
    memcached_set(memc, k.constData(), k.length(), value.constData(), value.length(), timeout, 0);
}

PairInfo::List MemcachedSqlDataAccessor::allPairsInfoList()
{
    PairInfo::List ret = DirectSqlDataAccessor::allPairsInfoList();
//    QMutexLocker lock(&LocalCachesSqlDataAccessor::pairInfoCacheRWAccess);
    for(const PairInfo::Ptr& info : ret)
    {
        QString key = QString("pair:%1").arg(info->pair);
        cache_put(key, info, 100);
    }
    return ret;
}

PairInfo::Ptr MemcachedSqlDataAccessor::pairInfo(const PairName &pair)
{
//    QMutexLocker rlock(&LocalCachesSqlDataAccessor::pairInfoCacheRWAccess);
    QString key = QString("pair:%1").arg(pair);
    auto info = cache_get<PairInfo>(key);
    if (!info)
    {
        info = DirectSqlDataAccessor::pairInfo(pair);
        if (info)
            cache_put(key, info, 100);
    }
    return info;
}

TickerInfo::Ptr MemcachedSqlDataAccessor::tickerInfo(const PairName &pair)
{
//    QMutexLocker lock(&LocalCachesSqlDataAccessor::tickerInfoCacheRWAccess);
    QString key = QString("ticker:%1").arg(pair);
    auto info = cache_get<TickerInfo>(key);
    if (!info)
    {
        info = DirectSqlDataAccessor::tickerInfo(pair);
        if (info)
            cache_put(key, info, 30);
    }
    return info;
}

OrderInfo::Ptr MemcachedSqlDataAccessor::orderInfo(OrderId order_id)
{
    QString key = QString("order:%1").arg(order_id);
    auto info = cache_get<OrderInfo>(key);
    if (!info)
    {
        info = DirectSqlDataAccessor::orderInfo(order_id);
        if (info)
            cache_put(key, info, 100);
    }
    return info;
}

ApikeyInfo::Ptr MemcachedSqlDataAccessor::apikeyInfo(const ApiKey &apikey)
{
    QString key = QString("apikey:%1").arg(apikey);
    auto info = cache_get<ApikeyInfo>(key);
    if (!info)
    {
        info = DirectSqlDataAccessor::apikeyInfo(apikey);
        if (info)
            cache_put(key, info, 100);
    }
    return info;
}

UserInfo::Ptr MemcachedSqlDataAccessor::userInfo(UserId user_id)
{
    QString key = QString("user:%1").arg(user_id);
    auto info = cache_get<UserInfo>(key);
    if (!info)
    {
        info = DirectSqlDataAccessor::userInfo(user_id);
        if (info)
            cache_put(key, info, 100);
    }
    return info;
}

bool MemcachedSqlDataAccessor::updateNonce(const ApiKey &key, quint32 nonce)
{

}
