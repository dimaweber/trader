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
    using ApiKey = QString;

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
        using WPtr = std::weak_ptr<PairInfo>;
        using List = QList<PairInfo::Ptr>; // WPtr ?

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
        using Ptr = std::shared_ptr<TickerInfo>;

        Rate high;
        Rate low;
        Rate avg;
        Amount vol;
        Amount vol_cur;
        Rate last;
        Rate buy;
        Rate sell;
        QDateTime updated;
        PairName pairName;

        PairInfo::WPtr pair_ptr;
        PairInfo::Ptr pair();
    };
    struct OrderInfo
    {
        enum class Type {Sell, Buy};
        enum class Status {Active=1, Done, Cancelled, PartiallyDone};
        using Ptr = std::shared_ptr<OrderInfo>;
        using WPtr = std::shared_ptr<OrderInfo>;
        using List = QList<OrderInfo::Ptr>; // WPtr ?

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
    struct TradeInfo
    {
        enum class Type {Ask, Bid};
        using Ptr = std::shared_ptr<TradeInfo>;
        using List  = QList<TradeInfo::Ptr>; // Wptr ?

        TradeInfo::Type type;
        Rate rate;
        Amount amount;
        TradeId tid;
        QDateTime created;
        OrderId order_id;
        OwnerId owner_id;
        OrderInfo::WPtr order_ptr;
    };

    using DepthItem = QPair<Responce::Rate, Responce::Amount>;
    using Depth = QList<DepthItem>;
    using BuySellDepth = QPair<Depth, Depth>;
    using Funds = QMap<QString, Amount>;
    using FundsPtr = std::shared_ptr<Funds>;
    using FundsWPtr = std::weak_ptr<Funds>;

    struct OwnerInfo
    {
        using Ptr = std::shared_ptr<OwnerInfo>;
        using WPtr = std::weak_ptr<OwnerInfo>;

        OwnerId owner_id;
        QString name;
        FundsWPtr funds_ptr;

        FundsPtr funds ();
    };

    struct ApikeyInfo
    {
        using Ptr = std::shared_ptr<ApikeyInfo>;

        ApiKey apikey;
        bool info;
        bool trade;
        bool withdraw;
        OwnerId owner_id;
        OwnerInfo::WPtr owner_ptr;

        OwnerInfo::Ptr owner();
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

    std::unique_ptr<Authentificator>  auth;
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
    static QReadWriteLock pairInfoCacheRWAccess;
    static QMap<Responce::PairName, TickerInfo::Ptr> tickerInfoCache;
    static QReadWriteLock tickerInfoCacheRWAccess;
    static QCache<Responce::OrderId, Responce::OrderInfo::Ptr> orderInfoCache;
    static QReadWriteLock orderInfoCacheRWAccess;
    static QCache<Responce::TradeId, Responce::TradeInfo::Ptr> tradeInfoCache;
    static QReadWriteLock tradeInfoCacheRWAccess;
    static QCache<ApiKey, ApikeyInfo::Ptr> apikeyInfoCache;
    static QReadWriteLock apikeyInfoCacheRWAccess;
    static QCache<OwnerId, OwnerInfo::Ptr> ownerInfoCache;
    static QReadWriteLock ownerInfoCacheRWAccess;
    static QCache<OwnerId, FundsPtr> fundsCache;
    static QReadWriteLock fundsCacheRWAccess;

    PairInfo::List   allPairsInfoList();
    PairInfo::Ptr    pairInfo(const QString& pair);
    TickerInfo::Ptr  tickerInfo(const QString& pair);
    OrderInfo::Ptr   orderInfo(OrderId order_id);
    OrderInfo::List  activeOrdersInfoList(const QString& apikey);
    TradeInfo::List  allTradesInfo(const PairName& pair);
    QMap<Responce::PairName, Responce::BuySellDepth> allActiveOrdersAmountAgreggatedByRateList(const QList<PairName>& pairs);
    ApikeyInfo::Ptr  apikeyInfo(const ApiKey& apikey);
    OwnerInfo::Ptr   ownerInfo(OwnerId owner_id);
    FundsPtr         funds(OwnerId owner_id);

    NewOrderVolume new_order_currency_volume (OrderInfo::Type type, const QString& pair, Amount amount, Rate rate);
    QVariantList appendDepthToMap(const Depth& depth, int limit);
};

#endif // RESPONCE_H
