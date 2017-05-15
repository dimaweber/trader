#ifndef RESPONCE_H
#define RESPONCE_H

#include "decimal.h"
#include <QCache>
#include <QVariantMap>

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

    enum Method {Invalid, AuthIssue, AccessIssue,
                 PublicInfo, PublicTicker, PublicDepth, PublicTrades,
                 PrivateGetInfo, PrivateTrade, PrivateActiveOrders, PrivateOrderInfo,
                 PrivateCanelOrder, PrivateTradeHistory, PrivateTransHistory,
                 PrivateCoinDepositAddress,
                 PrivateWithdrawCoin, PrivateCreateCupon, PrivateRedeemCupon
                };

    static QAtomicInt counter;
    Responce(QSqlDatabase& database);

    QVariantMap getResponce(const QueryParser& parser, Method& method);

    QVariantMap exchangeBalance();

    static QString oppositOrderType(const QString& type);
private:
    QSqlDatabase& db;

    QVariantMap getInfoResponce(Method& method);
    QVariantMap getTickerResponce(const QueryParser& httpQuery, Method& method);
    QVariantMap getDepthResponce(const QueryParser& httpQuery, Method& method);
    QVariantMap getTradesResponce(const QueryParser& httpQuery, Method& method);

    QVariantMap getPrivateInfoResponce(const QueryParser& httpQuery, Method &method);
    QVariantMap getPrivateActiveOrdersResponce(const QueryParser& httpQuery, Method &method);
    QVariantMap getPrivateOrderInfoResponce(const QueryParser &httpQuery, Method& method);

    QVariantMap getPrivateTradeResponce(const QueryParser& httpQuery, Method& method);
    QVariantMap getPrivateCalcelOrderResponce(const QueryParser& httpQuery, Method& method);

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
    TradeCurrencyVolume trade_volumes (const QString& type, const QString& pair, Fee fee,
                                     Amount trade_amount, Rate matched_order_rate);
    NewOrderVolume new_order_currency_volume (const QString& type, const QString& pair, Amount amount, Rate rate);
    quint32 doExchange(QString ownerName, const QString& rate, TradeCurrencyVolume volumes, const QString& type, Rate rt, const QString& pair, QSqlQuery& query, Amount& amnt, Fee fee, QVariant owner_id, QVariant pair_id, QVariantMap orderCreateParams);
    OrderCreateResult createTrade(const QString& key, const QString& pair, const QString& type, const QString& rate, const QString& amount);
    QVariantList appendDepthToMap(QSqlQuery& query, const QString& pairName, int limit);

    std::unique_ptr<Authentificator> auth;
    std::unique_ptr<QSqlQuery> selectTickerInfoQuery;
    std::unique_ptr<QSqlQuery> selectOrderInfoQuery;
    std::unique_ptr<QSqlQuery> selectActiveOrdersQuery;
    std::unique_ptr<QSqlQuery> selectPairsInfoQuery;
    std::unique_ptr<QSqlQuery> selectBuyOrdersQuery;
    std::unique_ptr<QSqlQuery> selectSellOrdersQuery;
    std::unique_ptr<QSqlQuery> selectAllTradesInfo;
    std::unique_ptr<QSqlQuery> selectFundsInfoQuery;
    std::unique_ptr<QSqlQuery> selectFundsInfoQueryByOwnerId;
    std::unique_ptr<QSqlQuery> selectRightsInfoQuery;
    std::unique_ptr<QSqlQuery> selectActiveOrdersCountQuery;
    std::unique_ptr<QSqlQuery> selectCurrencyVolumeQuery;
    std::unique_ptr<QSqlQuery> selectOrdersForSellTrade;
    std::unique_ptr<QSqlQuery> selectOrdersForBuyTrade;
    std::unique_ptr<QSqlQuery> selectOwnerForKeyQuery;
    std::unique_ptr<QSqlQuery> updateDepositQuery;
    std::unique_ptr<QSqlQuery> updateOrderAmount;
    std::unique_ptr<QSqlQuery> updateOrderDone;
    std::unique_ptr<QSqlQuery> createTradeQuery;
    std::unique_ptr<QSqlQuery> createOrderQuery;
    std::unique_ptr<QSqlQuery> cancelOrderQuery;
    std::unique_ptr<QSqlQuery> startTransaction;
    std::unique_ptr<QSqlQuery> commitTransaction;
    std::unique_ptr<QSqlQuery> rollbackTransaction;
    std::unique_ptr<QSqlQuery> totalBalance;


    struct PairInfo {
        Rate min_price;
        Rate max_price;
        Rate min_amount;
        Fee fee;
        PairId pair_id;
        int decimal_places;
    };
    static QCache<QString, PairInfo> pairInfoCache;
    PairInfo* pairInfo(const QString& pair);
};

#endif // RESPONCE_H
