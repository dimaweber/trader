#include "responce.h"
#include "query_parser.h"
#include "sql_database.h"
#include "utils.h"

#include <QDateTime>
#include <QSqlQuery>

#define EXCHNAGE_OWNER_ID 1000

QVariantMap getInfoResponce(Method& method);
QVariantMap getTickerResponce(const QueryParser& httpQuery, Method& method);
QVariantMap getDepthResponce(const QueryParser& httpQuery, Method& method);
QVariantMap getTradesResponce(const QueryParser& httpQuery, Method& method);

QVariantMap getPrivateInfoResponce(const QueryParser& httpQuery, Method &method);
QVariantMap getPrivateActiveOrdersResponce(const QueryParser& httpQuery, Method &method);
QVariantMap getPrivateOrderInfoResponce(const QueryParser &httpQuery, Method& method);

QVariantMap privateTrade(const QueryParser& httpQuery, Method& method);

static std::unique_ptr<Authentificator> auth;
static std::unique_ptr<QSqlQuery> selectTickerInfoQuery;
static std::unique_ptr<QSqlQuery> selectOrderInfoQuery;
static std::unique_ptr<QSqlQuery> selectActiveOrdersQuery;
static std::unique_ptr<QSqlQuery> selectPairsInfoQuery;
static std::unique_ptr<QSqlQuery> selectPairInfoQuery;
static std::unique_ptr<QSqlQuery> selectBidsQuery;
static std::unique_ptr<QSqlQuery> selectAsksQuery;
static std::unique_ptr<QSqlQuery> selectAllTradesInfo;
static std::unique_ptr<QSqlQuery> selectFundsInfoQuery;
static std::unique_ptr<QSqlQuery> selectRightsInfoQuery;
static std::unique_ptr<QSqlQuery> selectActiveOrdersCountQuery;
static std::unique_ptr<QSqlQuery> selectCurrencyVolumeQuery;
static std::unique_ptr<QSqlQuery> selectOrdersForSellTrade;
static std::unique_ptr<QSqlQuery> selectOrdersForBuyTrade;
std::unique_ptr<QSqlQuery> selectOwnerForKeyQuery;
std::unique_ptr<QSqlQuery> updateDepositQuery;
std::unique_ptr<QSqlQuery> updateOrderAmount;
std::unique_ptr<QSqlQuery> updateOrderDone;
std::unique_ptr<QSqlQuery> createTradeQuery;
std::unique_ptr<QSqlQuery> createOrderQuery;

