#ifndef RESPONCE_H
#define RESPONCE_H

#include "decimal.h"
#include <QDateTime>
#include <QMap>
#include <QMutex>
#include <QReadWriteLock>
#include <QVariantMap>
#include <QList>

#include <memory>

class Authentificator;
class QueryParser;
class QSqlDatabase;
class QSqlQuery;

#define EXCHNAGE_OWNER_ID 1000

class Responce
{
public:
    using Amount = cppdecimal::decimal<7>;
    using Fee    = cppdecimal::decimal<7>;
    using Rate   = cppdecimal::decimal<7>;
    using PairId = quint32;
    using OwnerId = quint32;
    using TradeId = quint32;
    using OrderId = quint32;
    using PairName = QString;

    enum Method {Invalid, AuthIssue, AccessIssue,
                 PublicInfo, PublicTicker, PublicDepth, PublicTrades,
                 PrivateGetInfo, PrivateTrade, PrivateActiveOrders, PrivateOrderInfo,
                 PrivateCanelOrder, PrivateTradeHistory, PrivateTransHistory,
                 PrivateCoinDepositAddress,
                 PrivateWithdrawCoin, PrivateCreateCupon, PrivateRedeemCupon
                };

    struct PairInfo
    {
        using Ptr = std::shared_ptr<PairInfo>;
        using List = QList<PairInfo::Ptr>;

        Rate min_price;
        Rate max_price;
        Rate min_amount;
        Fee fee;
        PairId pair_id;
        int decimal_places;
        bool hidden;
        PairName pair;
    };
    struct TickerInfo
    {
        // TODO: std::weak_ptr<PairInfo> pair;
        Rate high;
        Rate low;
        Rate avg;
        Amount vol;
        Amount vol_cur;
        Rate last;
        Rate buy;
        Rate sell;
        QDateTime updated;
        typedef std::shared_ptr<TickerInfo> Ptr;
    };
    struct OrderInfo
    {
        enum class Type {Sell, Buy};
        enum class Status {Active=1, Done, Cancelled, PartiallyDone};
        using Ptr = std::shared_ptr<OrderInfo>;
        using List = QList<OrderInfo::Ptr>;

        PairName pair;
        std::weak_ptr<PairInfo> pair_ptr;
        Type type;
        Amount start_amount;
        Amount amount;
        Rate rate;
        QDateTime created;
        Status status;
        OwnerId owner_id;
        OrderId order_id;
    };

    Responce(QSqlDatabase& database);

    QVariantMap getResponce(const QueryParser& parser, Method& method);

    QVariantMap exchangeBalance();

    static OrderInfo::Type oppositOrderType(OrderInfo::Type type);
private:
    static QAtomicInt counter;
    QSqlDatabase& db;

    QVariantMap getInfoResponce(Method& method);
    QVariantMap getTickerResponce(const QueryParser& httpQuery, Method& method);
    QVariantMap getDepthResponce(const QueryParser& httpQuery, Method& method);
    QVariantMap getTradesResponce(const QueryParser& httpQuery, Method& method);

    QVariantMap getPrivateInfoResponce(const QueryParser& httpQuery, Method &method);
    QVariantMap getPrivateActiveOrdersResponce(const QueryParser& httpQuery, Method &method);
    QVariantMap getPrivateOrderInfoResponce(const QueryParser &httpQuery, Method& method);

    QVariantMap getPrivateTradeResponce(const QueryParser& httpQuery, Method& method);
    QVariantMap getPrivateCancelOrderResponce(const QueryParser& httpQuery, Method& method);

    struct TradeCurrencyVolume
    {
        QString goods;
        QString currency;
        QString trader_currency_in;
        Amount   trader_volume_in;
        QString trader_currency_out;
        Amount trader_volume_out;
        QString parter_currency_in;
        Amount partner_volume_in;
        Amount exchange_currency_in;
        Amount exchange_goods_in;
    };

