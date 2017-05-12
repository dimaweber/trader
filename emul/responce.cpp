#include "responce.h"
#include "query_parser.h"
#include "sql_database.h"

#include <QDateTime>
#include <QSqlQuery>

static QVariantMap getInfoResponce(Method& method);
static QVariantMap getTickerResponce(const QueryParser& httpQuery, Method& method);
static QVariantMap getDepthResponce(const QueryParser& httpQuery, Method& method);
static QVariantMap getTradesResponce(const QueryParser& httpQuery, Method& method);

static QVariantMap getPrivateInfoResponce(const QueryParser& httpQuery, Method &method);
static QVariantMap getPrivateActiveOrdersResponce(const QueryParser& httpQuery, Method &method);
static QVariantMap getPrivateOrderInfoResponce(const QueryParser &httpQuery, Method& method);

static std::unique_ptr<Authentificator> auth;
static std::unique_ptr<QSqlQuery> selectTickerInfoQuery;
static std::unique_ptr<QSqlQuery> selectOrderInfoQuery;
static std::unique_ptr<QSqlQuery> selectActiveOrdersQuery;
static std::unique_ptr<QSqlQuery> selectPairInfoQuery;
static std::unique_ptr<QSqlQuery> selectBidsQuery;
static std::unique_ptr<QSqlQuery> selectAsksQuery;
static std::unique_ptr<QSqlQuery> selectAllTradesInfo;
static std::unique_ptr<QSqlQuery> selectFundsInfoQuery;
static std::unique_ptr<QSqlQuery> selectRightsInfoQuery;
static std::unique_ptr<QSqlQuery> selectActiveOrdersCountQuery;
static std::unique_ptr<QSqlQuery> selectCurrencyVolumeQuery;

bool initialiazeResponce(QSqlDatabase& db)
{
    auth = std::make_unique<Authentificator>(db);

    selectTickerInfoQuery.reset( new QSqlQuery(db));
    selectOrderInfoQuery.reset(new QSqlQuery(db));
    selectActiveOrdersQuery.reset(new QSqlQuery(db));
    selectPairInfoQuery.reset( new QSqlQuery(db));
    selectBidsQuery.reset( new QSqlQuery(db));
    selectAsksQuery.reset( new QSqlQuery(db));
    selectAllTradesInfo.reset(new QSqlQuery(db));
    selectFundsInfoQuery.reset(new QSqlQuery(db));
    selectRightsInfoQuery.reset(new QSqlQuery(db));
    selectActiveOrdersCountQuery.reset(new QSqlQuery(db));
    selectCurrencyVolumeQuery.reset(new QSqlQuery(db));

    bool ok =
        selectTickerInfoQuery->prepare("select high, low, avg, vol, vol_cur, last, buy, sell, updated from ticker t left join pairs p on p.pair_id = t.pair_id where p.pair=:name")
    &&  selectPairInfoQuery->prepare("select pair, decimal_places, min_price, max_price, min_amount, hidden, fee from pairs")
    &&  selectOrderInfoQuery->prepare("select p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status+0 from orders o left join apikeys a  on o.owner_id=a.owner_id left join pairs p on p.pair_id = o.pair_id where o.order_id=:order_id")
    &&  selectActiveOrdersQuery->prepare("select o.order_id, p.pair, o.type, o.amount, o.rate, o.created, 0 from orders o left join apikeys a  on o.owner_id=a.owner_id left join pairs p on p.pair_id = o.pair_id where a.apikey=:key and o.status = 'active'")
    &&  selectBidsQuery->prepare("select type, rate, sum(amount), decimal_places from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and type='bids' and p.pair=:name group by pair, type, rate order by pair, type, rate desc")
    &&  selectAsksQuery->prepare("select type, rate, sum(amount), decimal_places from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and type='asks' and p.pair=:name group by pair, type, rate order by pair, type, rate asc")
    &&  selectAllTradesInfo->prepare("select o.type, o.rate, t.amount, t.trade_id, t.created from trades t left join orders o on o.order_id=t.order_id left join pairs p on o.pair_id=p.pair_id where p.pair=:name  order by t.trade_id desc")
    &&  selectFundsInfoQuery->prepare("select c.currency, d.volume from deposits d left join currencies c on c.currency_id=d.currency_id left join apikeys a on a.owner_Id=d.owner_id where apikey=:key")
    &&  selectRightsInfoQuery->prepare("select info,trade,withdraw from apikeys where apikey=:key")
    &&  selectActiveOrdersCountQuery->prepare("select count(*) from apikeys a left join orders o on o.owner_id=a.owner_id where a.apikey=:key and o.status = 0")
    &&  selectCurrencyVolumeQuery->prepare("select d.volume from deposits d left join currencies c on c.currency_id=d.currency_id left join apikeys a on a.owner_Id=d.owner_id where apikey=:key and c.currency=:currency")
        ;

    return ok;
}