bool initialiazeResponce(QSqlDatabase& db)
{
    auth.reset(new Authentificator(db));

    selectTickerInfoQuery.reset( new QSqlQuery(db));
    selectOrderInfoQuery.reset(new QSqlQuery(db));
    selectActiveOrdersQuery.reset(new QSqlQuery(db));
    selectPairsInfoQuery.reset( new QSqlQuery(db));
    selectPairInfoQuery.reset( new QSqlQuery(db));
    selectBidsQuery.reset( new QSqlQuery(db));
    selectAsksQuery.reset( new QSqlQuery(db));
    selectAllTradesInfo.reset(new QSqlQuery(db));
    selectFundsInfoQuery.reset(new QSqlQuery(db));
    selectRightsInfoQuery.reset(new QSqlQuery(db));
    selectActiveOrdersCountQuery.reset(new QSqlQuery(db));
    selectCurrencyVolumeQuery.reset(new QSqlQuery(db));
    selectOrdersForSellTrade.reset(new QSqlQuery(db));
    selectOrdersForBuyTrade.reset(new QSqlQuery(db));
    selectOwnerForKeyQuery.reset(new QSqlQuery(db));
    updateDepositQuery.reset(new QSqlQuery(db));
    updateOrderAmount.reset(new QSqlQuery(db));
    updateOrderDone.reset(new QSqlQuery(db));
    createTradeQuery.reset(new QSqlQuery(db));
    createOrderQuery.reset(new QSqlQuery(db));

    bool ok =
        selectTickerInfoQuery->prepare("select high, low, avg, vol, vol_cur, last, buy, sell, updated from ticker t left join pairs p on p.pair_id = t.pair_id where p.pair=:name")
    &&  selectPairsInfoQuery->prepare("select pair, decimal_places, min_price, max_price, min_amount, hidden, fee from pairs")
    &&  selectPairInfoQuery->prepare("select min_price, max_price, min_amount, fee, pair_id from pairs where pair=:pair")
    &&  selectOrderInfoQuery->prepare("select p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status+0 from orders o left join apikeys a  on o.owner_id=a.owner_id left join pairs p on p.pair_id = o.pair_id where o.order_id=:order_id")
    &&  selectActiveOrdersQuery->prepare("select o.order_id, p.pair, o.type, o.amount, o.rate, o.created, 0 from orders o left join apikeys a  on o.owner_id=a.owner_id left join pairs p on p.pair_id = o.pair_id where a.apikey=:key and o.status = 'active'")
    &&  selectBidsQuery->prepare("select type, rate, sum(amount), decimal_places from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and type='bids' and p.pair=:name group by pair, type, rate order by pair, type, rate desc")
    &&  selectAsksQuery->prepare("select type, rate, sum(amount), decimal_places from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and type='asks' and p.pair=:name group by pair, type, rate order by pair, type, rate asc")
    &&  selectAllTradesInfo->prepare("select o.type, o.rate, t.amount, t.trade_id, t.created from trades t left join orders o on o.order_id=t.order_id left join pairs p on o.pair_id=p.pair_id where p.pair=:name  order by t.trade_id desc")
    &&  selectFundsInfoQuery->prepare("select c.currency, d.volume from deposits d left join currencies c on c.currency_id=d.currency_id left join apikeys a on a.owner_Id=d.owner_id where apikey=:key")
    &&  selectRightsInfoQuery->prepare("select info,trade,withdraw from apikeys where apikey=:key")
    &&  selectActiveOrdersCountQuery->prepare("select count(*) from apikeys a left join orders o on o.owner_id=a.owner_id where a.apikey=:key and o.status = 0")
    &&  selectCurrencyVolumeQuery->prepare("select d.volume from deposits d left join currencies c on c.currency_id=d.currency_id left join apikeys a on a.owner_Id=d.owner_id where apikey=:key and c.currency=:currency")
    &&  selectOrdersForSellTrade->prepare("select order_id, amount, rate, owner_id from orders o left join pairs p on p.pair_id = o.pair_id where type='bids' and status='active' and pair=:pair and owner_id<>:owner_id and rate >= :rate order by rate desc,order_id asc")
    &&  selectOrdersForBuyTrade->prepare("select order_id, amount, rate, o.owner_id, w.name from orders o left join pairs p on p.pair_id = o.pair_id left join owners w on w.owner_id=o.owner_id where type='asks' and status='active' and pair=:pair and o.owner_id<>:owner_id and rate <= :rate order by rate asc,order_id")
    &&  selectOwnerForKeyQuery->prepare("select a.owner_id, o.name from apikeys a left join owners o on o.owner_id=a.owner_id where a.apikey=:key")
    &&  updateDepositQuery->prepare("update deposits d left join currencies c on c.currency_id=d.currency_id set d.volume = volume + :diff where owner_id=:owner_id and c.currency=:currency")
    &&  updateOrderAmount->prepare ("update orders set amount=amount-:diff where order_id=:order_id")
    &&  updateOrderDone->prepare("update orders set amount=0, status='done' where order_id=:order_id")
    &&  createTradeQuery->prepare("insert into trades (owner_id, order_id, amount, created) values (:owner_id, :order_id, :amount, :created)")
    &&  createOrderQuery->prepare("insert into orders (pair_id, owner_id, type, rate, start_amount, amount, created, status) values (:pair_id, :owner_id, :type, :rate, :start_amount, :amount, :created, 'active')")
            ;

    return ok;
}

