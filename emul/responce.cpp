#include "responce.h"
#include "query_parser.h"
#include "sql_database.h"
#include "memcachedsqldataaccessor.h"
#include "utils.h"

#include <QCache>
#include <QSqlError>
#include <QSqlQuery>

QAtomicInt Responce::counter = 0;

Responce::Responce(QSqlDatabase& database)
    :db(database)
{
    dataAccessor = std::make_shared<MemcachedSqlDataAccessor>(db);
    auth.reset(new Authentificator(dataAccessor));

    selectActiveOrdersCountQuery.reset(new QSqlQuery(db));

    cancelOrderQuery.reset(new QSqlQuery(db));

    startTransaction.reset(new QSqlQuery("start transaction", db));
    commitTransaction.reset(new QSqlQuery("commit", db));
    rollbackTransaction.reset(new QSqlQuery("rollback", db));


    prepareSql(*selectActiveOrdersCountQuery, "select count(*) from apikeys a left join orders o on o.user_id=a.user_id where a.apikey=:key and o.status = 'active'");


    prepareSql(*cancelOrderQuery, "update orders set status=case when start_amount=amount then 'cancelled' else 'part_done' end where order_id=:order_id");
}

Responce::TradeCurrencyVolume Responce::trade_volumes (OrderInfo::Type type, const QString& pair, Fee fee,
                                 Amount trade_amount, Rate matched_order_rate)
{
    TradeCurrencyVolume ret;
    ret.currency = pair.right(3);
    ret.goods = pair.left(3);

    Fee contra_fee = Fee(1) - fee;
    if (type == OrderInfo::Type::Buy)
    {
        ret.trader_currency_out = ret.currency;
        ret.trader_volume_out = trade_amount * matched_order_rate;
        ret.trader_currency_in = ret.goods;
        ret.trader_volume_in = trade_amount * contra_fee;
    }
    else
    {
        ret.trader_currency_out = ret.goods;
        ret.trader_volume_out = trade_amount;
        ret.trader_currency_in = ret.currency;
        ret.trader_volume_in = trade_amount * matched_order_rate * contra_fee;
    }

    ret.parter_currency_in = ret.trader_currency_out;
    ret.partner_volume_in = ret.trader_volume_out * contra_fee;

    ret.exchange_currency_in = trade_amount * matched_order_rate * fee;
    ret.exchange_goods_in = trade_amount * fee;

    return ret;
}

Responce::NewOrderVolume Responce::new_order_currency_volume (OrderInfo::Type type, const QString& pair, Amount amount, Rate rate)
{
    NewOrderVolume ret;
    if (type == OrderInfo::Type::Sell)
    {
        ret.currency = pair.left(3);
        ret.volume = amount;
    }
    else
    {
        ret.currency = pair.right(3);
        ret.volume = amount * rate;
    }
    return ret;
}

OrderInfo::Type Responce::oppositOrderType(OrderInfo::Type type)
{
    if (type==OrderInfo::Type::Buy)
        return OrderInfo::Type::Sell;
    return OrderInfo::Type::Buy;
}

