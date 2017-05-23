#ifndef MEMCACHEDSQLDATAACCESSOR_H
#define MEMCACHEDSQLDATAACCESSOR_H

#include "sqlclient.h"
#include <libmemcached/memcached.h>

class MemcachedSqlDataAccessor : public DirectSqlDataAccessor
{
    memcached_server_st* servers = nullptr;
    memcached_st* memc = nullptr;
public:
    MemcachedSqlDataAccessor(QSqlDatabase& db);
    virtual ~MemcachedSqlDataAccessor();

    virtual PairInfo::List   allPairsInfoList() override;
    virtual PairInfo::Ptr    pairInfo(const PairName& pair) override;
    virtual TickerInfo::Ptr  tickerInfo(const PairName& pair) override;
    virtual OrderInfo::Ptr   orderInfo(OrderId order_id) override;
    virtual ApikeyInfo::Ptr  apikeyInfo(const ApiKey& apikey) override;
    virtual UserInfo::Ptr    userInfo(UserId user_id) override;

//    virtual bool tradeUpdateDeposit(const UserId &user_id, const QString& currency, const Amount& diff, const QString& userName) override;
//    virtual bool reduceOrderAmount(OrderId order_id, const Amount& amount) override;
//    virtual bool closeOrder(OrderId order_id) override;
//    virtual OrderId createNewOrderRecord(const PairName& pair, const UserId& user_id, OrderInfo::Type type, const Rate& rate, const Amount& start_amount) override;

    virtual bool       updateNonce(const ApiKey& key, quint32 nonce) override;

    //    virtual bool rollback() override;
private:
    template<class INFO>
    typename INFO::Ptr cache_get(const QString &key);

    template<class INFOPtr>
    void cache_put(const QString &key, const INFOPtr& info, int timeout=100);
};

#endif // MEMCACHEDSQLDATAACCESSOR_H