bool tradeUpdateDeposit(const QVariant& owner_id, const QString& currency, float diff, const QString& ownerName)
{
    QVariantMap updateDepParams;
    updateDepParams[":owner_id"] = owner_id;
    updateDepParams[":currency"] = currency;
    updateDepParams[":diff"] = diff;
    bool ok;
    ok = performSql("update dep", *updateDepositQuery, updateDepParams, true);
    if (ok)
        std::clog << "\t\t" << QString("%1: %2 %3 %4")
                     .arg(ownerName)
                     .arg((diff < 0)?"lost":"recieved")
                     .arg(QString::number(qAbs(diff), 'f', 6))
                     .arg(currency.toUpper())
                  << std::endl;
    return ok;
}

QString createTrade(const QString& key, const QString& pair, const QString& type, const QString& rate, const QString& amount)
{
    QVariantMap orderCreateParams;
    orderCreateParams[":key"] = key;
    bool isSell = false;

    orderCreateParams[":pair"] = pair;
    performSql("get pair info", *selectPairInfoQuery, orderCreateParams);
    if (!selectPairInfoQuery->next())
        return "You incorrectly entered one of fields.";

    float min_price = selectPairInfoQuery->value(0).toFloat();
    float max_price = selectPairInfoQuery->value(1).toFloat();
    float min_amount = selectPairInfoQuery->value(2).toFloat();
    float fee = selectPairInfoQuery->value(3).toFloat() / 100;
    QVariant pair_id = selectPairInfoQuery->value(4);;

    if (type == "buy")
        orderCreateParams[":currency"] = pair.right(3);
    else if (type == "sell")
    {
        isSell = true;
        orderCreateParams[":currency"] = pair.left(3);
    }
    else
        return "You incorrectly entered one of fields.";

    if (!performSql("get funds info", *selectCurrencyVolumeQuery, orderCreateParams, true))
        return "internal database error: fail to perform query";

    if (!selectCurrencyVolumeQuery->next())
        return "invalid parameter: pair";

    bool ok;
    float currencyAvailable = selectCurrencyVolumeQuery->value(0).toFloat(&ok);
    if (!ok)
        return "internal database error: non numeric currency volume";

    float amnt = amount.toFloat(&ok);
    if (!ok)
        return "invalid parameter: amount";

    if (amnt < min_amount)
        return QString("Value %1 must be greater than 0.001 %1.")
                .arg(orderCreateParams[":currency"].toString().toUpper())
                .arg(min_amount);


    float rt = rate.toFloat(&ok);
    if (!ok)
        return "invalid parameter: rate";

    if (rt < min_price)
        return QString("Price per %1 must be greater than %2 %3.")
                .arg(pair.left(3).toUpper())
                .arg(min_price)
                .arg(pair.right(3).toUpper());
    if (rt > max_price)
        return QString("Price per %1 must be lower than %2 %3.")
                .arg(pair.left(3).toUpper())
                .arg(max_price)
                .arg(pair.right(3).toUpper());

    if (    (isSell && currencyAvailable < amnt)
        || (!isSell && currencyAvailable < amnt * rt))
        return QString("It is not enough %1 for %2")
                         .arg(orderCreateParams[":currency"].toString().toUpper())
                         .arg(isSell?"sell":"purchase");

    performSql("get owner id", *selectOwnerForKeyQuery, orderCreateParams, true);
    if (!selectOwnerForKeyQuery->next())
        return "internal database error: unable to get owner_id";

    QVariant owner_id = selectOwnerForKeyQuery->value(0);
    QString ownerName = selectOwnerForKeyQuery->value(1).toString();
    orderCreateParams[":rate"] = rate;
    orderCreateParams[":owner_id"] = owner_id;

    std::clog << QString("user %1 wants to %2 %3 %4 for %5 %6 (rate %7)")
                 .arg(ownerName).arg(type).arg(amnt).arg(pair.left(3).toUpper())
                 .arg(amnt * rt).arg(pair.right(3).toUpper()).arg(rt)
              << std::endl;

    if (type == "buy")
    {
        performSql("get ask orders", *selectOrdersForBuyTrade, orderCreateParams, true);
        while(selectOrdersForBuyTrade->next())
        {
            QVariant matched_order_id = selectOrdersForBuyTrade->value(0);
            float matched_amount = selectOrdersForBuyTrade->value(1).toFloat();
            float matched_rate = selectOrdersForBuyTrade->value(2).toFloat();
            QVariant matched_owner_id = selectOrdersForBuyTrade->value(3);
            QString matched_ownerName = selectOrdersForBuyTrade->value(4).toString();

            std::clog << '\t'
                      <<QString("Found ask order %1 from user %2 for %3 @ %4 ")
                         .arg(matched_order_id.toUInt()).arg(matched_ownerName)
                         .arg(matched_amount).arg(matched_rate)
                      << std::endl;

            float trade_amount = qMin(matched_amount, amnt);
            tradeUpdateDeposit(owner_id, pair.left(3), trade_amount * (1 - fee) , ownerName);
            tradeUpdateDeposit(owner_id, pair.right(3), -trade_amount * matched_rate, ownerName);
            tradeUpdateDeposit(matched_owner_id, pair.right(3), trade_amount * matched_rate * (1-fee), matched_ownerName);
            tradeUpdateDeposit(matched_owner_id, pair.right(3), -trade_amount, matched_ownerName);
            tradeUpdateDeposit(1000, pair.left(3), trade_amount * fee, "Exchange");
            tradeUpdateDeposit(1000, pair.right(3), matched_rate * trade_amount * fee, "Exchange");

            QVariantMap updateOrderParams;
            updateOrderParams[":order_id"] = matched_order_id;
            updateOrderParams[":diff"] = trade_amount;

            if (trade_amount >= matched_amount)
            {
                performSql("finish order", *updateOrderDone, updateOrderParams, true);
                std::clog << "\t\tOrder " << matched_order_id.toUInt() << "done" << std::endl;
            }
            else
            {
                performSql("update order", *updateOrderAmount, updateOrderParams, false);
                std::clog << "\t\tOrder " << matched_order_id.toUInt() << " amount changed to " << matched_amount - amnt << std::endl;
            }
            QVariantMap createTradeParams;
            createTradeParams[":owner_id"] = owner_id;
            createTradeParams[":order_id"] = matched_order_id;
            createTradeParams[":amount"] = trade_amount;
            createTradeParams[":created"]  = QDateTime::currentDateTime();
            performSql("create trade", *createTradeQuery, createTradeParams, true);

            amnt -= trade_amount;
            if (qFuzzyCompare(amnt+1.0, 0.0 + 1.0))
                break;
        }
        if (amnt > 0)
        {
            QVariantMap createOrderParams;
            createOrderParams[":owner_id"] = owner_id;
            createOrderParams[":pair_id"] = pair_id;
            createOrderParams[":amount"] = amnt;
            createOrderParams[":start_amount"] = amnt;
            createOrderParams[":rate"] = rt;
            createOrderParams[":created"] = QDateTime::currentDateTime();
            createOrderParams[":type"] = "bids";
            performSql("create order", *createOrderQuery, createOrderParams, true);
            std::clog << "new bid order for " << amnt << " @ " << rate << " created" << std::endl;
        }
    }

    return "not implemented yet";
}