quint32 Responce::doExchange(QString userName, const Rate& rate, TradeCurrencyVolume volumes, OrderInfo::Type type, Rate rt, const PairName& pair, QSqlQuery& query, Amount& amnt, Fee fee, UserId user_id)
{
    quint32 ret = 0;
    QVariantMap params;
    params[":pair"] = pair;
    params[":user_id"] = user_id;
    params[":rate"] = QString::fromStdString(DEC_NAMESPACE::toString(rate));
    QString matchedOrderType = (oppositOrderType(type) == OrderInfo::Type::Buy)?"buy":"sell";
    if (!performSql(QString("get %1 orders").arg(matchedOrderType), query, params, true))
    {
        return (quint32)-1;
    }
    while(query.next())
    {
        PairInfo::Ptr pairInfo = dataAccessor->pairInfo(pair);
        OrderId matched_order_id = query.value(0).toUInt();
        Amount matched_amount = qvar2dec<7>(query.value(1));
        Amount matched_rate   = qvar2dec<7>(query.value(2));
        UserId matched_user_id = query.value(3).toUInt();
        QString matched_userName = query.value(4).toString();

//        std::clog << '\t'
//                  <<QString("Found %5 order %1 from user %2 for %3 @ %4 ")
//                     .arg(matched_order_id)
//                     .arg(matched_userName)
//                     .arg(dec2qstr(matched_amount, 6))
//                     .arg(dec2qstr(matched_rate, pairInfo->decimal_places))
//                     .arg(matchedOrderType)
//                  << std::endl;

        Amount trade_amount = qMin(matched_amount, amnt);
        volumes = trade_volumes(type, pair, fee, trade_amount, matched_rate);

        if (! (   dataAccessor->tradeUpdateDeposit(user_id, volumes.trader_currency_in, volumes.trader_volume_in , userName)
               && dataAccessor->tradeUpdateDeposit(user_id, volumes.trader_currency_out, -volumes.trader_volume_out , userName)
               && dataAccessor->tradeUpdateDeposit(matched_user_id, volumes.parter_currency_in, volumes.partner_volume_in, matched_userName)
               && dataAccessor->tradeUpdateDeposit(EXCHNAGE_USER_ID, volumes.currency, volumes.exchange_currency_in, "Exchange")
               && dataAccessor->tradeUpdateDeposit(EXCHNAGE_USER_ID, volumes.goods, volumes.exchange_goods_in, "Exchange")
                  ))
                return (quint32)-1;

        if (trade_amount >= matched_amount)
        {
            if (!dataAccessor->closeOrder(matched_order_id))
                return (quint32)-1;
        }
        else
        {
            if (!dataAccessor->reduceOrderAmount(matched_order_id, trade_amount))
                return (quint32)-1;
        }
        if (!dataAccessor->createNewTradeRecord(user_id, matched_order_id, trade_amount))
            return (quint32)-1;
        else
        {
            TickerInfo::Ptr ticker = dataAccessor->tickerInfo(pair);
            if (ticker)
            {
                QMutexLocker lock(&ticker->updateAccess);
                ticker->last = matched_rate;
                if (type == OrderInfo::Type::Buy)
                    ticker->buy = matched_rate;
                else
                    ticker->sell = matched_rate;
            }
        }

        amnt -= trade_amount;
        if (amnt == Amount(0))
            break;
    }
    if (amnt > Amount(0))
    {
        NewOrderVolume orderVolume;
        orderVolume = new_order_currency_volume(type, pair, amnt, rt);
        if (!dataAccessor->tradeUpdateDeposit(user_id, orderVolume.currency, -orderVolume.volume, userName))
            return (quint32)-1;


        ret = dataAccessor->createNewOrderRecord(pair, user_id, type, rate, amnt);
    }
    return ret;
}

