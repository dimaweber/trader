#ifndef SQLCLIENT_H
#define SQLCLIENT_H

#include "types.h"

#include <QtCore/qglobal.h>
#include <QMutex>

#include <memory>

class QSqlDatabase;

class AbstractDataAccessor
{
public:
    virtual PairInfo::List   allPairsInfoList() = 0;
    virtual PairInfo::Ptr    pairInfo(const PairName& pair) =0;
    virtual TickerInfo::Ptr  tickerInfo(const PairName& pair) =0;
    virtual OrderInfo::Ptr   orderInfo(OrderId order_id) =0;
    virtual OrderInfo::List  activeOrdersInfoList(const QString& apikey) =0;
    virtual TradeInfo::List  allTradesInfo(const PairName& pair) =0;
    virtual ApikeyInfo::Ptr  apikeyInfo(const ApiKey& apikey) =0;
    virtual UserInfo::Ptr    userInfo(UserId user_id) =0;

    virtual QMap<PairName, BuySellDepth> allActiveOrdersAmountAgreggatedByRateList(const QList<PairName>& pairs) =0;
    virtual bool tradeUpdateDeposit(const UserId &user_id, const QString& currency, const Amount &diff, const QString& userName) =0;
    virtual bool reduceOrderAmount(OrderId, const Amount& amount) =0;
    virtual bool closeOrder(OrderId order_id) =0;
    virtual bool createNewTradeRecord(UserId user_id, OrderId order_id, const Amount& amount) =0;
    virtual OrderId createNewOrderRecord(const PairName& pair, const UserId& user_id, OrderInfo::Type type, const Rate& rate, const Amount& start_amount) =0;

    virtual QByteArray randomKeyWithPermissions(bool info, bool trade, bool withdraw) =0;
    virtual QByteArray randomKeyForTrade(const QString& currency, const Amount& amount) =0;
    virtual QByteArray signWithKey(const QByteArray& message, const ApiKey& key) =0;
    virtual QByteArray secretForKey(const ApiKey& key) =0;
    virtual bool updateNonce(const ApiKey& key, quint32 nonce) = 0;


    virtual Amount getDepositCurrencyVolume(const ApiKey& key, const QString& currency) =0;
    virtual Amount  getOrdersCurrencyVolume(const ApiKey& key, const QString& currency) =0;

    virtual OrderInfo::List negativeAmountOrders() = 0;

    virtual void updateTicker() = 0;

    virtual bool transaction() =0;
    virtual bool commit() =0;
    virtual bool rollback() =0;
};

class DirectSqlDataAccessor : public AbstractDataAccessor
{
    QSqlDatabase& db;
public :
    DirectSqlDataAccessor(QSqlDatabase& db);

    PairInfo::List   allPairsInfoList() override;
    PairInfo::Ptr    pairInfo(const PairName& pair) override;
    TickerInfo::Ptr  tickerInfo(const PairName& pair) override;
    OrderInfo::Ptr   orderInfo(OrderId order_id) override;
    OrderInfo::List  activeOrdersInfoList(const QString& apikey) override;
    TradeInfo::List  allTradesInfo(const PairName& pair) override;
    ApikeyInfo::Ptr  apikeyInfo(const ApiKey& apikey) override;
    UserInfo::Ptr    userInfo(UserId user_id) override;

    QMap<PairName, BuySellDepth> allActiveOrdersAmountAgreggatedByRateList(const QList<PairName>& pairs) override;
    bool tradeUpdateDeposit(const UserId &user_id, const QString& currency, const Amount& diff, const QString& userName) override;
    bool reduceOrderAmount(OrderId order_id, const Amount& amount) override;
    bool closeOrder(OrderId order_id) override;
    bool createNewTradeRecord(UserId user_id, OrderId order_id, const Amount& amount) override;
    OrderId createNewOrderRecord(const PairName& pair, const UserId& user_id, OrderInfo::Type type, const Rate& rate, const Amount& start_amount) override;

    virtual QByteArray randomKeyWithPermissions(bool info, bool trade, bool withdraw) override;
    virtual QByteArray randomKeyForTrade(const QString& currency, const Amount& amount) override;
    virtual QByteArray signWithKey(const QByteArray& message, const ApiKey& key) override;
    virtual QByteArray secretForKey(const ApiKey& key) override;
    virtual bool       updateNonce(const ApiKey& key, quint32 nonce) override;

    virtual Amount getDepositCurrencyVolume(const ApiKey& key, const QString& currency) override;
    virtual Amount  getOrdersCurrencyVolume(const ApiKey& key, const QString& currency) override;

    virtual OrderInfo::List negativeAmountOrders() override;

    virtual void updateTicker() override;

    virtual bool transaction() override;
    virtual bool commit()      override;
    virtual bool rollback()    override;
};

class LocalCachesSqlDataAccessor : public DirectSqlDataAccessor
{
    static QMap<PairName,  PairInfo::Ptr>    pairInfoCache;
    static QMap<PairName,  TickerInfo::Ptr>  tickerInfoCache;
    static QCache<OrderId, OrderInfo::Ptr>   orderInfoCache;
    static QCache<TradeId, TradeInfo::Ptr>   tradeInfoCache;
    static QCache<ApiKey,  ApikeyInfo::Ptr>  apikeyInfoCache;
    static QCache<UserId,  UserInfo::Ptr>    userInfoCache;

    static QMutex  pairInfoCacheRWAccess;
    static QMutex  tickerInfoCacheRWAccess;
    static QMutex  orderInfoCacheRWAccess;
    static QMutex  tradeInfoCacheRWAccess;
    static QMutex  apikeyInfoCacheRWAccess;
    static QMutex  userInfoCacheRWAccess;

public :
    LocalCachesSqlDataAccessor(QSqlDatabase& db);

    PairInfo::List   allPairsInfoList() override;
    PairInfo::Ptr    pairInfo(const PairName& pair) override;
    TickerInfo::Ptr  tickerInfo(const PairName& pair) override;
    OrderInfo::Ptr   orderInfo(OrderId order_id) override;
    ApikeyInfo::Ptr  apikeyInfo(const ApiKey& apikey) override;
    UserInfo::Ptr    userInfo(UserId user_id) override;

    bool tradeUpdateDeposit(const UserId &user_id, const QString& currency, const Amount& diff, const QString& userName) override;
    bool reduceOrderAmount(OrderId order_id, const Amount& amount) override;
    bool closeOrder(OrderId order_id) override;
    OrderId createNewOrderRecord(const PairName& pair, const UserId& user_id, OrderInfo::Type type, const Rate& rate, const Amount& start_amount) override;

    virtual bool       updateNonce(const ApiKey& key, quint32 nonce) override;

    virtual bool rollback() override;
};

#endif // SQLCLIENT_H