    struct NewOrderVolume
    {
        QString currency;
        Responce::Amount volume;
    };

    struct OrderCreateResult
    {
        QString errMsg;
        Amount recieved;
        Amount remains;
        quint32 order_id;
        bool ok;
    };

    bool tradeUpdateDeposit(const QVariant& owner_id, const QString& currency, Amount diff, const QString& ownerName);
    TradeCurrencyVolume trade_volumes (OrderInfo::Type type, const QString& pair, Fee fee,
                                     Amount trade_amount, Rate matched_order_rate);
    quint32 doExchange(QString ownerName, const QString& rate, TradeCurrencyVolume volumes, OrderInfo::Type type, Rate rt, const QString& pair, QSqlQuery& query, Amount& amnt, Fee fee, QVariant owner_id, QVariant pair_id, QVariantMap orderCreateParams);
    OrderCreateResult createTrade(const QString& key, const QString& pair, OrderInfo::Type type, const QString& rate, const QString& amount);
    QVariantList appendDepthToMap(QSqlQuery& query, const QString& pairName, int limit);

    std::unique_ptr<Authentificator>  auth;
    std::unique_ptr<QSqlQuery>  selectSellOrdersQuery;
    std::unique_ptr<QSqlQuery>  selectAllTradesInfo;
    std::unique_ptr<QSqlQuery>  selectFundsInfoQuery;
    std::unique_ptr<QSqlQuery>  selectFundsInfoQueryByOwnerId;
    std::unique_ptr<QSqlQuery>  selectRightsInfoQuery;
    std::unique_ptr<QSqlQuery>  selectActiveOrdersCountQuery;
    std::unique_ptr<QSqlQuery>  selectCurrencyVolumeQuery;
    std::unique_ptr<QSqlQuery>  selectOrdersForSellTrade;
    std::unique_ptr<QSqlQuery>  selectOrdersForBuyTrade;
    std::unique_ptr<QSqlQuery>  selectOwnerForKeyQuery;
    std::unique_ptr<QSqlQuery>  updateDepositQuery;
    std::unique_ptr<QSqlQuery>  updateOrderAmount;
    std::unique_ptr<QSqlQuery>  updateOrderDone;
    std::unique_ptr<QSqlQuery>  createTradeQuery;
    std::unique_ptr<QSqlQuery>  createOrderQuery;
    std::unique_ptr<QSqlQuery>  cancelOrderQuery;
    std::unique_ptr<QSqlQuery>  startTransaction;
    std::unique_ptr<QSqlQuery>  commitTransaction;
    std::unique_ptr<QSqlQuery>  rollbackTransaction;
    std::unique_ptr<QSqlQuery>  totalBalance;

    static QMap<Responce::PairName, Responce::PairInfo::Ptr> pairInfoCache;
    static QMutex pairInfoCacheAccess;
    static QReadWriteLock pairInfoCacheAccessRW;
    static QMap<Responce::PairName, TickerInfo::Ptr> tickerInfoCache;
    static QMutex tickerInfoCacheAccess;
    static QCache<Responce::OrderId, Responce::OrderInfo::Ptr> orderInfoCache;
    static QMutex orderInfoCacheAccess;

    PairInfo::List allPairsInfoList();
    PairInfo::Ptr    pairInfo(const QString& pair);
    TickerInfo::Ptr  tickerInfo(const QString& pair);
    OrderInfo::Ptr   orderInfo(OrderId order_id);
    OrderInfo::List activeOrdersInfoList(const QString& apikey);
    QMap<Responce::PairName, QList<QPair<Responce::Rate, Responce::Amount>>> allBuyOrdersAmountAgreggatedByRateList(const QList<PairName>& pairs);

    NewOrderVolume new_order_currency_volume (OrderInfo::Type type, const QString& pair, Amount amount, Rate rate);
};

#endif // RESPONCE_H
