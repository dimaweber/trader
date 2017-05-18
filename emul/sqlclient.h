#ifndef SQLCLIENT_H
#define SQLCLIENT_H

#include "types.h"

#include <QtCore/qglobal.h>
#include <QSqlQuery>

#include <memory>

class QSqlDatabase;

class AbstractDataAccessor
{
public:
    virtual PairInfo::List   allPairsInfoList() = 0;
    virtual PairInfo::Ptr    pairInfo(const QString& pair) =0;
    virtual TickerInfo::Ptr  tickerInfo(const QString& pair) =0;
    virtual OrderInfo::Ptr   orderInfo(OrderId order_id) =0;
    virtual OrderInfo::List  activeOrdersInfoList(const QString& apikey) =0;
    virtual TradeInfo::List  allTradesInfo(const PairName& pair) =0;
    virtual ApikeyInfo::Ptr  apikeyInfo(const ApiKey& apikey) =0;
    virtual UserInfo::Ptr    userInfo(UserId user_id) =0;

    virtual QMap<PairName, BuySellDepth> allActiveOrdersAmountAgreggatedByRateList(const QList<PairName>& pairs) =0;
    virtual bool tradeUpdateDeposit(const UserId &user_id, const QString& currency, Amount diff, const QString& userName) =0;
    virtual bool reduceOrderAmount(OrderId, Amount amount) =0;
    virtual bool closeOrder(OrderId order_id) =0;
    virtual bool createNewTradeRecord(UserId user_id, OrderId order_id, const Amount &amount) =0;
    virtual OrderId createNewOrderRecord(const PairName& pair, const UserId& user_id, OrderInfo::Type type, Rate rate, Amount start_amount) =0;

    virtual QByteArray randomKeyWithPermissions(bool info, bool trade, bool withdraw) =0;
    virtual QByteArray randomKeyForTrade(const QString& currency, Amount amount) =0;
    virtual QByteArray signWithKey(const QByteArray& message, const QByteArray& key) =0;
    virtual QByteArray secretForKey(const QByteArray& key) =0;
    virtual bool updateNonce(const ApiKey& key, quint32 nonce) = 0;


    virtual Amount getDepositCurrencyVolume(const QByteArray& key, const QString& currency) =0;
    virtual Amount  getOrdersCurrencyVolume(const QByteArray& key, const QString& currency) =0;
};

class DirectSqlDataAccessor : public AbstractDataAccessor
{
    QSqlDatabase& db;
public :
    DirectSqlDataAccessor(QSqlDatabase& db);

    PairInfo::List   allPairsInfoList() override;
    PairInfo::Ptr    pairInfo(const QString& pair) override;
    TickerInfo::Ptr  tickerInfo(const QString& pair) override;
    OrderInfo::Ptr   orderInfo(OrderId order_id) override;
    OrderInfo::List  activeOrdersInfoList(const QString& apikey) override;
    TradeInfo::List  allTradesInfo(const PairName& pair) override;
    ApikeyInfo::Ptr  apikeyInfo(const ApiKey& apikey) override;
    UserInfo::Ptr    userInfo(UserId user_id) override;

    QMap<PairName, BuySellDepth> allActiveOrdersAmountAgreggatedByRateList(const QList<PairName>& pairs) override;
    bool tradeUpdateDeposit(const UserId &user_id, const QString& currency, Amount diff, const QString& userName) override;
    bool reduceOrderAmount(OrderId order_id, Amount amount) override;
    bool closeOrder(OrderId order_id) override;
    bool createNewTradeRecord(UserId user_id, OrderId order_id, const Amount &amount) override;
    OrderId createNewOrderRecord(const PairName& pair, const UserId& user_id, OrderInfo::Type type, Rate rate, Amount start_amount) override;

    virtual QByteArray randomKeyWithPermissions(bool info, bool trade, bool withdraw) override;
    virtual QByteArray randomKeyForTrade(const QString& currency, Amount amount) override;
    virtual QByteArray signWithKey(const QByteArray& message, const QByteArray& key) override;
    virtual QByteArray secretForKey(const QByteArray& key) override;
    virtual bool       updateNonce(const ApiKey& key, quint32 nonce) override;

    virtual Amount getDepositCurrencyVolume(const QByteArray& key, const QString& currency) override;
    virtual Amount  getOrdersCurrencyVolume(const QByteArray& key, const QString& currency) override;
};


#endif // SQLCLIENT_H