QVariantMap getResponce(const QueryParser& parser, Method& method)
{
    method = Method::Invalid;
    QString methodName = parser.method();
    QVariantMap var;

    QueryParser::Scope scope = parser.apiScope();
    if (scope == QueryParser::Scope::Public)
    {
        if (methodName == "info")
        {
            var = getInfoResponce( method);
        }
        else if (methodName == "ticker" )
        {
            var = getTickerResponce(parser, method);
        }
        else if (methodName == "depth")
        {
            var = getDepthResponce(parser, method);
        }
        else if (methodName == "trades")
        {
            var = getTradesResponce(parser, method);
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
            method = AuthIssue;
            return var;
        }

        if (methodsRequresInfo.contains(methodName) && !auth->hasInfo(key))
        {
            var["success"] = 0;
            var["error"] = "api key dont have info permission";
            method = AccessIssue;
            return var;
        }
        else if (methodsRequresTrade.contains(methodName) && !auth->hasTrade(key))
        {
            var["success"] = 0;
            var["error"] = "api key dont have trade permission";
            method = AccessIssue;
            return var;
        }
        else if (methodsRequresWithdraw.contains(methodName) && !auth->hasWithdraw(key))
        {
            var["success"] = 0;
            var["error"] = "api key dont have withdraw permission";
            method = AccessIssue;
            return var;
        }

        if (methodName == "getInfo")
        {
            var = getPrivateInfoResponce(parser, method);
        }
        else if (methodName == "ActiveOrders")
        {
            var = getPrivateActiveOrdersResponce(parser, method);
        }
        else if (methodName == "Trade")
        {
            var = privateTrade(parser, method);
        }
        else if (methodName == "OrderInfo")
        {
            var = getPrivateOrderInfoResponce( parser, method);
        }
        else if (methodName == "CancelOrder")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
        }
        else if (methodName == "TradeHistory")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
        }
        else if (methodName == "TransHistory")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
        }
        else if (methodName == "CoinDepositAddress")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
        }
        else if (methodName == "WithdrawCoin")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
        }
        else if (methodName == "CreateCupon")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
        }
        else if (methodName == "RedeemCupon")
        {
            var["success"] = 0;
            var["error"] = "not implemented yet";
        }
    }

    if (var.isEmpty())
    {
        var["success"] = 0;

        if (method != Invalid)
            var["error"] = "Fail to provide info";
        else
            var["error"] = "Invalid method";
     }

    return var;
}