Responce::OrderCreateResult Responce::checkParamsAndDoExchange(const ApiKey& key, const PairName& pair, OrderInfo::Type type, const Rate& rate, const Amount& amount)
{
    OrderCreateResult ret;
    ret.recieved = 0;
    ret.remains = 0;
    ret.order_id = 0;
    ret.ok = false;

    PairInfo::Ptr info = dataAccessor->pairInfo(pair);
    if (!info)
    {
        ret.errMsg = "You incorrectly entered one of fields.";
        return ret;
    }

    const Rate& min_price  = info->min_price;
    const Rate& max_price  = info->max_price;
    const Rate& min_amount = info->min_amount;
    const Fee& fee         = info->fee/Fee(100);
    int decimal_places = info->decimal_places;

    QString currency;

    if (type == OrderInfo::Type::Buy)
        currency = pair.right(3);
    else
        currency = pair.left(3);

    ApikeyInfo::Ptr apikey = dataAccessor->apikeyInfo(key);
    UserInfo::Ptr user = apikey->user_ptr.lock();
    if (!user)
    {
        user = dataAccessor->userInfo(apikey->user_id);
        apikey->user_ptr = user;
    }
    Amount currencyAvailable = user->funds[currency];

    Amount amnt = amount;

    if (amnt < min_amount)
    {
        ret.errMsg = QString("Value %1 must be greater than %2 %1.")
                .arg(currency.toUpper())
                .arg(dec2qstr(min_amount, 6));
        return ret;
    }

    if (rate < min_price)
    {
        ret.errMsg = QString("Price per %1 must be greater than %2 %3.")
                .arg(pair.left(3).toUpper())
                .arg(dec2qstr(min_price, decimal_places))
                .arg(pair.right(3).toUpper());
        return ret;
    }
    if (rate > max_price)
    {
        ret.errMsg = QString("Price per %1 must be lower than %2 %3.")
                .arg(pair.left(3).toUpper())
                .arg(dec2qstr(max_price, decimal_places))
                .arg(pair.right(3).toUpper());
        return ret;
    }

    if (   (type == OrderInfo::Type::Sell && currencyAvailable < amnt)
        || (type == OrderInfo::Type::Buy && currencyAvailable < amnt * rate))
    {
        ret.errMsg =         QString("It is not enough %1 for %2")
                .arg(currency.toUpper())
                .arg((type == OrderInfo::Type::Sell)?"sell":"purchase");
        return ret;
    }


    UserId user_id = user->user_id;
    QString userName = user->name;

//    std::clog << QString("user %1 wants to %2 %3 %4 for %5 %6 (rate %7)")
//                 .arg(userName).arg((type == OrderInfo::Type::Buy)?"buy":"sell").arg(dec2qstr(amnt, 6)).arg(pair.left(3).toUpper())
//                 .arg(dec2qstr(amnt * rate, decimal_places)).arg(pair.right(3).toUpper()).arg(dec2qstr(rate, decimal_places))
//              << std::endl;

    TradeCurrencyVolume volumes;
    static QMap<QString, QMutex*> tradeMutex;
    static QMutex mutexsCollectionAccess;

    QString mutexKey = QString("%1-%2").arg(pair).arg(static_cast<int>(type));
    mutexsCollectionAccess.lock();
    QMutex* pMutex = nullptr;
    auto iter = tradeMutex.find(mutexKey);
    if (iter == tradeMutex.end())
    {
        pMutex = new QMutex;
        tradeMutex.insert(mutexKey, pMutex);
    }
    else
        pMutex = iter.value();
    mutexsCollectionAccess.unlock();

    bool success = true;
    do
    {
        try
        {
            QMutexLocker lock(pMutex);
            dataAccessor->transaction();
            QSqlQuery query(db);
            if (type == OrderInfo::Type::Buy)
                prepareSql(query,  "select order_id, amount, rate, o.user_id, w.name from orders o left join pairs p on p.pair_id = o.pair_id left join users w on w.user_id=o.user_id where type='sell' and status='active' and pair=:pair and o.user_id<>:user_id and rate <= :rate order by rate asc, order_id asc");
            else if (type == OrderInfo::Type::Sell)
                prepareSql(query, "select order_id, amount, rate, o.user_id, w.name from orders o left join pairs p on p.pair_id = o.pair_id left join users w on w.user_id=o.user_id where type='buy'  and status='active' and pair=:pair and o.user_id<>:user_id and rate >= :rate order by rate desc, order_id asc");

            amnt = amount;
            ret.order_id = doExchange(userName, rate, volumes, type, rate, pair, query, amnt, fee, user_id);
            dataAccessor->commit();
            success = true;
        }
        catch(const QSqlQuery& e)
        {
            dataAccessor->rollback();
            if (e.lastError().nativeErrorCode() != "1213")
                throw;
            success = false;
        }
    } while (!success);

    if (ret.order_id != (quint32)-1)
    {
        ret.ok = true;
        ret.recieved = (amount - amnt) * (Fee(1) - fee);
        ret.remains = amnt;
        ret.errMsg.clear();
    }
    else
    {
        ret.errMsg = "internal database error";
        ret.ok = false;
    }

    return ret;
}

