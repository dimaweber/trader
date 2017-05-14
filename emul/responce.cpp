#include "responce.h"
#include "query_parser.h"
#include "sql_database.h"
#include "utils.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

QAtomicInt Responce::counter = 0;

Responce::Responce(QSqlDatabase& database)
    :db(database)
{
    auth.reset(new Authentificator(db));

    selectTickerInfoQuery.reset( new QSqlQuery(db));
    selectOrderInfoQuery.reset(new QSqlQuery(db));
    selectActiveOrdersQuery.reset(new QSqlQuery(db));
    selectPairsInfoQuery.reset( new QSqlQuery(db));
    selectPairInfoQuery.reset( new QSqlQuery(db));
    selectBuyOrdersQuery.reset( new QSqlQuery(db));
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
    &&  prepareSql(*selectTickerInfoQuery, "select high, low, avg, vol, vol_cur, last, buy, sell, updated from ticker t left join pairs p on p.pair_id = t.pair_id where p.pair=:name")
    &&  prepareSql(*selectPairsInfoQuery, "select pair, decimal_places, min_price, max_price, min_amount, hidden, fee from pairs")
    &&  prepareSql(*selectPairInfoQuery, "select min_price, max_price, min_amount, fee, pair_id from pairs where pair=:pair")
    &&  prepareSql(*selectOrderInfoQuery, "select p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status+0, o.owner_id from orders o left join pairs p on p.pair_id = o.pair_id where o.order_id=:order_id")
    &&  prepareSql(*selectActiveOrdersQuery, "select o.order_id, p.pair, o.type, o.amount, o.rate, o.created, 0 from orders o left join apikeys a  on o.owner_id=a.owner_id left join pairs p on p.pair_id = o.pair_id where a.apikey=:key and o.status = 'active'")
    &&  prepareSql(*selectBuyOrdersQuery, "select rate, sum(amount) from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and type='buy' and p.pair=:name group by rate order by  rate desc")
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

Responce::TradeCurrencyVolume Responce::trade_volumes (const QString& type, const QString& pair, Fee fee,
                                 Amount trade_amount, Rate matched_order_rate)
{
    TradeCurrencyVolume ret;
    ret.currency = pair.right(3);
    ret.goods = pair.left(3);

    Fee contra_fee = Fee(1) - fee;
    if (type == "buy")
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

Responce::NewOrderVolume Responce::new_order_currency_volume (const QString& type, const QString& pair, Amount amount, Rate rate)
{
    NewOrderVolume ret;
    if (type == "sell")
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

QString Responce::oppositOrderType(const QString& type)
{
    if (type=="buy")
        return "sell";
    return "buy";
}

quint32 Responce::doExchange(QString ownerName, const QString& rate, TradeCurrencyVolume volumes, const QString& type, Rate rt, const QString& pair, QSqlQuery& query, Amount& amnt, Fee fee, QVariant owner_id, QVariant pair_id, QVariantMap orderCreateParams)
{
    quint32 ret = 0;
    if (!performSql(QString("get %1 orders").arg(oppositOrderType(type)), query, orderCreateParams, true))
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
        createOrderParams[":type"] = type;
        if (!performSql("create order", *createOrderQuery, createOrderParams, true))
            return (quint32)-1;
//        std::clog << "new " << type << " order for " << amnt << " @ " << rate << " created" << std::endl;

        ret = createOrderQuery->lastInsertId().toUInt();
    }
    return ret;
}

Responce::OrderCreateResult Responce::createTrade(const QString& key, const QString& pair, const QString& type, const QString& rate, const QString& amount)
{
    OrderCreateResult ret;
    ret.recieved = 0;
    ret.remains = 0;
    ret.order_id = 0;
    ret.ok = false;

    QVariantMap orderCreateParams;
    orderCreateParams[":key"] = key;
    bool isSell = false;

    orderCreateParams[":pair"] = pair;
    if (!performSql("get pair info", *selectPairInfoQuery, orderCreateParams, true))
    {
        ret.errMsg = "internal database error";
        return ret;
    }
    if (!selectPairInfoQuery->next())
    {
        ret.errMsg = "You incorrectly entered one of fields.";
        return ret;
    }

    Rate min_price  = Rate(selectPairInfoQuery->value(0).toString().toStdString());
    Rate max_price  = Rate(selectPairInfoQuery->value(1).toString().toStdString());
    Rate min_amount = Rate(selectPairInfoQuery->value(2).toString().toStdString());
    Fee fee         = Fee (selectPairInfoQuery->value(3).toString().toStdString()) / 100;
    QVariant pair_id = selectPairInfoQuery->value(4);;

    if (type == "buy")
        orderCreateParams[":currency"] = pair.right(3);
    else if (type == "sell")
    {
        isSell = true;
        orderCreateParams[":currency"] = pair.left(3);
    }
    else
    {
        ret.errMsg = "You incorrectly entered one of fields.";
        return ret;
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
    if (type == "buy")
        query = selectOrdersForBuyTrade.get();
    else if (type == "sell")
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
            var = getPrivateCalcelOrderResponce(parser, method);
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
                pair["updated"] = selectTickerInfoQuery->value(8).toDateTime().toTime_t();

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
        var["return"] = result;
        var["success"] = 1;
    }

    return var;
}

QVariantMap Responce::getPrivateOrderInfoResponce(const QueryParser &httpQuery, Method& method)
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
            var["return"] = result;
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
            OrderCreateResult ret = createTrade(httpQuery.key(), httpQuery.pair(), httpQuery.orderType(), httpQuery.rate(), httpQuery.amount());
            if (!ret.ok)
            {
                rollbackTransaction->exec();
                var["success"] = 0;
                var["error"] = ret.errMsg;
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

QVariantMap Responce::getPrivateCalcelOrderResponce(const QueryParser& httpQuery, Method& method)
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
            performSql("get order info", *selectOrderInfoQuery, params, true);
            if (selectOrderInfoQuery->next())
            {
                // p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status+0
                QString pair = selectOrderInfoQuery->value(0).toString();
                QString type = selectOrderInfoQuery->value(1).toString();
                Amount amount = Amount(selectOrderInfoQuery->value(3).toString().toStdString());
                Rate rate = Amount(selectOrderInfoQuery->value(4).toString().toStdString());
                uint status = selectOrderInfoQuery->value(6).toUInt();
                quint32 owner_id = selectOrderInfoQuery->value(7).toUInt();

                if (status > 1)
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