QVariantMap getInfoResponce(Method &method)
{
    method = Method::PublicInfo;
    QVariantMap var;
    QVariantMap pairs;

    var["server_time"] = QDateTime::currentDateTime().toTime_t();
    if (performSql("get info", *selectPairsInfoQuery, QVariantMap(), true))
    {
        while (selectPairsInfoQuery->next())
        {
            QVariantMap pair;
            pair["decimal_places"] = selectPairsInfoQuery->value(1).toInt();
            pair["min_price"] = selectPairsInfoQuery->value(2).toFloat();
            pair["max_price"] = selectPairsInfoQuery->value(3).toFloat();
            pair["min_amount"] = selectPairsInfoQuery->value(4).toFloat();
            pair["hidden"] = selectPairsInfoQuery->value(5).toInt();
            pair["fee"] = selectPairsInfoQuery->value(6).toFloat();

            pairs[selectPairsInfoQuery->value(0).toString()] = pair;
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

    return var;
}

QVariantMap getPrivateOrderInfoResponce(const QueryParser &httpQuery, Method& method)
{
    method = Method::PrivateOrderInfo;
    QVariantMap var;

    QVariantMap params;
    QString order_id = httpQuery.order_id();
    if (order_id.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "invalid parameter: order_id";
        return var;
    }
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
            order["start_amount"] = selectOrderInfoQuery->value(2);
            order["amount"] = selectOrderInfoQuery->value(3);
            order["rate"] = selectOrderInfoQuery->value(4);
            order["timestamp_created"] = selectOrderInfoQuery->value(5).toDateTime().toTime_t();
            order["status"] = selectOrderInfoQuery->value(6);

            result[order_id] = order;
            var["result"] = result;
            var["success"] = 1;
        }
        else
        {
            var["success"] = 0;
            var["error"] = "invalid order";
            return var;
        }
    }

    return var;
}

QVariantMap privateTrade(const QueryParser& httpQuery, Method& method)
{
    method = Method::PrivateTrade;
    QVariantMap var;

    QString errMsg = createTrade(httpQuery.key(), httpQuery.pair(), httpQuery.orderType(), httpQuery.rate(), httpQuery.amount());
    if (!errMsg.isEmpty())
    {
        var["success"] = 0;
        var["error"] = errMsg;
    }

    return var;
}