QVariantMap Responce::getResponce(const QueryParser& parser, Method& method)
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
            var = getPrivateTradeResponce(parser, method);
        }
        else if (methodName == "OrderInfo")
        {
            var = getPrivateOrderInfoResponce( parser, method);
        }
        else if (methodName == "CancelOrder")
        {
            var = getPrivateCancelOrderResponce(parser, method);
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

QVariantMap Responce::getInfoResponce(Method &method)
{
    method = Method::PublicInfo;
    QVariantMap var;
    QVariantMap pairs;

    var["server_time"] = QDateTime::currentDateTime().toTime_t();
    PairInfo::List allPairs = dataAccessor->allPairsInfoList();
    for (PairInfo::Ptr info: allPairs)
    {
        QVariantMap pair;
        pair["decimal_places"] = info->decimal_places;
        pair["min_price"] = dec2qstr(info->min_price, info->decimal_places);
        pair["max_price"] = dec2qstr(info->max_price, info->decimal_places);
        pair["min_amount"] = dec2qstr(info->min_amount, 6);
        pair["hidden"] = static_cast<int>(info->hidden);
        pair["fee"] = dec2qstr(info->fee, 7);

        pairs[info->pair] = pair;
    }
    var["pairs"] = pairs;
    return var;
}

QVariantMap Responce::getTickerResponce(const QueryParser& httpQuery, Method& method)
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
        TickerInfo::Ptr info = dataAccessor->tickerInfo(pairName);
        if (info)
        {
            QVariantMap pair;
            PairInfo::Ptr pinfo = info->pair_ptr.lock();
            int decimal_places=  7;
            if (pinfo)
                decimal_places = pinfo->decimal_places;
            pair["high"] = dec2qstr(info->high, decimal_places);
            pair["low"]  = dec2qstr(info->low, decimal_places);
            pair["avg"]  = dec2qstr(info->avg, decimal_places);
            pair["vol"]  = dec2qstr(info->vol, 6);
            pair["vol_cur"] = dec2qstr(info->vol_cur, decimal_places);
            pair["last"] = dec2qstr(info->last, decimal_places);
            pair["buy"] = dec2qstr(info->buy, decimal_places);
            pair["sell"] = dec2qstr(info->sell, decimal_places);
            pair["updated"] = info->updated.toTime_t();

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
    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Empty pair list";
    }
    return var;
}

QVariantList Responce::appendDepthToMap(const Depth& depth, int limit, int dp)
{
    QVariantList ret;
    for(QPair<Rate, Amount> item: depth)
    {
            QVariantList values;
            values << dec2qstr(item.first, dp) << dec2qstr(item.second, 6);
            QVariant castedValue = values;
            ret << castedValue;
            if (ret.length() >= limit)
                break;
    }
    return ret;
}


QVariantMap Responce::getDepthResponce(const QueryParser& httpQuery, Method& method)
{
    method = Method::PublicDepth;
    QVariantMap var;
    int limit = httpQuery.limit();
    QMap<PairName, BuySellDepth> pairsDepth = dataAccessor->allActiveOrdersAmountAgreggatedByRateList(httpQuery.pairs());
    for (QMap<PairName, BuySellDepth>::const_iterator item = pairsDepth.constBegin(); item != pairsDepth.constEnd(); item++)
    {
        const QString& pairName = item.key();
        if (pairName.isEmpty())
            continue;
        if (var.contains(pairName))
        {
            var.clear();
            var["success"] = 0;
            var["error"] = "Duplicated pair name: " + pairName;
            break;
        }
        PairInfo::Ptr pinfo = dataAccessor->pairInfo(pairName);
        int decimal_places = 7;
        if (pinfo)
            decimal_places = pinfo->decimal_places;
        QVariantMap p;
        p["bids"] = appendDepthToMap(item.value().first,  limit, decimal_places);
        p["asks"] = appendDepthToMap(item.value().second, limit, decimal_places);
        var[pairName] = p;
    }
    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Empty pair list";
    }
    return var;
}

QVariantMap Responce::getTradesResponce(const QueryParser &httpQuery, Method &method)
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
        TradeInfo::List tradesList = dataAccessor->allTradesInfo(pairName);
        QVariantList list;
        QVariantMap tr;
        QString type;
        PairInfo::Ptr pinfo = dataAccessor->pairInfo(pairName);
        int decimal_places = 7;
        if (pinfo)
            decimal_places = pinfo->decimal_places;

        for(TradeInfo::Ptr info: tradesList)
        {
            if (list.size() >= limit)
                break;

            if (info->type == TradeInfo::Type::Bid)
                type = "bid";
            else
                type = "ask";

            tr["type"] = type;
            tr["price"] = dec2qstr(info->rate, decimal_places);
            tr["amount"] = dec2qstr(info->amount, 6);
            tr["tid"] = info->tid;
            tr["timestamp"] = info->created.toTime_t();

            list << tr;
        }
        var[pairName] = list;
    }
    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Empty pair list";
    }
    return var;
}