bool createTrade(const QString& key, const QString& pair, const QString& type, const QString& rate, const QString& amount)
{
    QVariantMap fundsInfoParams;
    fundsInfoParams[":key"] = key;
    bool isSell = false;
    if (type.toLower() == "buy")
        fundsInfoParams[":currency"] = pair.right(3).toLower();
    else if (type.toLower() == "sell")
    {
        isSell = true;
        fundsInfoParams[":currency"] = pair.left(3).toLower();
    }
    else
        return false;

    if (!performSql("get funds info", *selectCurrencyVolumeQuery, fundsInfoParams, true))
        return false;

    if (!selectFundsInfoQuery->next())
        return false;

    bool ok;
    float currencyAvailable = selectCurrencyVolumeQuery->value(0).toFloat(&ok);
    if (!ok)
        return false;

    float amnt = amount.toFloat(&ok);
    if (!ok)
        return false;

    float rt = rate.toFloat(&ok);
    if (!ok)
        return false;

    if (    isSell && currencyAvailable < amnt
        || !isSell && currencyAvailable < amnt * rt)
        return false;



    return false;
}

QVariantMap getResponce(const QueryParser& parser, Method& method)
{
    QString methodName = parser.method();
    QVariantMap var;

    QueryParser::Scope scope = parser.apiScope();
    if (scope == QueryParser::Scope::Public)
    {
        if (methodName == "info")
        {
            return getInfoResponce( method);
        }

        if (methodName == "ticker" )
        {
            return getTickerResponce(parser, method);
        }

        if (methodName == "depth")
        {
            return getDepthResponce(parser, method);
        }

        if (methodName == "trades")
        {
            return getTradesResponce(parser, method);
        }
    }
    else if (scope == QueryParser::Scope::Private)
    {

        QString authErrMsg;
        QString key = parser.key();

        QStringList methodsRequresInfo = {"getInfo", "ActiveOrders", "OrderInfo", "TradeHistory", "TransHistory", "CoinDepositAddress",};
        QStringList methodsRequresTrade = {"Trade", "CancelOrder"};
        QStringList methodsRequresWithdraw = {"WithdrawCoin", "CreateCupon",  "RedeemCupon"};

        if (!auth->authOk(key, parser.sign(), parser.nonce(), parser.signedData(), authErrMsg))
        {
            var["success"] = 0;
            var["error"] = authErrMsg;
            method = Invalid;
            return var;
        }

        if (methodsRequresInfo.contains(methodName) && !auth->hasInfo(key))
        {
            var["success"] = 0;
            var["error"] = "api key dont have info permission";
            method = Invalid;
            return var;
        }
        else if (methodsRequresTrade.contains(methodName) && !auth->hasTrade(key))
        {
            var["success"] = 0;
            var["error"] = "api key dont have trade permission";
            method = Invalid;
            return var;
        }
        else if (methodsRequresWithdraw.contains(methodName) && !auth->hasWithdraw(key))
        {
            var["success"] = 0;
            var["error"] = "api key dont have withdraw permission";
            method = Invalid;
            return var;
        }

        if (methodName == "getInfo")
        {
            return getPrivateInfoResponce(parser, method);
        }

        if (methodName == "ActiveOrders")
        {
            return getPrivateActiveOrdersResponce(parser, method);
        }

        if (methodName == "PrivateTrade")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
            return var;
        }

        if (methodName == "PrivateOrderInfo")
        {
            return getPrivateOrderInfoResponce( parser, method);
        }

        if (methodName == "PrivateCanelOrder")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
            return var;
        }

        if (methodName == "PrivateTradeHistory")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
            return var;
        }

        if (methodName == "PrivateTransHistory")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
            return var;
        }

        if (methodName == "PrivateCoinDepositAddress")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
            return var;
        }

        if (methodName == "PrivateWithdrawCoin")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
            return var;
        }

        if (methodName == "PrivateCreateCupon")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
            return var;
        }

        if (methodName == "PrivateRedeemCupon")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
            return var;
        }
    }

    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Invalid method";
        method = Invalid;
    }

    return var;
}

