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

#define EXCHNAGE_USER_ID 1000

class Responce
{
public:
    using Amount = cppdecimal::decimal<7>;
    using Fee    = cppdecimal::decimal<7>;
    using Rate   = cppdecimal::decimal<7>;
    using PairId = quint32;
    using UserId = quint32;
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
        UserId user_id;
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
        UserId user_id;
        OrderInfo::WPtr order_ptr;
    };

    using DepthItem = QPair<Responce::Rate, Responce::Amount>;
    using Depth = QList<DepthItem>;
    using BuySellDepth = QPair<Depth, Depth>;
    using Funds = QMap<QString, Amount>;

    struct UserInfo
    {
        using Ptr = std::shared_ptr<UserInfo>;
        using WPtr = std::weak_ptr<UserInfo>;

        UserId user_id;
        QString name;
        Funds funds;

        void updateDeposit(const QString& currency, Amount value);
    };

    struct ApikeyInfo
    {
        using Ptr = std::shared_ptr<ApikeyInfo>;

        ApiKey apikey;
        bool info;
        bool trade;
        bool withdraw;
        UserId user_id;
        UserInfo::WPtr user_ptr;
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
        QString  goods;
        QString  currency;
        QString  trader_currency_in;
        Amount   trader_volume_in;
        QString  trader_currency_out;
        Amount   trader_volume_out;
        QString  parter_currency_in;
        Amount   partner_volume_in;
        Amount   exchange_currency_in;
        Amount   exchange_goods_in;
    };

    struct NewOrderVolume
    {
        QString           currency;
        Responce::Amount  volume;
    };

    struct OrderCreateResult
    {
        QString  errMsg;
        Amount   recieved;
        Amount   remains;
        quint32  order_id;
        bool     ok;
    };

    NewOrderVolume new_order_currency_volume (OrderInfo::Type type, const QString& pair, Amount amount, Rate rate);
    QVariantList appendDepthToMap(const Depth& depth, int limit);
    TradeCurrencyVolume trade_volumes (OrderInfo::Type type, const QString& pair, Fee fee,
                                     Amount trade_amount, Rate matched_order_rate);
    quint32 doExchange(QString userName, const Rate& rate, TradeCurrencyVolume volumes, OrderInfo::Type type, Rate rt, const PairName &pair, QSqlQuery& query, Amount& amnt, Fee fee, UserId user_id);
    OrderCreateResult checkParamsAndDoExchange(const ApiKey& key, const PairName& pair, OrderInfo::Type type, const Rate& rate, const Amount& amount);

    std::unique_ptr<Authentificator>  auth;
    std::unique_ptr<QSqlQuery>  selectActiveOrdersCountQuery;
    std::unique_ptr<QSqlQuery>  selectOrdersForSellTrade;
    std::unique_ptr<QSqlQuery>  selectOrdersForBuyTrade;


    std::unique_ptr<QSqlQuery>  cancelOrderQuery;

    std::unique_ptr<QSqlQuery>  startTransaction;
    std::unique_ptr<QSqlQuery>  commitTransaction;
    std::unique_ptr<QSqlQuery>  rollbackTransaction;

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
    static QCache<UserId, UserInfo::Ptr> userInfoCache;
    static QReadWriteLock userInfoCacheRWAccess;

    PairInfo::List   allPairsInfoList();
    PairInfo::Ptr    pairInfo(const QString& pair);
    TickerInfo::Ptr  tickerInfo(const QString& pair);
    OrderInfo::Ptr   orderInfo(OrderId order_id);
    OrderInfo::List  activeOrdersInfoList(const QString& apikey);
    TradeInfo::List  allTradesInfo(const PairName& pair);
    ApikeyInfo::Ptr  apikeyInfo(const ApiKey& apikey);
    UserInfo::Ptr    userInfo(UserId user_id);

    QMap<Responce::PairName, Responce::BuySellDepth> allActiveOrdersAmountAgreggatedByRateList(const QList<PairName>& pairs);
    bool tradeUpdateDeposit(const UserId &user_id, const QString& currency, Amount diff, const QString& userName);
    bool reduceOrderAmount(Responce::OrderId, Amount amount);
    bool closeOrder(Responce::OrderId order_id);
    bool createNewTradeRecord(Responce::UserId user_id, Responce::OrderId order_id, const Amount &amount);
    OrderId createNewOrderRecord(const PairName& pair, const UserId& user_id, OrderInfo::Type type, Rate rate, Amount start_amount);
};

#endif // RESPONCE_H
