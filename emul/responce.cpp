#include "responce.h"
#include "query_parser.h"
#include "sql_database.h"
#include "utils.h"

#include <QCache>
#include <QSqlError>
#include <QSqlQuery>

#define TICKER_CACHE_EXPIRE_SECONDS 300

QAtomicInt Responce::counter = 0;

//QMap<PairName, PairInfo::Ptr> Responce::pairInfoCache;
//QMap<PairName, TickerInfo::Ptr> Responce::tickerInfoCache;
//QCache<OrderId, OrderInfo::Ptr> Responce::orderInfoCache;
//QCache<TradeId, TradeInfo::Ptr> Responce::tradeInfoCache;
//QCache<ApiKey, ApikeyInfo::Ptr> Responce::apikeyInfoCache;
//QCache<UserId, UserInfo::Ptr> Responce::userInfoCache;
//QMutex Responce::pairInfoCacheRWAccess;
//QMutex Responce::tickerInfoCacheRWAccess;
//QMutex Responce::orderInfoCacheRWAccess;
//QMutex Responce::tradeInfoCacheRWAccess;
//QMutex Responce::apikeyInfoCacheRWAccess;
//QMutex Responce::userInfoCacheRWAccess;

Responce::Responce(QSqlDatabase& database)
    :db(database)
{
    dataAccessor = std::make_shared<DirectSqlDataAccessor>(db);
    auth.reset(new Authentificator(dataAccessor));

    selectActiveOrdersCountQuery.reset(new QSqlQuery(db));
    selectOrdersForSellTrade.reset(new QSqlQuery(db));
    selectOrdersForBuyTrade.reset(new QSqlQuery(db));

    cancelOrderQuery.reset(new QSqlQuery(db));

    startTransaction.reset(new QSqlQuery("start transaction", db));
    commitTransaction.reset(new QSqlQuery("commit", db));
    rollbackTransaction.reset(new QSqlQuery("rollback", db));


    prepareSql(*selectActiveOrdersCountQuery, "select count(*) from apikeys a left join orders o on o.user_id=a.user_id where a.apikey=:key and o.status = 'active'");
    prepareSql(*selectOrdersForSellTrade, "select order_id, amount, rate, o.user_id, w.name from orders o left join pairs p on p.pair_id = o.pair_id left join users w on w.user_id=o.user_id where type='buy' and status='active' and pair=:pair and o.user_id<>:user_id and rate >= :rate order by rate desc, order_id asc");
    prepareSql(*selectOrdersForBuyTrade, "select order_id, amount, rate, o.user_id, w.name from orders o left join pairs p on p.pair_id = o.pair_id left join users w on w.user_id=o.user_id where type='sell' and status='active' and pair=:pair and o.user_id<>:user_id and rate <= :rate order by rate asc, order_id asc");

    prepareSql(*cancelOrderQuery, "update orders set status=case when start_amount=amount then 'cancelled' else 'part_done' end where order_id=:order_id");
}

//bool Responce::tradeUpdateDeposit(const UserId& user_id, const QString& currency, Amount diff, const QString& userName)
//{
//    QSqlQuery sql(db);
//    prepareSql(sql, "update deposits d left join currencies c on c.currency_id=d.currency_id set d.volume = volume + :diff where user_id=:user_id and c.currency=:currency");
//    QVariantMap updateDepParams;
//    updateDepParams[":user_id"] = user_id;
//    updateDepParams[":currency"] = currency;
//    updateDepParams[":diff"] = diff);
//    bool ok;
//    ok = performSql("update ':currency' amount by :diff for user :user_id", sql, updateDepParams, true);
//    if (ok)
//    {
//        {
//            QMutexLocker lock(&userInfoCacheRWAccess);
//            UserInfo::Ptr* puser = userInfoCache.object(user_id);
//            if (puser)
//                (*puser)->funds[currency] +=  diff;
//        }
//        std::clog << "\t\t" << QString("%1: %2 %3 %4")
//                     .arg(userName)
//                     .arg((diff.sign() == -1)?"lost":"recieved")
//                     .arg(QString::number(qAbs(diff)), 'f', 6))
//                     .arg(currency.toUpper())
//                  << std::endl;
//    }
//    return ok;
//}

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
        OrderId matched_order_id = query.value(0).toUInt();
        Amount matched_amount = qvar2dec<7>(query.value(1));
        Amount matched_rate   = qvar2dec<7>(query.value(2));
        UserId matched_user_id = query.value(3).toUInt();
        QString matched_userName = query.value(4).toString();

        std::clog << '\t'
                  <<QString("Found %5 order %1 from user %2 for %3 @ %4 ")
                     .arg(matched_order_id).arg(matched_userName)
                     .arg(dec2qstr(matched_amount)).arg(dec2qstr(matched_rate)).arg(matchedOrderType)
                  << std::endl;

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