QVariantMap getInfoResponce(Method &method)
{
    method = Method::PublicInfo;
    QVariantMap var;
    QVariantMap pairs;

    var["server_time"] = QDateTime::currentDateTime().toTime_t();
    if (performSql("get info", *selectPairInfoQuery, QVariantMap(), true))
    {
        while (selectPairInfoQuery->next())
        {
            QVariantMap pair;
            pair["decimal_places"] = selectPairInfoQuery->value(1).toInt();
            pair["min_price"] = selectPairInfoQuery->value(2).toFloat();
            pair["max_price"] = selectPairInfoQuery->value(3).toFloat();
            pair["min_amount"] = selectPairInfoQuery->value(4).toFloat();
            pair["hidden"] = selectPairInfoQuery->value(5).toInt();
            pair["fee"] = selectPairInfoQuery->value(6).toFloat();

            pairs[selectPairInfoQuery->value(0).toString()] = pair;
        }
    }
    var["pairs"] = pairs;
    return var;
}

QVariantMap getTickerResponce(const QueryParser& httpQuery, Method& method)
{
    method = Method::PublicTicker;
    QVariantMap var;
    QVariantMap tickerParams;

    for (const QString& pairName: httpQuery.pairs())
    {
        if (pairName.isEmpty())
            continue;
        if (var.contains(pairName))
        {
            var.clear();
            var["success"] = 0;
            var["error"] = "Duplicated pair name: " + pairName;
            break;
        }
        tickerParams[":name"] = pairName;
        if (performSql("get ticker", *selectTickerInfoQuery, tickerParams, true))
        {
            if (selectTickerInfoQuery->next())
            {
                QVariantMap pair;
                pair["high"] = selectTickerInfoQuery->value(0);
                pair["low"] = selectTickerInfoQuery->value(1);
                pair["avg"] = selectTickerInfoQuery->value(2);
                pair["vol"] = selectTickerInfoQuery->value(3);
                pair["vol_cur"] = selectTickerInfoQuery->value(4);
                pair["last"] = selectTickerInfoQuery->value(5);
                pair["buy"] = selectTickerInfoQuery->value(6);
                pair["sell"] = selectTickerInfoQuery->value(7);
                pair["updated"] = selectTickerInfoQuery->value(8);

                var[pairName] = pair;
            }
            else
            {
                if (!httpQuery.ignoreInvalid())
                {
                    var.clear();
                    var["success"] = 0;
                    var["error"] = "Invalid pair name: " + pairName;
                    break;
                }
            }
        }
    }
    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Empty pair list";
    }
    return var;
}

static void appendDepthToMap(QVariantMap& var, QSqlQuery& query, const QString& pairName, int limit)
{
    QVariantMap params;
    params[":name"] = pairName;
    if (performSql("append depth", query, params, true))
    {
        while(query.next())
        {
            QString type = query.value(0).toString();

            QVariantMap mapPair;
            if (var.contains(pairName))
                mapPair = var[pairName].toMap();
            QVariantList list;
            if (mapPair.contains(type))
                list = mapPair[type].toList();
            if (list.size() >= limit)
                break;
            QVariantList values;
            values << query.value(1) << query.value(2);
            QVariant castedValue = values;
            list << castedValue;
            mapPair[type] = list;
            var[pairName] = mapPair;
        }
    }
}

QVariantMap getDepthResponce(const QueryParser& httpQuery, Method& method)
{
    method = Method::PublicDepth;
    QVariantMap var;
    int limit = httpQuery.limit();
    for (const QString& pairName: httpQuery.pairs())
    {
        if (pairName.isEmpty())
            continue;
        if (var.contains(pairName))
        {
            var.clear();
            var["success"] = 0;
            var["error"] = "Duplicated pair name: " + pairName;
            break;
        }
        appendDepthToMap(var, *selectBidsQuery, pairName, limit);
        appendDepthToMap(var, *selectAsksQuery, pairName, limit);
    }
    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Empty pair list";
    }
    return var;
}

