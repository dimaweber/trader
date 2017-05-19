#ifndef RESPONCE_H
#define RESPONCE_H

#include "types.h"
#include "sqlclient.h"

#include <QDateTime>
#include <QMap>
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
    Responce(QSqlDatabase& database);

    QVariantMap getResponce(const QueryParser& parser, Method& method);

    QVariantMap exchangeBalance();
    OrderInfo::List negativeAmountOrders();
    void updateTicker();

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
        Amount  volume;
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
    QVariantList appendDepthToMap(const Depth& depth, int limit, int dp);
    TradeCurrencyVolume trade_volumes (OrderInfo::Type type, const QString& pair, Fee fee,
                                     Amount trade_amount, Rate matched_order_rate);
    quint32 doExchange(QString userName, const Rate& rate, TradeCurrencyVolume volumes, OrderInfo::Type type, Rate rt, const PairName &pair, QSqlQuery& query, Amount& amnt, Fee fee, UserId user_id);
    OrderCreateResult checkParamsAndDoExchange(const ApiKey& key, const PairName& pair, OrderInfo::Type type, const Rate& rate, const Amount& amount);

    std::unique_ptr<Authentificator>  auth;
    std::unique_ptr<QSqlQuery>  selectActiveOrdersCountQuery;


    std::unique_ptr<QSqlQuery>  cancelOrderQuery;

    std::unique_ptr<QSqlQuery>  startTransaction;
    std::unique_ptr<QSqlQuery>  commitTransaction;
    std::unique_ptr<QSqlQuery>  rollbackTransaction;

    std::shared_ptr<AbstractDataAccessor> dataAccessor;
};

#endif // RESPONCE_H