QVariantMap Responce::getPrivateInfoResponce(const QueryParser &httpQuery, Method& method)
{
    method = Method::PrivateGetInfo;
    QVariantMap var;
    QVariantMap funds;
    QVariantMap rights;
    QVariantMap result;


    QVariantMap params;
    ApikeyInfo::Ptr apikey = dataAccessor->apikeyInfo(httpQuery.key());
    UserInfo::Ptr user = apikey->user_ptr.lock();
    if (!user)
    {
        user = dataAccessor->userInfo(apikey->user_id);
        apikey->user_ptr = user;
    }
    Funds& f = user->funds;
    for(const QString& cur: f.keys())
    {
        funds[cur] = dec2qstr(f[cur], 6);
    }
    result["funds"] = funds;
    rights["info"] = apikey->info;
    rights["trade"] = apikey->trade;
    rights["withdraw"] = apikey->withdraw;
    result["rights"] = rights;
    if (performSql("get orders count", *selectActiveOrdersCountQuery, params, true))
    {
        if (selectActiveOrdersCountQuery->next())
            result["open_orders"] = selectActiveOrdersCountQuery->value(0).toUInt();
    }
    if (result.contains("rights") && result.contains("funds") && result.contains("open_orders"))
    {
        result["transaction_count"] = 0;
        result["server_time"] = QDateTime::currentDateTime().toTime_t();
        var["return"] = result;
        var["success"] = 1;
    }
    else
        var.clear();

    return var;
}

QVariantMap Responce::getPrivateActiveOrdersResponce(const QueryParser &httpQuery, Method& method)
{
    method = Method::PrivateActiveOrders;
    QVariantMap var;

    OrderInfo::List list = dataAccessor->activeOrdersInfoList(httpQuery.key());
    QVariantMap result;
    for(OrderInfo::Ptr& info: list)
    {
        OrderId order_id = info->order_id;
        QVariantMap order;
        PairInfo::Ptr pinfo = dataAccessor->pairInfo(info->pair);
        int decimal_places = 7;
        if (pinfo)
            decimal_places = pinfo->decimal_places;
        order["pair"]   = info->pair;
        order["type"]   = (info->type == OrderInfo::Type::Sell)?"sell":"buy";
        order["amount"] = dec2qstr(info->amount, 6);
        order["rate"]   = dec2qstr(info->rate, decimal_places);
        order["timestamp_created"] = info->created.toTime_t();
        order["status"] = static_cast<int>(info->status) - 1;

        result[QString::number(order_id)] = order;
    }
    var["return"] = result;
    var["success"] = 1;

    return var;
}

QVariantMap Responce::getPrivateOrderInfoResponce(const QueryParser &httpQuery, Method& method)
{
    method = Method::PrivateOrderInfo;
    QVariantMap var;

    QString order_id = httpQuery.order_id();
    if (order_id.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "invalid parameter: order_id";
        return var;
    }

    OrderInfo::Ptr info = dataAccessor->orderInfo(order_id.toUInt());
    QVariantMap result;
    if (info)
    {
        QVariantMap order;
        PairInfo::Ptr pinfo = dataAccessor->pairInfo(info->pair);
        int decimal_places = 7;
        if (pinfo)
            decimal_places = pinfo->decimal_places;
        order["pair"] = info->pair;
        order["type"] = (info->type == OrderInfo::Type::Buy)?"buy":"sell";
        order["start_amount"] = dec2qstr(info->start_amount, 6);
        order["amount"] = dec2qstr(info->start_amount, 6);
        order["rate"] = dec2qstr(info->rate, decimal_places);
        order["timestamp_created"] = info->created.toTime_t();
        order["status"] = static_cast<int>(info->status);

        result[order_id] = order;
        var["return"] = result;
        var["success"] = 1;
    }
    else
    {
        var["success"] = 0;
        var["error"] = "invalid order";
        return var;
    }

    return var;
}

