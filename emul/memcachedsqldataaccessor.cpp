#include "memcachedsqldataaccessor.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QFileInfo>
#include <QSettings>

#include <iostream>

MemcachedSqlDataAccessor::MemcachedSqlDataAccessor(QSqlDatabase& db)
    :DirectSqlDataAccessor (db)
{
    memcached_return rc;
    memc = memcached_create(nullptr);

    // TODO: pass settings from main programm!
    QString iniFilePath = QCoreApplication::applicationDirPath() + "/../data/emul.ini";
    if (!QFileInfo(iniFilePath).exists())
    {
        std::cerr << "*** No INI file!" << std::endl;
    }
    QSettings settings(iniFilePath, QSettings::IniFormat);
    QStringList serverList = settings.value("memcached/servers", "localhost:11211").toStringList();
    for (const QString& server: serverList)
    {
        QString hostname;
        int port = 11211;
        QStringList spl = server.split(':');
        if (spl.length() > 0)
        {
            hostname = spl[0];
            if (spl.length()>1)
                port = spl[1].toUInt();

            std::clog << "memcached server " << qPrintable(hostname) << ':' << port << std::endl;
            servers = memcached_server_list_append(servers, hostname.toUtf8().constData(), port, &rc);
        }
    }

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
    memcached_server_free(servers);
}

template <class INFO>
typename INFO::Ptr MemcachedSqlDataAccessor::cache_get(const QByteArray& key)
{
    typename INFO::Ptr info;
    memcached_return rc;
    char* data = nullptr;
    size_t value_length;
    uint32_t flags;
    data = memcached_get(memc, key.constData(), key.length(), &value_length, &flags, &rc);
    if (rc == MEMCACHED_SUCCESS)
    {
        QByteArray value;
        value.setRawData(data, value_length);
        info = std::make_shared<INFO>();
        info->unpack(value);
    }
    free(data);
    return info;
}

template<class INFOPtr>
void MemcachedSqlDataAccessor::cache_put(const QByteArray& key, const INFOPtr &info, int timeout)
{
    QByteArray value = info->pack();
    memcached_set(memc, key.constData(), key.length(), value.constData(), value.length(), timeout, 0);
}

PairInfo::List MemcachedSqlDataAccessor::allPairsInfoList()
{
    PairInfo::List ret = DirectSqlDataAccessor::allPairsInfoList();
    for(const PairInfo::Ptr& info : ret)
    {
        QByteArray key = QString("pair:%1").arg(info->pair).toUtf8();
        cache_put(key, info, 100);
    }
    return ret;
}

PairInfo::Ptr MemcachedSqlDataAccessor::pairInfo(const PairName &pair)
{
    QByteArray key = QString("pair:%1").arg(pair).toUtf8();
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
    QByteArray key = QString("ticker:%1").arg(pair).toUtf8();
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
    QByteArray key = QString("order:%1").arg(order_id).toUtf8();
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
    QByteArray key = QString("apikey:%1").arg(apikey).toUtf8();
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
    QByteArray key = QString("user:%1").arg(user_id).toUtf8();
    auto info = cache_get<UserInfo>(key);
    if (!info)
    {
        info = DirectSqlDataAccessor::userInfo(user_id);
        if (info)
            cache_put(key, info, 100);
    }
    return info;
}

bool MemcachedSqlDataAccessor::tradeUpdateDeposit(const UserId& user_id, const QString& currency, const Amount& diff, const QString& userName)
{
    bool ok = DirectSqlDataAccessor::tradeUpdateDeposit(user_id, currency, diff, userName);
    if (ok)
    {
        QByteArray key = QString("user:%1").arg(user_id).toUtf8();
        auto info = cache_get<UserInfo>(key);
        if (info)
        {
            info->funds[currency] += diff;
            cache_put(key, info, 100);
        }
    }
    return ok;
}

bool MemcachedSqlDataAccessor::reduceOrderAmount(OrderId order_id, const Amount& amount)
{
    bool ok = DirectSqlDataAccessor::reduceOrderAmount(order_id, amount);
    if (ok)
    {
        QByteArray key = QString("order:%1").arg(order_id).toUtf8();
        auto info = cache_get<OrderInfo>(key);
        if (info)
        {
            info->amount -= amount;
            cache_put(key, info, 100);
        }
    }
    return ok;
}

bool MemcachedSqlDataAccessor::closeOrder(OrderId order_id)
{
    bool ok = DirectSqlDataAccessor::closeOrder(order_id);
    if (ok)
    {
        QByteArray key = QString("order:%1").arg(order_id).toUtf8();
        memcached_delete(memc, key.constData(), key.length(), 0);
    }
    return ok;
}

OrderId MemcachedSqlDataAccessor::createNewOrderRecord(const PairName& pair, const UserId& user_id, OrderInfo::Type type, const Rate& rate, const Amount& start_amount)
{
    OrderId id = DirectSqlDataAccessor::createNewOrderRecord(pair, user_id, type, rate, start_amount);
    if (id)
    {
        OrderInfo::Ptr info = std::make_shared<OrderInfo>();
        info->pair = pair;
        info->user_id = user_id;
        info->type = type;
        info->rate = rate;
        info->start_amount = start_amount;
        info->amount = start_amount;
        info->order_id = id;
        info->created = QDateTime::currentDateTime();
        info->status = OrderInfo::Status::Active;

        QByteArray key = QString("order:%1").arg(id).toUtf8();
        cache_put(key, info, 100);
    }
    return id;
}

bool MemcachedSqlDataAccessor::updateNonce(const ApiKey &apikey, quint32 nonce)
{
    bool ok  = DirectSqlDataAccessor::updateNonce(apikey, nonce);
    if (ok)
    {
        QByteArray key = QString("apikey:%1").arg(apikey).toUtf8();
        auto info = cache_get<ApikeyInfo>(key);
        if (info)
        {
            info->nonce = nonce;
            cache_put(key, info, 100);
        }
    }
    return ok;
}
