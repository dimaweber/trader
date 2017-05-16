#include "responce.h"
#include "query_parser.h"
#include "sql_database.h"
#include "utils.h"

#include <QCache>
#include <QSqlError>
#include <QSqlQuery>

#define TICKER_CACHE_EXPIRE_SECONDS 300

QAtomicInt Responce::counter = 0;

Responce::Responce(QSqlDatabase& database)
    :db(database)
{
    auth.reset(new Authentificator(db));

    selectSellOrdersQuery.reset( new QSqlQuery(db));
    selectAllTradesInfo.reset(new QSqlQuery(db));
    selectFundsInfoQuery.reset(new QSqlQuery(db));
    selectFundsInfoQueryByOwnerId.reset(new QSqlQuery(db));
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
    cancelOrderQuery.reset(new QSqlQuery(db));
    startTransaction.reset(new QSqlQuery("start transaction", db));
    commitTransaction.reset(new QSqlQuery("commit", db));
    rollbackTransaction.reset(new QSqlQuery("rollback", db));
    totalBalance.reset(new QSqlQuery(db));

    bool ok =true

    &&  prepareSql(*selectSellOrdersQuery, "select rate, sum(amount) from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and type='sell' and p.pair=:name group by rate order by  rate asc")
    &&  prepareSql(*selectAllTradesInfo, "select o.type, o.rate, t.amount, t.trade_id, t.created from trades t left join orders o on o.order_id=t.order_id left join pairs p on o.pair_id=p.pair_id where p.pair=:name  order by t.trade_id desc")
    &&  prepareSql(*selectFundsInfoQuery, "select c.currency, d.volume from deposits d left join currencies c on c.currency_id=d.currency_id left join apikeys a on a.owner_Id=d.owner_id where apikey=:key")
    &&  prepareSql(*selectFundsInfoQueryByOwnerId, "select c.currency, d.volume from deposits d left join currencies c on c.currency_id=d.currency_id where owner_id=:owner_id")
    &&  prepareSql(*selectRightsInfoQuery, "select info,trade,withdraw from apikeys where apikey=:key")
    &&  prepareSql(*selectActiveOrdersCountQuery, "select count(*) from apikeys a left join orders o on o.owner_id=a.owner_id where a.apikey=:key and o.status = 'active'")
    &&  prepareSql(*selectCurrencyVolumeQuery, "select d.volume from deposits d left join currencies c on c.currency_id=d.currency_id left join apikeys a on a.owner_Id=d.owner_id where apikey=:key and c.currency=:currency")
    &&  prepareSql(*selectOrdersForSellTrade, "select order_id, amount, rate, o.owner_id, w.name from orders o left join pairs p on p.pair_id = o.pair_id left join owners w on w.owner_id=o.owner_id where type='buy' and status='active' and pair=:pair and o.owner_id<>:owner_id and rate >= :rate order by rate desc, order_id asc")
    &&  prepareSql(*selectOrdersForBuyTrade, "select order_id, amount, rate, o.owner_id, w.name from orders o left join pairs p on p.pair_id = o.pair_id left join owners w on w.owner_id=o.owner_id where type='sell' and status='active' and pair=:pair and o.owner_id<>:owner_id and rate <= :rate order by rate asc, order_id asc")
    &&  prepareSql(*selectOwnerForKeyQuery, "select a.owner_id, o.name from apikeys a left join owners o on o.owner_id=a.owner_id where a.apikey=:key")
    &&  prepareSql(*updateDepositQuery, "update deposits d left join currencies c on c.currency_id=d.currency_id set d.volume = volume + :diff where owner_id=:owner_id and c.currency=:currency")
    &&  prepareSql(*updateOrderAmount, "update orders set amount=amount-:diff where order_id=:order_id")
    &&  prepareSql(*updateOrderDone, "update orders set amount=0, status='done' where order_id=:order_id")
    &&  prepareSql(*createTradeQuery, "insert into trades (owner_id, order_id, amount, created) values (:owner_id, :order_id, :amount, :created)")
    &&  prepareSql(*createOrderQuery, "insert into orders (pair_id, owner_id, type, rate, start_amount, amount, created, status) values (:pair_id, :owner_id, :type, :rate, :start_amount, :amount, :created, 'active')")
    &&  prepareSql(*cancelOrderQuery, "update orders set status=case when start_amount=amount then 'cancelled' else 'part_done' end where order_id=:order_id")
    &&  prepareSql(*totalBalance, "SELECT cur, sum(vol) from ("
                   "select c.currency as cur, sum(volume) as vol from deposits d left join currencies c on c.currency_id = d.currency_id group by d.currency_id  "
                   " UNION "
                   "select right(p.pair,3) as cur, sum(amount*rate) as vol  from orders o left join pairs p on p.pair_id = o.pair_id where status='active' and type='buy'  group by right(p.pair, 3) "
                   " UNION "
                   "select left(p.pair,3) as cur, sum(amount)      as vol  from orders o left join pairs p on p.pair_id = o.pair_id where status='active' and type='sell' group by  left(p.pair, 3)"
                   ") A group by cur")
            ;

    //return ok;
}