QVariantMap Responce::getPrivateTradeResponce(const QueryParser& httpQuery, Method& method)
{
    method = Method::PrivateTrade;
    QVariantMap var;

    bool done = false;
    do
    {
        OrderInfo::Type type;
        if (httpQuery.orderType() == "buy")
            type = OrderInfo::Type::Buy;
        else if (httpQuery.orderType() == "sell")
            type = OrderInfo::Type::Sell;
        else
        {
            var["success"] = 0;
            var["error"] = "You incorrectly entered one of fields.";
            return var;
        }
        Rate rate = Rate(httpQuery.rate().toStdString());
        Amount amount = Amount(httpQuery.amount().toStdString());
        OrderCreateResult ret = checkParamsAndDoExchange(httpQuery.key(), httpQuery.pair(), type, rate, amount);
        if (!ret.ok)
        {
            var["success"] = 0;
            var["error"] = ret.errMsg;
            done = true;
        }
        else
        {
            done = true;
            var["success"] = 1;
            QVariantMap res;

            res["remains"] = dec2qstr(ret.remains, 7);
            res["received"] = dec2qstr(ret.recieved, 6);
            res["order_id"] = ret.order_id;

            QVariantMap funds;
            ApikeyInfo::Ptr aInfo = dataAccessor->apikeyInfo(httpQuery.key());
            if (!aInfo)
            {
                res["success"] = 0;
                res["error"] = "invalid api key";
                return res;
            }
            UserInfo::Ptr  uInfo = aInfo->user_ptr.lock();
            if (!uInfo)
            {
                uInfo = dataAccessor->userInfo(aInfo->user_id);
                aInfo->user_ptr = uInfo;
            }
            Funds& f = uInfo->funds;
            for(const QString& cur: f.keys())
                funds[cur] = dec2qstr(f[cur], 6);
            res["funds"] = funds;
            var["return"] = res;
        }
    }  while (!done);
    return var;
}

QVariantMap Responce::getPrivateCancelOrderResponce(const QueryParser& httpQuery, Method& method)
{
    method = Method::PrivateCanelOrder;
    QVariantMap var;
    QVariantMap params;
    QVariantMap funds;
    QVariantMap ret;
    QString order_id = httpQuery.order_id();
    if (order_id.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "invalid parameter: order_id";
        return var;
    }
    params[":order_id"] = order_id;
    bool done = false;
    do
    {
        try
        {
            startTransaction->exec();
            OrderInfo::Ptr info = dataAccessor->orderInfo(order_id.toUInt());
            if (info)
            {
                // p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status+0
                QString pair = info->pair;
                OrderInfo::Type type = info->type;
                Amount amount = info->amount;
                Rate rate = info->rate;
                OrderInfo::Status status = info->status;
                quint32 user_id = info->user_id;

                if (status != OrderInfo::Status::Active)
                {
                    var["success"] = 0;
                    var["error"] = "not active order";
                    return var;
                }
                NewOrderVolume orderVolume = new_order_currency_volume(type, pair, amount, rate);
                dataAccessor->tradeUpdateDeposit(user_id, orderVolume.currency, orderVolume.volume, QString::number(user_id));

                UserInfo::Ptr user = dataAccessor->userInfo(user_id);
                for(const QString& cur: user->funds.keys())
                    funds[cur] = funds[cur];
                ret["funds"] = funds;

                performSql("cancel order", *cancelOrderQuery, params, true);

                ret["order_id"] = order_id;

                var["return"] = ret;
                var["success"] = 1;
                var["return"] = ret;
            }
            commitTransaction->exec();
            done = true;
        }
        catch (std::runtime_error& e)
        {
            std::cerr << e.what() << std::endl;
            rollbackTransaction->exec();
        }
        catch (const QSqlQuery& q)
        {
            std::cerr << q.lastError().text() << std::endl;
            rollbackTransaction->exec();
        }
    } while (!done);

    return var;
}

QVariantMap Responce::exchangeBalance()
{
    QVariantMap balance;
    QSqlQuery sql(db);
    sql.exec("START TRANSACTION");
    QString query = "SELECT cur, sum(vol) from ("
                   "select c.currency as cur, sum(volume) as vol from deposits d left join currencies c on c.currency_id = d.currency_id group by d.currency_id  "
                   " UNION "
                   "select right(p.pair,3) as cur, sum(amount*rate) as vol  from orders o left join pairs p on p.pair_id = o.pair_id where status='active' and type='buy'  group by right(p.pair, 3) "
                   " UNION "
                   "select left(p.pair,3) as cur, sum(amount)      as vol  from orders o left join pairs p on p.pair_id = o.pair_id where status='active' and type='sell' group by  left(p.pair, 3)"
                   ") A group by cur";
    performSql("get deposits balance", sql, query, true);
    sql.exec("COMMIT");
    while(sql.next())
        balance[sql.value(0).toString()] = sql.value(1).toFloat();


    return balance;
}

OrderInfo::List Responce::negativeAmountOrders()
{
    return dataAccessor->negativeAmountOrders();
}

void Responce::updateTicker()
{
    dataAccessor->updateTicker();
}