//bool Responce::reduceOrderAmount(OrderId order_id, Amount amount)
//{
//    QSqlQuery sql(db);
//    prepareSql(sql, "update orders set amount=amount-:diff where order_id=:order_id");
//    QVariantMap params;
//    params[":diff"] = amount);
//    params[":order_id"] = order_id;
//    if (!performSql("reduce :order_id amount by :diff", sql, params, true))
//        return false;

//    {
//        QMutexLocker lock(&orderInfoCacheRWAccess);
//        OrderInfo::Ptr* item = orderInfoCache.object(order_id);
//        if (item)
//            (*item)->amount -= amount;
//    }
//    std::clog << "\t\tOrder " << order_id << " amount changed by " << amount << std::endl;
//    return true;
//}

//bool Responce::closeOrder(OrderId order_id)
//{
//    QSqlQuery sql(db);
//    prepareSql(sql, "update orders set amount=0, status='done' where order_id=:order_id");
//    QVariantMap params;
//    params[":order_id"] = order_id;
//    if (!performSql("close order :order_id", sql, params, true))
//        return false;

//    QMutexLocker lock(&orderInfoCacheRWAccess);
//    orderInfoCache.remove(order_id);
//    std::clog << "\t\tOrder " << order_id << "done" << std::endl;

//    return true;

//}

//bool Responce::createNewTradeRecord(UserId user_id, OrderId order_id, const Amount& amount)
//{
//    QSqlQuery sql(db);
//    prepareSql(sql, "insert into trades (user_id, order_id, amount, created) values (:user_id, :order_id, :amount, :created)");
//    QVariantMap params;
//    params[":user_id"] = user_id;
//    params[":order_id"] = order_id;
//    params[":created"] = QDateTime::currentDateTime();
//    params[":amount"] = amount);
//    performSql("create new trade for user :user_id", sql, params, true);
//    return true;

//}

//OrderId Responce::createNewOrderRecord(const PairName &pair, const UserId &user_id, OrderInfo::Type type, Rate rate, Amount amount)
//{
//    QSqlQuery sql(db);
//    QVariantMap params;
//    params[":pair_id"]  = pairInfo(pair)->pair_id;
//    params[":user_id"]  = user_id;
//    params[":type"]     = (type == OrderInfo::Type::Buy)?"buy":"sell";
//    params[":rate"]     = rate);
//    params[":start_amount"] = amount);
//    params[":created"] = QDateTime::currentDateTime();
//    if (!prepareSql(sql, "insert into orders (pair_id, user_id, type, rate, start_amount, amount, created, status) values (:pair_id, :user_id, :type, :rate, :start_amount, :start_amount, :created, 'active')"))
//        return static_cast<OrderId>(-1);
//    if (!performSql("create new ':pair' order for user :user_id as :amount @ :rate", sql, params, true ))
//        return static_cast<OrderId>(-1);

//    std::clog << "new " << ((type == OrderInfo::Type::Buy)?"buy":"sell") << " order for " << amount << " @ " << rate << " created" << std::endl;