QVariantMap getTradesResponce(const QueryParser &httpQuery, Method &method)
{
    method = Method::PublicTrades;
    QVariantMap var;
    int limit = httpQuery.limit();
    for (const QString& pairName: httpQuery.pairs())
    {
        if (pairName.isEmpty())
            continue;
        if (var.contains(pairName))
        {
            var.clear();
            var["success"] = 0;
            var["error"] = "Duplicated pair name: " + pairName;
            break;
        }
        QVariantMap tradesParams;
        tradesParams[":name"] = pairName;
        if (performSql("select trades", *selectAllTradesInfo, tradesParams, true))
        {
            QVariantList list;
            QVariantMap tr;
            QString type;
            while(selectAllTradesInfo->next())
            {

                if (list.size() >= limit)
                    break;

                if (selectAllTradesInfo->value(0).toString() == "asks")
                    type = "bid";
                else
                    type = "ask";

                tr["type"] = type;
                tr["price"] = selectAllTradesInfo->value(1);
                tr["amount"] = selectAllTradesInfo->value(2);
                tr["tid"] = selectAllTradesInfo->value(3);
                tr["timestamp"] = selectAllTradesInfo->value(4).toDateTime().toTime_t();

                list << tr;
            }
            var[pairName] = list;
        }
    }
    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Empty pair list";
    }
    return var;
}

QVariantMap getPrivateInfoResponce(const QueryParser &httpQuery, Method& method)
{
    method = Method::PrivateGetInfo;
    QVariantMap var;
    QVariantMap funds;
    QVariantMap rights;
    QVariantMap result;


    QVariantMap params;
    params[":key"] = httpQuery.key();
    if (performSql("get funds", *selectFundsInfoQuery, params, true))
    {
        while(selectFundsInfoQuery->next())
        {
            QString currency = selectFundsInfoQuery->value(0).toString();
            funds[currency] = selectFundsInfoQuery->value(1);
        }
        result["funds"] = funds;
    }
    if (performSql("get rights", *selectRightsInfoQuery, params, true))
    {
        if (selectRightsInfoQuery->next())
        {
            rights["info"] = selectRightsInfoQuery->value(0).toInt();
            rights["trade"] = selectRightsInfoQuery->value(1).toInt();
            rights["withdraw"] = selectRightsInfoQuery->value(2).toInt();
        }
        result["rights"] = rights;
    }
    if (performSql("get orders count", *selectActiveOrdersCountQuery, params, true))
    {
        if (selectActiveOrdersCountQuery->next())
            result["open_orders"] = selectActiveOrdersCountQuery->value(0).toUInt();
    }
    if (result.contains("rights") && result.contains("funds") && result.contains("open_orders"))
    {
        result["transaction_count"] = 0;
        result["server_time"] = QDateTime::currentDateTime().toTime_t();
        var["result"] = result;
        var["success"] = 1;
    }
    else
        var.clear();


    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Fail to provide info";
    }
    return var;
}

QVariantMap getPrivateActiveOrdersResponce(const QueryParser &httpQuery, Method& method)
{
    method = Method::PrivateActiveOrders;
    QVariantMap var;

    QVariantMap params;
    params[":key"] = httpQuery.key();
    QVariantMap result;
    if (performSql("get active orders", *selectActiveOrdersQuery, params, true))
    {
        while(selectActiveOrdersQuery->next())
        {
            QString order_id = selectActiveOrdersQuery->value(0).toString();
            QVariantMap order;
            order["pair"] = selectActiveOrdersQuery->value(1);
            QString type = selectActiveOrdersQuery->value(2).toString();
            order["type"] = (type=="bids")?"buy":"sell";
            order["amount"] = selectActiveOrdersQuery->value(3);
            order["rate"] = selectActiveOrdersQuery->value(4);
            order["timestamp_created"] = selectActiveOrdersQuery->value(5).toDateTime().toTime_t();
            order["status"] = selectActiveOrdersQuery->value(6);

            result[order_id] = order;
        }
        var["result"] = result;
        var["success"] = 1;
    }

    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Fail to provide info";
    }
    return var;
}

QVariantMap getPrivateOrderInfoResponce(const QueryParser &httpQuery, Method& method)
{
    method = Method::PrivateActiveOrders;
    QVariantMap var;

    QVariantMap params;
    QString order_id = httpQuery.order_id();
    params[":order_id"] = order_id;
    QVariantMap result;
    if (performSql("get order info", *selectOrderInfoQuery, params, true))
    {
        if(selectOrderInfoQuery->next())
        {
            QVariantMap order;
            order["pair"] = selectOrderInfoQuery->value(0);
            QString type = selectOrderInfoQuery->value(1).toString();
            order["type"] = (type=="bids")?"buy":"sell";
            order["amount"] = selectOrderInfoQuery->value(2);
            order["rate"] = selectOrderInfoQuery->value(3);
            order["timestamp_created"] = selectOrderInfoQuery->value(4).toDateTime().toTime_t();
            order["status"] = selectOrderInfoQuery->value(5);

            result[order_id] = order;
        }
        var["result"] = result;
        var["success"] = 1;
    }

    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Fail to provide info";
    }
    return var;
}