bool Responce::tradeUpdateDeposit(const QVariant& owner_id, const QString& currency, Amount diff, const QString& ownerName)
{
    QVariantMap updateDepParams;
    updateDepParams[":owner_id"] = owner_id;
    updateDepParams[":currency"] = currency;
    updateDepParams[":diff"] = diff.getAsDouble();
    bool ok;
    ok = performSql("update dep", *updateDepositQuery, updateDepParams, true);
    if (ok)
        std::clog << "\t\t" << QString("%1: %2 %3 %4")
                     .arg(ownerName)
                     .arg((diff.sign() == -1)?"lost":"recieved")
                     .arg(QString::number(qAbs(diff.getAsDouble()), 'f', 6))
                     .arg(currency.toUpper())
                  << std::endl;
    return ok;
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

Responce::OrderInfo::Type Responce::oppositOrderType(OrderInfo::Type type)
{
    if (type==OrderInfo::Type::Buy)
        return OrderInfo::Type::Sell;
    return OrderInfo::Type::Buy;
}

quint32 Responce::doExchange(QString ownerName, const QString& rate, TradeCurrencyVolume volumes, Responce::OrderInfo::Type type, Rate rt, const QString& pair, QSqlQuery& query, Amount& amnt, Fee fee, QVariant owner_id, QVariant pair_id, QVariantMap orderCreateParams)
{
    quint32 ret = 0;
    if (!performSql(QString("get %1 orders").arg((oppositOrderType(type) == OrderInfo::Type::Buy)?"buy":"sell"), query, orderCreateParams, true))
    {
        return (quint32)-1;
    }
    while(query.next())
    {
        QVariant matched_order_id = query.value(0);
        Amount matched_amount = Amount(query.value(1).toString().toStdString());
        Amount matched_rate   = Amount(query.value(2).toString().toStdString());
        QVariant matched_owner_id = query.value(3);
        QString matched_ownerName = query.value(4).toString();

//        std::clog << '\t'
//                  <<QString("Found %5 order %1 from user %2 for %3 @ %4 ")
//                     .arg(matched_order_id.toUInt()).arg(matched_ownerName)
//                     .arg(matched_amount.getAsDouble()).arg(matched_rate.getAsDouble()).arg(oppositOrderType(type))
//                  << std::endl;

        Amount trade_amount = qMin(matched_amount, amnt);
        volumes = trade_volumes(type, pair, fee, trade_amount, matched_rate);

        if (! (   tradeUpdateDeposit(owner_id, volumes.trader_currency_in, volumes.trader_volume_in , ownerName)
               && tradeUpdateDeposit(owner_id, volumes.trader_currency_out, -volumes.trader_volume_out , ownerName)
               && tradeUpdateDeposit(matched_owner_id, volumes.parter_currency_in, volumes.partner_volume_in, matched_ownerName)
               && tradeUpdateDeposit(EXCHNAGE_OWNER_ID, volumes.currency, volumes.exchange_currency_in, "Exchange")
               && tradeUpdateDeposit(EXCHNAGE_OWNER_ID, volumes.goods, volumes.exchange_goods_in, "Exchange")
                  ))
                return (quint32)-1;

        QVariantMap updateOrderParams;
        updateOrderParams[":order_id"] = matched_order_id;
        updateOrderParams[":diff"] = trade_amount.getAsDouble();

        if (trade_amount >= matched_amount)
        {
            if (!performSql("finish order", *updateOrderDone, updateOrderParams, true))
                return (quint32)-1;
//            std::clog << "\t\tOrder " << matched_order_id.toUInt() << "done" << std::endl;
        }
        else
        {
            if (!performSql("update order", *updateOrderAmount, updateOrderParams, true))
                return (quint32)-1;
//            std::clog << "\t\tOrder " << matched_order_id.toUInt() << " amount changed to " << matched_amount - trade_amount << std::endl;
        }
        QVariantMap createTradeParams;
        createTradeParams[":owner_id"] = owner_id;
        createTradeParams[":order_id"] = matched_order_id;
        createTradeParams[":amount"] = trade_amount.getAsDouble();
        createTradeParams[":created"]  = QDateTime::currentDateTime();
        if (!performSql("create trade", *createTradeQuery, createTradeParams, true))
            return (quint32)-1;

        amnt -= trade_amount;
        if (amnt == Amount(0))
            break;
    }
    if (amnt > Amount(0))
    {
        NewOrderVolume orderVolume;
        orderVolume = new_order_currency_volume(type, pair, amnt, rt);
        if (!tradeUpdateDeposit(owner_id, orderVolume.currency, -orderVolume.volume, ownerName))
            return (quint32)-1;

        QVariantMap createOrderParams;
        createOrderParams[":owner_id"] = owner_id;
        createOrderParams[":pair_id"] = pair_id;
        createOrderParams[":amount"] = amnt.getAsDouble();
        createOrderParams[":start_amount"] = amnt.getAsDouble();
        createOrderParams[":rate"] = rt.getAsDouble();
        createOrderParams[":created"] = QDateTime::currentDateTime();
        createOrderParams[":type"] = (type == OrderInfo::Type::Buy)?"buy":"sell";
        if (!performSql("create order", *createOrderQuery, createOrderParams, true))
            return (quint32)-1;
//        std::clog << "new " << type << " order for " << amnt << " @ " << rate << " created" << std::endl;

        ret = createOrderQuery->lastInsertId().toUInt();
    }
    return ret;
}

Responce::OrderCreateResult Responce::createTrade(const QString& key, const QString& pair, OrderInfo::Type type, const QString& rate, const QString& amount)
{
    OrderCreateResult ret;
    ret.recieved = 0;
    ret.remains = 0;
    ret.order_id = 0;
    ret.ok = false;

    QVariantMap orderCreateParams;
    orderCreateParams[":key"] = key;
    bool isSell = false;

    PairInfo::Ptr info = pairInfo(pair);
    if (!info)
    {
        ret.errMsg = "You incorrectly entered one of fields.";
        return ret;
    }

    Rate min_price  = info->min_price;
    Rate max_price  = info->max_price;
    Rate min_amount = info->min_amount;
    Fee fee         = info->fee;
    PairId pair_id = info->pair_id;

    if (type == OrderInfo::Type::Buy)
        orderCreateParams[":currency"] = pair.right(3);
    else/* if (type == OrderInfo::Type::Sell)*/
    {
        isSell = true;
        orderCreateParams[":currency"] = pair.left(3);
    }

    if (!performSql("get funds info", *selectCurrencyVolumeQuery, orderCreateParams, true))
    {
        ret.errMsg = "internal database error: fail to perform query";
        return ret;
    }

    if (!selectCurrencyVolumeQuery->next())
    {
        ret.errMsg = QString("internal database error: could not get deposits for %1").arg(orderCreateParams[":currency"].toString()) ;
        return ret;
    }

    Amount currencyAvailable = Amount(selectCurrencyVolumeQuery->value(0).toString().toStdString());

    Amount amnt = Amount(amount.toStdString());

    if (amnt < min_amount)
    {
        ret.errMsg = QString("Value %1 must be greater than 0.001 %1.")
                .arg(orderCreateParams[":currency"].toString().toUpper())
                .arg(min_amount.getAsDouble());
        return ret;
    }

    Rate rt = Rate(rate.toStdString());

    if (rt < min_price)
    {
        ret.errMsg = QString("Price per %1 must be greater than %2 %3.")
                .arg(pair.left(3).toUpper())
                .arg(min_price.getAsDouble())
                .arg(pair.right(3).toUpper());
        return ret;
    }
    if (rt > max_price)
    {
        ret.errMsg = QString("Price per %1 must be lower than %2 %3.")
                .arg(pair.left(3).toUpper())
                .arg(max_price.getAsDouble())
                .arg(pair.right(3).toUpper());
        return ret;
    }

    if (    (isSell && currencyAvailable < amnt)
        || (!isSell && currencyAvailable < amnt * rt))
    {
        ret.errMsg =         QString("It is not enough %1 for %2")
                .arg(orderCreateParams[":currency"].toString().toUpper())
                .arg(isSell?"sell":"purchase");
        return ret;
    }

    if (!performSql("get owner id", *selectOwnerForKeyQuery, orderCreateParams, true))
    {
        ret.errMsg = "internal database error";
        return ret;
    }

    if (!selectOwnerForKeyQuery->next())
    {
        ret.errMsg = "internal database error: unable to get owner_id";
        return ret;
    }


    QVariant owner_id = selectOwnerForKeyQuery->value(0);
    QString ownerName = selectOwnerForKeyQuery->value(1).toString();
    orderCreateParams[":rate"] = rate;
    orderCreateParams[":owner_id"] = owner_id;

//    std::clog << QString("user %1 wants to %2 %3 %4 for %5 %6 (rate %7)")
//                 .arg(ownerName).arg(type).arg(amnt.getAsDouble()).arg(pair.left(3).toUpper())
//                 .arg((amnt * rt).getAsDouble()).arg(pair.right(3).toUpper()).arg(rt.getAsDouble())
//              << std::endl;

    TradeCurrencyVolume volumes;
    QSqlQuery* query = nullptr;
    if (type == OrderInfo::Type::Buy)
        query = selectOrdersForBuyTrade.get();
    else if (type == OrderInfo::Type::Sell)
        query = selectOrdersForSellTrade.get();

    ret.order_id = doExchange(ownerName, rate, volumes, type, rt, pair, *query, amnt, fee, owner_id, pair_id, orderCreateParams);
    if (ret.order_id != (quint32)-1)
    {
        ret.ok = true;
        ret.recieved = Amount(amount.toStdString()) - amnt;
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
    PairInfo::List allPairs = allPairsInfoList();
    for (PairInfo::Ptr info: allPairs)
    {
        QVariantMap pair;
        pair["decimal_places"] = info->decimal_places;
        pair["min_price"] = info->min_price.getAsDouble();
        pair["max_price"] = info->max_price.getAsDouble();
        pair["min_amount"] = info->min_amount.getAsDouble();
        pair["hidden"] = info->hidden;
        pair["fee"] = info->fee.getAsDouble();

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
        TickerInfo::Ptr info = tickerInfo(pairName);
        if (info)
        {
            QVariantMap pair;
            pair["high"] = info->high.getAsDouble();
            pair["low"]  = info->low.getAsDouble();
            pair["avg"]  = info->avg.getAsDouble();
            pair["vol"]  = info->vol.getAsDouble();
            pair["vol_cur"] = info->vol_cur.getAsDouble();
            pair["last"] = info->last.getAsDouble();
            pair["buy"] = info->buy.getAsDouble();
            pair["sell"] = info->sell.getAsDouble();
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

QVariantList Responce::appendDepthToMap(QSqlQuery& query, const QString& pairName, int limit)
{
    QVariantList ret;
    QVariantMap params;
    params[":name"] = pairName;
    if (performSql("append depth", query, params, true))
    {
        while(query.next())
        {
            QVariantList values;
            values << query.value(0) << query.value(1);
            QVariant castedValue = values;
            ret << castedValue;
            if (ret.length() >= limit)
                break;
        }
    }
    return ret;
}

QMap<Responce::PairName, Responce::PairInfo::Ptr> Responce::pairInfoCache;
QReadWriteLock Responce::pairInfoCacheAccessRW;

Responce::PairInfo::List Responce::allPairsInfoList()
{
    // This function never use cache and always read from DB, invalidating cache
    QSqlQuery sql(db);
    PairInfo::List ret;
    prepareSql(sql, "select pair, decimal_places, min_price, max_price, min_amount, hidden, fee, pair_id from pairs");
    if (performSql("retrieve all pairs info", sql, QVariantMap()))
    {
        QWriteLocker wlock(&Responce::pairInfoCacheAccessRW);
        pairInfoCache.clear();
        while(sql.next())
        {
            PairInfo::Ptr info (new PairInfo);
            info->pair = sql.value(0).toString();
            info->decimal_places = sql.value(1).toInt();
            info->min_price = Rate(sql.value(2).toString().toStdString());
            info->max_price = Rate(sql.value(3).toString().toStdString());
            info->min_amount = Rate(sql.value(4).toString().toStdString());
            info->hidden = sql.value(6).toBool();
            info->fee = Fee(sql.value(6).toString().toStdString());
            info->pair_id = sql.value(7).toUInt();

            pairInfoCache.insert(info->pair, info);
            ret.append(info);
        }
    }
    return ret;
}

Responce::PairInfo::Ptr Responce::pairInfo(const QString &pair)
{
    PairInfo::Ptr p = nullptr;
    {
        QReadLocker rlock(&Responce::pairInfoCacheAccessRW);
        p = pairInfoCache.object(pair);
        if (p)
            return p;
    }
    {
        QWriteLocker wlock(&Responce::pairInfoCacheAccessRW);
        p = pairInfoCache.object(pair);
        if (p)
            return p;
        
        QSqlQuery sql(db);
        prepareSql(sql, "select min_price, max_price, min_amount, fee, pair_id, decimal_places from pairs where pair=:pair");
        QVariantMap params;
        params[":pair"] = pair;
        if (performSql("get pair :pair info", sql, params, true) && sql.next())
        {
            PairInfo::Ptr info (new PairInfo());
            info->min_price = Rate(sql.value(0).toString().toStdString());
            info->max_price = Rate(sql.value(1).toString().toStdString());
            info->min_amount = Rate(sql.value(2).toString().toStdString());
            info->fee = Fee(sql.value(3).toString().toStdString());
            info->pair_id = sql.value(4).toUInt();
            info->decimal_places = sql.value(5).toInt();

            pairInfoCache.insert(pair, info);
            return info;
        }
        else
        {
            std::cerr << "cannot get pair '" << pair << "' info " << std::endl;
            //throw std::runtime_error("internal database error: cannot get pair info");
            return nullptr;
        }
    }
    return p;
}

QMap<Responce::PairName, Responce::TickerInfo::Ptr> Responce::tickerInfoCache;
QMutex Responce::tickerInfoCacheAccess;

Responce::TickerInfo::Ptr Responce::tickerInfo(const QString& pair)
{
    QMutexLocker lock(&tickerInfoCacheAccess);
    if (!tickerInfoCache.contains(pair) || tickerInfoCache[pair]->updated.secsTo(QDateTime::currentDateTime()) > TICKER_CACHE_EXPIRE_SECONDS)
    {
        QSqlQuery sql(db);
        prepareSql(sql, "select high, low, avg, vol, vol_cur, last, buy, sell, updated from ticker t left join pairs p on p.pair_id = t.pair_id where p.pair=:name");
        QVariantMap params;
        params[":name"] = pair;
        if (performSql("get ticker for pair ':name'", sql, params) && sql.next())
        {
            TickerInfo::Ptr info (new TickerInfo);

            info->high = Rate(sql.value(0).toString().toStdString());
            info->low  = Rate(sql.value(1).toString().toStdString());
            info->avg  = Rate(sql.value(2).toString().toStdString());
            info->vol  = Amount(sql.value(3).toString().toStdString());
            info->vol_cur  = Amount(sql.value(4).toString().toStdString());
            info->last = Amount(sql.value(5).toString().toStdString());
            info->buy = Amount(sql.value(6).toString().toStdString());
            info->sell = Amount(sql.value(7).toString().toStdString());
            info->updated = sql.value(8).toDateTime();

            tickerInfoCache[pair] = info;
            return info;
        }
        else
        {
            std::cerr << "cannot get pair '" << pair << "' ticker info " << std::endl;
            //throw std::runtime_error("internal database error: cannot get pair ticker info");
            return nullptr;
        }
    }
    return tickerInfoCache[pair];
}

QCache<Responce::OrderId, Responce::OrderInfo::Ptr> Responce::orderInfoCache;
QReadWriteLock Responce::orderInfoCacheRWAccess;

Responce::OrderInfo::Ptr Responce::orderInfo(Responce::OrderId order_id)
{
    OrderInfo::Ptr* p = nullptr;
    {
        QReadLocker rlock(&orderInfoCacheRWAccess);
        info = Responce::orderInfoCache.object(order_id);
        if (info)
            return *info;
    }
    
    {
        QWriteLocker wlock(&orderInfoCacheRWAccess);
        info = Responce::orderInfoCache.object(order_id);
        if (info)
            return *info;
        
        QSqlQuery sql(db);
        prepareSql(sql, "select p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status+0, o.owner_id from orders o left join pairs p on p.pair_id = o.pair_id where o.order_id=:order_id");
        QVariantMap params;
        params[":order_id"] = order_id;
        if (performSql("get info for order :order_id", sql, params) && sql.next())
        {
            OrderInfo::Ptr* info = new OrderInfo::Ptr(new OrderInfo);
            (*info)->pair = sql.value(0).toString();
            //info->pair_ptr = pairInfo(pair);
            (*info)->type = (sql.value(1).toString() == "sell")?OrderInfo::Type::Sell : OrderInfo::Type::Buy;
            (*info)->start_amount = Amount(sql.value(2).toString().toStdString());
            (*info)->amount = Amount(sql.value(3).toString().toStdString());
            (*info)->rate = Rate(sql.value(4).toString().toStdString());
            (*info)->created = sql.value(5).toDateTime();
            (*info)->status = static_cast<OrderInfo::Status>(sql.value(6).toInt());
            (*info)->owner_id = sql.value(7).toUInt();
            (*info)->order_id = order_id;

            orderInfoCache.insert(order_id, info);
            return *info;
        }
        else
            return nullptr;
    }
    return *orderInfoCache[order_id];
}

Responce::OrderInfo::List Responce::activeOrdersInfoList(const QString& apikey)
{
    QSqlQuery sql(db);
    OrderInfo::List list;
    prepareSql(sql, "select o.order_id, p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status, o.owner_id from orders o left join apikeys a  on o.owner_id=a.owner_id left join pairs p on p.pair_id = o.pair_id where a.apikey=:apikey and o.status = 'active'");
    QVariantMap params;
    params[":apikey"] = apikey;
    if (performSql("get active orders for key ':apikey'", sql, params))
        while(sql.next())
        {
            Responce::OrderId order_id = sql.value(0).toUInt();
            OrderInfo::Ptr* pinfo = orderInfoCache.object(order_id);
            if (!pinfo)
            {
                pinfo = new OrderInfo::Ptr(new OrderInfo);
                (*pinfo)->pair = sql.value(1).toString();
                //info->pair_ptr = pairInfo(pair);
                (*pinfo)->type = (sql.value(2).toString() == "sell")?OrderInfo::Type::Sell : OrderInfo::Type::Buy;
                (*pinfo)->start_amount = Amount(sql.value(3).toString().toStdString());
                (*pinfo)->amount = Amount(sql.value(4).toString().toStdString());
                (*pinfo)->rate = Rate(sql.value(5).toString().toStdString());
                (*pinfo)->created = sql.value(6).toDateTime();
                (*pinfo)->status = OrderInfo::Status::Active;
                (*pinfo)->owner_id = sql.value(7).toUInt();
                (*pinfo)->order_id = order_id;

                orderInfoCache.insert(order_id, pinfo);
            }
            list.append(*pinfo);
        }
    return list;
}

QMap<Responce::PairName, QList<QList<QPair<Responce::Rate, Responce::Amount>>, QList<QPair<Responce::Rate, Responce::Amount>>>> Responce::allBuyOrdersAmountAgreggatedByRateList(const QList<PairName>& pairs)
{
    QSqlQuery sql(db);
    QMap<PairName, QList<QPair<Rate, Amount>>> map;
    prepareSql(sql, "select pair, type, rate, sum(amount) from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and type = 'buy' and p.pair in (:pairs) group by pair, type, rate order by  pair,  type, rate desc");
    QVariantMap params;
    params[":pairs"] = pairs;
    if (performSql("get active buy orders for pairs ':pairs'", sql, params))
    {
        while(sql.next())
        {
            QString pair = sql.value(0).toString();
            OrderInfo::Type type = (type=="buy")?OrderInfo::Type::Buy:OrderInfo::Type::Sell;
            Rate rate = Rate(sql.value(2).toString().toStdString());
            Amount sumAmount = Amount(sql.value(3).toString().toStdString());

            if (type == OrderInfo::Type::Buy)
                map[pair].first.append(qMakePair(rate, sumAmount));
            else
                map[pair].second.prepend(qMakePair(rate, sumAmount));
        }
    }
    return map;
}

QVariantMap Responce::getDepthResponce(const QueryParser& httpQuery, Method& method)
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
        QVariantMap p;
        p["bids"] = appendDepthToMap(*selectBuyOrdersQuery, pairName, limit);
        p["asks"] = appendDepthToMap(*selectSellOrdersQuery, pairName, limit);
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

QVariantMap Responce::getPrivateInfoResponce(const QueryParser &httpQuery, Method& method)
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

    QVector<Responce::OrderInfo::Ptr> list = activeOrdersInfoList(httpQuery.key());
    QVariantMap result;
    for(Responce::OrderInfo::Ptr& info: list)
    {
        Responce::OrderId order_id = info->order_id;
        QVariantMap order;
        order["pair"]   = info->pair;
        order["type"]   = (info->type == Responce::OrderInfo::Type::Sell)?"sell":"buy";
        order["amount"] = info->amount.getAsDouble();
        order["rate"]   = info->rate.getAsDouble();
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

    OrderInfo::Ptr info = orderInfo(order_id.toUInt());
    QVariantMap result;
    if (info)
    {
        QVariantMap order;
        order["pair"] = info->pair;
        order["type"] = (info->type == OrderInfo::Type::Buy)?"buy":"sell";
        order["start_amount"] = info->start_amount.getAsDouble();
        order["amount"] = info->start_amount.getAsDouble();
        order["rate"] = info->rate.getAsDouble();
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
        try
        {
            startTransaction->exec();
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
            OrderCreateResult ret = createTrade(httpQuery.key(), httpQuery.pair(), type, httpQuery.rate(), httpQuery.amount());
            if (!ret.ok)
            {
                rollbackTransaction->exec();
                var["success"] = 0;
                var["error"] = ret.errMsg;
                done = true;
            }
            else
            {
                commitTransaction->exec();
                done = true;
                var["success"] = 1;
                QVariantMap res;
                res["remains"] = ret.remains.getAsDouble();
                res["received"] = ret.recieved.getAsDouble();
                res["order_id"] = ret.order_id;

                QVariantMap funds;
                QVariantMap params;
                params[":key"] = httpQuery.key();
                if (performSql("get funds", *selectFundsInfoQuery, params, true))
                {
                    while(selectFundsInfoQuery->next())
                    {
                        QString currency = selectFundsInfoQuery->value(0).toString();
                        funds[currency] = selectFundsInfoQuery->value(1);
                    }
                    res["funds"] = funds;
                }
                var["return"] = res;
            }
        }
        catch(std::runtime_error& e)
        {
            std::cerr << e.what();
            rollbackTransaction->exec();
        }
        catch (const QSqlQuery& q)
        {
            std::cerr << q.lastError().text() << std::endl;
            rollbackTransaction->exec();
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
            OrderInfo::Ptr info = orderInfo(order_id.toUInt());
            if (info)
            {
                // p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status+0
                QString pair = info->pair;
                OrderInfo::Type type = info->type;
                Amount amount = info->amount;
                Rate rate = info->rate;
                OrderInfo::Status status = info->status;
                quint32 owner_id = info->owner_id;

                if (status != OrderInfo::Status::Active)
                {
                    var["success"] = 0;
                    var["error"] = "not active order";
                    return var;
                }
                NewOrderVolume orderVolume = new_order_currency_volume(type, pair, amount, rate);
                tradeUpdateDeposit(owner_id, orderVolume.currency, orderVolume.volume, QString::number(owner_id));

                params[":owner_id"] = owner_id;
                if (performSql("get funds", *selectFundsInfoQueryByOwnerId, params, true))
                {
                    while(selectFundsInfoQueryByOwnerId->next())
                    {
                        QString currency = selectFundsInfoQueryByOwnerId->value(0).toString();
                        funds[currency] = selectFundsInfoQueryByOwnerId->value(1);
                    }
                    ret["funds"] = funds;
                }
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
    performSql("get deposits balance", *totalBalance, "", true);
    while(totalBalance->next())
        balance[totalBalance->value(0).toString()] = totalBalance->value(1).toFloat();


    return balance;
}