//    return sql.lastInsertId().toUInt();
//}

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

    Rate min_price  = info->min_price;
    Rate max_price  = info->max_price;
    Rate min_amount = info->min_amount;
    Fee fee         = info->fee/Fee(100);

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
        ret.errMsg = QString("Value %1 must be greater than 0.001 %1.")
                .arg(currency.toUpper())
                .arg(dec2qstr(min_amount));
        return ret;
    }

    if (rate < min_price)
    {
        ret.errMsg = QString("Price per %1 must be greater than %2 %3.")
                .arg(pair.left(3).toUpper())
                .arg(dec2qstr(min_price))
                .arg(pair.right(3).toUpper());
        return ret;
    }
    if (rate > max_price)
    {
        ret.errMsg = QString("Price per %1 must be lower than %2 %3.")
                .arg(pair.left(3).toUpper())
                .arg(dec2qstr(max_price))
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

    std::clog << QString("user %1 wants to %2 %3 %4 for %5 %6 (rate %7)")
                 .arg(userName).arg((type == OrderInfo::Type::Buy)?"buy":"sell").arg(dec2qstr(amnt)).arg(pair.left(3).toUpper())
                 .arg(dec2qstr((amnt * rate))).arg(pair.right(3).toUpper()).arg(dec2qstr(rate))
              << std::endl;

    TradeCurrencyVolume volumes;
    QSqlQuery* query = nullptr;
    if (type == OrderInfo::Type::Buy)
        query = selectOrdersForBuyTrade.get();
    else if (type == OrderInfo::Type::Sell)
        query = selectOrdersForSellTrade.get();

    ret.order_id = doExchange(userName, rate, volumes, type, rate, pair, *query, amnt, fee, user_id);
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
        pair["min_price"] = dec2qstr(info->min_price);
        pair["max_price"] = dec2qstr(info->max_price);
        pair["min_amount"] = dec2qstr(info->min_amount);
        pair["hidden"] = info->hidden;
        pair["fee"] = dec2qstr(info->fee);

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
            pair["high"] = dec2qstr(info->high);
            pair["low"]  = dec2qstr(info->low);
            pair["avg"]  = dec2qstr(info->avg);
            pair["vol"]  = dec2qstr(info->vol);
            pair["vol_cur"] = dec2qstr(info->vol_cur);
            pair["last"] = dec2qstr(info->last);
            pair["buy"] = dec2qstr(info->buy);
            pair["sell"] = dec2qstr(info->sell);
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

QVariantList Responce::appendDepthToMap(const Depth& depth, int limit)
{
    QVariantList ret;
    for(QPair<Rate, Amount> item: depth)
    {
            QVariantList values;
            values << dec2qstr(item.first) << dec2qstr(item.second);
            QVariant castedValue = values;
            ret << castedValue;
            if (ret.length() >= limit)
                break;
    }
    return ret;
}

//PairInfo::List Responce::allPairsInfoList()
//{
//    // This function never use cache and always read from DB, invalidating cache
//    QSqlQuery sql(db);
//    PairInfo::List ret;
//    if (performSql("retrieve all pairs info", sql, "select pair, decimal_places, min_price, max_price, min_amount, hidden, fee, pair_id from pairs", true))
//    {
//        QMutexLocker lock(&Responce::pairInfoCacheRWAccess);
//        pairInfoCache.clear();
//        while(sql.next())
//        {
//            PairInfo::Ptr info (new PairInfo);
//            info->pair = sql.value(0).toString();
//            info->decimal_places = sql.value(1).toInt();
//            info->min_price = Rate(sql.value(2).toString().toStdString());
//            info->max_price = Rate(sql.value(3).toString().toStdString());
//            info->min_amount = Rate(sql.value(4).toString().toStdString());
//            info->hidden = sql.value(6).toBool();
//            info->fee = Fee(sql.value(6).toString().toStdString());
//            info->pair_id = sql.value(7).toUInt();

//            pairInfoCache.insert(info->pair, info);
//            ret.append(info);
//        }
//    }
//    return ret;
//}

//PairInfo::Ptr Responce::pairInfo(const QString &pair)
//{
//    QMutexLocker rlock(&Responce::pairInfoCacheRWAccess);
//    auto p = pairInfoCache.find(pair);
//    if (p != pairInfoCache.end())
//        return p.value();

//    QSqlQuery sql(db);
//    prepareSql(sql, "select min_price, max_price, min_amount, fee, pair_id, decimal_places from pairs where pair=:pair");
//    QVariantMap params;
//    params[":pair"] = pair;
//    if (performSql("get pair :pair info", sql, params, true) && sql.next())
//    {
//        PairInfo::Ptr info (new PairInfo());
//        info->min_price = Rate(sql.value(0).toString().toStdString());
//        info->max_price = Rate(sql.value(1).toString().toStdString());
//        info->min_amount = Rate(sql.value(2).toString().toStdString());
//        info->fee = Fee(sql.value(3).toString().toStdString());
//        info->pair_id = sql.value(4).toUInt();
//        info->decimal_places = sql.value(5).toInt();

//        pairInfoCache.insert(pair, info);
//        return info;
//    }
//    else
//    {
//        std::cerr << "cannot get pair '" << pair << "' info " << std::endl;
//        //throw std::runtime_error("internal database error: cannot get pair info");
//        return nullptr;
//    }
//}

//TickerInfo::Ptr Responce::tickerInfo(const QString& pairName)
//{
//    QMutexLocker lock(&tickerInfoCacheRWAccess);
//    auto p = tickerInfoCache.find(pairName);
//    if (p != tickerInfoCache.end() && p.value()->updated.secsTo(QDateTime::currentDateTime())  > TICKER_CACHE_EXPIRE_SECONDS)
//            return p.value();

//    QSqlQuery sql(db);
//    prepareSql(sql, "select high, low, avg, vol, vol_cur, last, buy, sell, updated from ticker t left join pairs p on p.pair_id = t.pair_id where p.pair=:name");
//    QVariantMap params;
//    params[":name"] = pairName;
//    if (performSql("get ticker for pair ':name'", sql, params, true) && sql.next())
//    {
//        TickerInfo::Ptr info (new TickerInfo);

//        info->high = Rate(sql.value(0).toString().toStdString());
//        info->low  = Rate(sql.value(1).toString().toStdString());
//        info->avg  = Rate(sql.value(2).toString().toStdString());
//        info->vol  = Amount(sql.value(3).toString().toStdString());
//        info->vol_cur  = Amount(sql.value(4).toString().toStdString());
//        info->last = Amount(sql.value(5).toString().toStdString());
//        info->buy = Amount(sql.value(6).toString().toStdString());
//        info->sell = Amount(sql.value(7).toString().toStdString());
//        info->updated = sql.value(8).toDateTime();
//        info->pairName = pairName;

//        tickerInfoCache[pairName] = info;
//        return info;
//    }
//    else
//    {
//        std::cerr << "cannot get pair '" << pairName << "' ticker info " << std::endl;
//        //throw std::runtime_error("internal database error: cannot get pair ticker info");
//        return nullptr;
//    }
//}

//OrderInfo::Ptr Responce::orderInfo(OrderId order_id)
//{
//    OrderInfo::Ptr* info = nullptr;
//    QMutexLocker lock(&orderInfoCacheRWAccess);
//    info = Responce::orderInfoCache.object(order_id);
//    if (info)
//        return *info;

//    QSqlQuery sql(db);
//    prepareSql(sql, "select p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status+0, o.user_id from orders o left join pairs p on p.pair_id = o.pair_id where o.order_id=:order_id");
//    QVariantMap params;
//    params[":order_id"] = order_id;
//    if (performSql("get info for order :order_id", sql, params, true) && sql.next())
//    {
//        OrderInfo::Ptr* info = new OrderInfo::Ptr(new OrderInfo);
//        (*info)->pair = sql.value(0).toString();
//        (*info)->type = (sql.value(1).toString() == "sell")?OrderInfo::Type::Sell : OrderInfo::Type::Buy;
//        (*info)->start_amount = Amount(sql.value(2).toString().toStdString());
//        (*info)->amount = Amount(sql.value(3).toString().toStdString());
//        (*info)->rate = Rate(sql.value(4).toString().toStdString());
//        (*info)->created = sql.value(5).toDateTime();
//        (*info)->status = static_cast<OrderInfo::Status>(sql.value(6).toInt());
//        (*info)->user_id = sql.value(7).toUInt();
//        (*info)->order_id = order_id;

//        orderInfoCache.insert(order_id, info);
//        return *info;
//    }
//    else
//        return nullptr;

//}

//OrderInfo::List Responce::activeOrdersInfoList(const QString& apikey)
//{
//    QSqlQuery sql(db);
//    OrderInfo::List list;
//    prepareSql(sql, "select o.order_id, p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status, o.user_id from orders o left join apikeys a  on o.user_id=a.user_id left join pairs p on p.pair_id = o.pair_id where a.apikey=:apikey and o.status = 'active'");
//    QVariantMap params;
//    params[":apikey"] = apikey;
//    if (performSql("get active orders for key ':apikey'", sql, params, true))
//        while(sql.next())
//        {
//            OrderId order_id = sql.value(0).toUInt();
//            OrderInfo::Ptr* pinfo = orderInfoCache.object(order_id);
//            if (!pinfo)
//            {
//                pinfo = new OrderInfo::Ptr(new OrderInfo);
//                (*pinfo)->pair = sql.value(1).toString();
//                //info->pair_ptr = pairInfo(pair);
//                (*pinfo)->type = (sql.value(2).toString() == "sell")?OrderInfo::Type::Sell : OrderInfo::Type::Buy;
//                (*pinfo)->start_amount = Amount(sql.value(3).toString().toStdString());
//                (*pinfo)->amount = Amount(sql.value(4).toString().toStdString());
//                (*pinfo)->rate = Rate(sql.value(5).toString().toStdString());
//                (*pinfo)->created = sql.value(6).toDateTime();
//                (*pinfo)->status = OrderInfo::Status::Active;
//                (*pinfo)->user_id = sql.value(7).toUInt();
//                (*pinfo)->order_id = order_id;

//                orderInfoCache.insert(order_id, pinfo);
//            }
//            list.append(*pinfo);
//        }
//    return list;
//}

//TradeInfo::List Responce::allTradesInfo(const PairName& pair)
//{
//    QSqlQuery sql(db);
//    TradeInfo::List list;
//    prepareSql(sql, "select o.type, o.rate, t.amount, t.trade_id, t.created, t.order_id, t.user_id from trades t left join orders o on o.order_id=t.order_id left join pairs p on o.pair_id=p.pair_id where p.pair=:pair  order by t.trade_id desc");
//    QVariantMap params;
//    params[":pair"] = pair;
//    performSql("get all trades for pair ':pair'", sql, params, true);
//    while (sql.next())
//    {
//        TradeInfo::Ptr* info = new TradeInfo::Ptr(new TradeInfo);
//        if (sql.value(0).toString() == "buy")
//            (*info)->type = TradeInfo::Type::Bid;
//        else if (sql.value(0).toString() == "sell")
//            (*info)->type = TradeInfo::Type::Ask;
//        (*info)->rate = Rate(sql.value(1).toString().toStdString());
//        (*info)->amount = Amount(sql.value(2).toString().toStdString());
//        (*info)->tid = sql.value(3).toUInt();
//        (*info)->created = sql.value(4).toDateTime();
//        (*info)->order_id = sql.value(5).toUInt();
//        {
//            QMutexLocker lock(&orderInfoCacheRWAccess);
//            OrderInfo::Ptr* pOrder = orderInfoCache.object((*info)->order_id);
//            if (pOrder)
//                (*info)->order_ptr = *pOrder;
//        }
//        (*info)->user_id = sql.value(6).toUInt();

//        list.append(*info);

//        QMutexLocker wlock(&tradeInfoCacheRWAccess);
//        tradeInfoCache.insert((*info)->order_id, info);
//    }
//    return list;
//}

//QMap<PairName, BuySellDepth> Responce::allActiveOrdersAmountAgreggatedByRateList(const QList<PairName>& pairs)
//{
//    QSqlQuery sql(db);
//    QMap<PairName, BuySellDepth> map;
//    QString strSql = "select pair, type, rate, sum(amount) from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and p.pair in ('%1') group by pair, type, rate order by  pair,  type, rate desc";
//    QStringList lst = pairs;
//    strSql = strSql.arg(lst.join("', '"));
//    if (performSql("get active buy orders for pairs ':pairs'", sql, strSql, true))
//    {
//        while(sql.next())
//        {
//            QString pair = sql.value(0).toString();
//            OrderInfo::Type type = (sql.value(1).toString()=="buy")?OrderInfo::Type::Buy:OrderInfo::Type::Sell;
//            Rate rate = Rate(sql.value(2).toString().toStdString());
//            Amount sumAmount = Amount(sql.value(3).toString().toStdString());

//            if (type == OrderInfo::Type::Buy)
//                map[pair].first.append(qMakePair(rate, sumAmount));
//            else
//                map[pair].second.prepend(qMakePair(rate, sumAmount));
//        }
//    }
//    return map;
//}

//ApikeyInfo::Ptr Responce::apikeyInfo(const ApiKey& apikey)
//{
//    QMutexLocker lock(&apikeyInfoCacheRWAccess);
//    ApikeyInfo::Ptr* info = apikeyInfoCache.object(apikey);
//    if (info)
//        return *info;

//    QSqlQuery sql(db);
//    QVariantMap params;
//    params[":key"] = apikey;
//    prepareSql(sql, "select info, trade, withdraw, user_id from apikeys where apikey=:key");
//    if ( performSql("get info for key ':key'", sql, params, true) && sql.next())
//    {
//        info = new ApikeyInfo::Ptr (new ApikeyInfo);
//        (*info)->apikey = apikey;
//        (*info)->info = sql.value(0).toBool();
//        (*info)->trade = sql.value(1).toBool();
//        (*info)->withdraw = sql.value(2).toBool();
//        (*info)->user_id = sql.value(3).toUInt();

//        apikeyInfoCache.insert(apikey, info);
//        return *info;
//    }
//    return nullptr;
//}


//UserInfo::Ptr Responce::userInfo(UserId user_id)
//{
//    QMutexLocker rlock(&userInfoCacheRWAccess);
//    UserInfo::Ptr* info = userInfoCache.object(user_id);
//    if (info)
//        return *info;

//    QSqlQuery sql(db);
//    prepareSql(sql, "select c.currency, d.volume, u.name from deposits d left join currencies c on c.currency_id=d.currency_id left join users u on u.user_id=d.user_id where u.user_id=:user_id");
//    QVariantMap params;
//    params[":user_id"] = user_id;
//    if (performSql("get user name and deposits for user id ':user_id'", sql, params, true))
//    {
//        info = new UserInfo::Ptr(new UserInfo);
//        (*info)->user_id = user_id;
//        while(sql.next())
//        {
//            (*info)->name = sql.value(2).toString();
//            (*info)->funds.insert(sql.value(0).toString(), Amount(sql.value(1).toString().toStdString()));
//        }
//        userInfoCache.insert(user_id, info);
//        return *info;
//    }
//    return nullptr;
//}

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
        QVariantMap p;
        p["bids"] = appendDepthToMap(item.value().first, limit);
        p["asks"] = appendDepthToMap(item.value().second,  limit);
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
        for(TradeInfo::Ptr info: tradesList)
        {
            if (list.size() >= limit)
                break;

            if (info->type == TradeInfo::Type::Bid)
                type = "bid";
            else
                type = "ask";

            tr["type"] = type;
            tr["price"] = dec2qstr(info->rate);
            tr["amount"] = dec2qstr(info->amount);
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
        funds[cur] = dec2qstr(f[cur]);
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
        order["pair"]   = info->pair;
        order["type"]   = (info->type == OrderInfo::Type::Sell)?"sell":"buy";
        order["amount"] = dec2qstr(info->amount);
        order["rate"]   = dec2qstr(info->rate);
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
        order["pair"] = info->pair;
        order["type"] = (info->type == OrderInfo::Type::Buy)?"buy":"sell";
        order["start_amount"] = dec2qstr(info->start_amount);
        order["amount"] = dec2qstr(info->start_amount);
        order["rate"] = dec2qstr(info->rate);
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
            Rate rate = Rate(httpQuery.rate().toStdString());
            Amount amount = Amount(httpQuery.amount().toStdString());
            OrderCreateResult ret = checkParamsAndDoExchange(httpQuery.key(), httpQuery.pair(), type, rate, amount);
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
                res["remains"] = dec2qstr(ret.remains);
                res["received"] = dec2qstr(ret.recieved);
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
                    funds[cur] = dec2qstr(f[cur]);
                res["funds"] = funds;
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
