#include "sqlclient.h"
#include "utils.h"
#include <QCache>
#include <QSqlQuery>
#include <QVariant>

QByteArray DirectSqlDataAccessor::randomKeyWithPermissions( bool info, bool trade, bool withdraw)
{
    QVariantMap params;
    params[":info"] = info;
    params[":trade"] = trade;
    params[":withdraw"] = withdraw;

    QSqlQuery getRandomKey(db);
    prepareSql(getRandomKey, "select apikey from apikeys a left join users o on o.user_id = a.user_id where o.user_type=1 and a.info=:info and a.trade=:trade and a.withdraw = :withdraw order by rand() limit 1");

    if (!performSql("get apikey", getRandomKey, params, true))
        return QByteArray();

    if (!getRandomKey.next())
        return QByteArray();

    return getRandomKey.value(0).toByteArray();
}

/// Return random key that has more then 'amount' of 'currency'
QByteArray DirectSqlDataAccessor::randomKeyForTrade(const QString& currency, const Amount& amount)
{
    QVariantMap params;
    params[":currency"] = currency;
    params[":amount"] = dec2qstr(amount, 7);

    QSqlQuery getRandomKeyWithBalance(db);
    prepareSql(getRandomKeyWithBalance, "select apikey from apikeys a left join deposits d  on d.user_id = a.user_id left join currencies c on d.currency_id = c.currency_id  left join users o on o.user_id = a.user_id where o.user_type = 1 and a.trade=true  and c.currency=:currency and d.volume>:amount order by rand() limit 1");
    if (!performSql("get apikey", getRandomKeyWithBalance, params, true))
    {
        std::cerr << "fail to perform sql" << std::endl;
        return QByteArray();
    }

    if (!getRandomKeyWithBalance.next())
    {
        std::cerr << "empty set returned" << std::endl;
        return QByteArray();
    }

    return getRandomKeyWithBalance.value(0).toByteArray();
}

QByteArray DirectSqlDataAccessor::signWithKey(const QByteArray& message, const ApiKey& key)
{
    QByteArray secret = secretForKey(key);
    if (!secret.isEmpty())
        return hmac_sha512(message, secret).toHex();
    return QByteArray();
}

QByteArray DirectSqlDataAccessor::secretForKey(const ApiKey& key)
{
    ApikeyInfo::Ptr info = apikeyInfo(key);
    if (info)
        return info->secret;
    return QByteArray();
}

bool DirectSqlDataAccessor::updateNonce(const ApiKey &key, quint32 nonce)
{
    QSqlQuery updateNonceQuery(db);
    updateNonceQuery.prepare("update apikeys set nonce=:nonce where apikey=:key");
    QVariantMap params;
    params[":key"] = key;
    params[":nonce"] = nonce;
    return performSql("update nonce", updateNonceQuery, params, true);
}

Amount DirectSqlDataAccessor::getDepositCurrencyVolume(const ApiKey& key, const QString &currency)
{
    QSqlQuery sql(db);
    prepareSql(sql, "select d.volume from deposits d left join apikeys a on a.user_id=d.user_id left join currencies c on c.currency_id=d.currency_id where a.apikey=:apikey and c.currency=:currency");
    QVariantMap params;
    params[":apikey"] = key;
    params[":currency"] = currency;
    if (performSql("get :currency volume for key :apikey", sql, params, true) && sql.next())
        return Amount(sql.value(0).toString().toStdString());
    return Amount(0);

}

Amount DirectSqlDataAccessor::getOrdersCurrencyVolume(const ApiKey &key, const QString &currency)
{
    Q_UNUSED(key)
    Q_UNUSED(currency)
    throw "not implemented yet";
}

OrderInfo::List DirectSqlDataAccessor::negativeAmountOrders()
{
    OrderInfo::List list;
    QSqlQuery sql(db);
    sql.exec("SELECT order_id from orders where amount<0");
    while(sql.next())
    {
        list.append(orderInfo(sql.value(0).toUInt()));
    }
    return list;
}

void DirectSqlDataAccessor::updateTicker()
{
    QSqlQuery sql1(db);
    QSqlQuery sql2(db);

    prepareSql(sql1, "select max(o.rate) as high, min(o.rate) as low, avg(o.rate) as avg, sum(t.amount) as vol, sum(t.amount * o.rate) as vol_cur, p.pair  from trades t left join orders o on o.order_id=t.order_id left join pairs p on p.pair_id=o.pair_id where now()-t.created < 60 * 60 * 4  group by o.pair_id");
    prepareSql(sql2, "update ticker t left join pairs p on p.pair_id=t.pair_id set high=:high, low=:low, avg=:avg, vol=:vol, vol_cur=:vol_cur, updated=:updated, last=avg, buy=avg, sell=avg where p.pair=:pair");

    if (sql1.exec())
    {
        while (sql1.next())
        {
            QVariantMap params;
            params[":high"] = sql1.value(0).toDouble();
            params[":low"]  = sql1.value(1).toDouble();
            params[":avg"] = sql1.value(2).toDouble();
            params[":vol"] = sql1.value(3).toDouble();
            params[":vol_cur"] = sql1.value(4).toDouble();
            params[":pair"] = sql1.value(5).toString();
            params[":updated"] = QDateTime::currentDateTime();

            performSql("update ticker for pair ':pair'", sql2, params, true);
        }
    }

}

bool DirectSqlDataAccessor::transaction() { return db.transaction();}

bool DirectSqlDataAccessor::commit() { return db.commit();}

bool DirectSqlDataAccessor::rollback() { return db.rollback(); }

DirectSqlDataAccessor::DirectSqlDataAccessor(QSqlDatabase &db)
    :db(db)
{
}

DirectSqlDataAccessor::~DirectSqlDataAccessor()
{

}

PairInfo::List DirectSqlDataAccessor::allPairsInfoList()
{
    QSqlQuery sql(db);
    PairInfo::List ret;
    if (performSql("retrieve all pairs info", sql, "select pair, decimal_places, min_price, max_price, min_amount, hidden, fee, pair_id from pairs", true))
    {
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

            ret.append(info);
        }
    }
    return ret;

}

PairInfo::Ptr DirectSqlDataAccessor::pairInfo(const PairName& pair)
{
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

        return info;
    }
    else
    {
        std::cerr << "cannot get pair '" << pair << "' info " << std::endl;
        //throw std::runtime_error("internal database error: cannot get pair info");
        return nullptr;
    }
}

TickerInfo::Ptr DirectSqlDataAccessor::tickerInfo(const PairName& pairName)
{
    QSqlQuery sql(db);
    prepareSql(sql, "select high, low, avg, vol, vol_cur, last, buy, sell, updated from ticker t left join pairs p on p.pair_id = t.pair_id where p.pair=:name");
    QVariantMap params;
    params[":name"] = pairName;
    if (performSql("get ticker for pair ':name'", sql, params, true) && sql.next())
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
        info->pairName = pairName;

        return info;
    }
    else
    {
        std::cerr << "cannot get pair '" << pairName << "' ticker info " << std::endl;
        //throw std::runtime_error("internal database error: cannot get pair ticker info");
        return nullptr;
    }

}

OrderInfo::Ptr DirectSqlDataAccessor::orderInfo(OrderId order_id)
{
    QSqlQuery sql(db);
    prepareSql(sql, "select p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status+0, o.user_id from orders o left join pairs p on p.pair_id = o.pair_id where o.order_id=:order_id");
    QVariantMap params;
    params[":order_id"] = order_id;
    if (performSql("get info for order :order_id", sql, params, true) && sql.next())
    {
        OrderInfo::Ptr* info = new OrderInfo::Ptr(new OrderInfo);
        (*info)->pair = sql.value(0).toString();
        (*info)->type = (sql.value(1).toString() == "sell")?OrderInfo::Type::Sell : OrderInfo::Type::Buy;
        (*info)->start_amount = Amount(sql.value(2).toString().toStdString());
        (*info)->amount = Amount(sql.value(3).toString().toStdString());
        (*info)->rate = Rate(sql.value(4).toString().toStdString());
        (*info)->created = sql.value(5).toDateTime();
        (*info)->status = static_cast<OrderInfo::Status>(sql.value(6).toInt());
        (*info)->user_id = sql.value(7).toUInt();
        (*info)->order_id = order_id;

        return *info;
    }
    else
        return nullptr;
}

OrderInfo::List DirectSqlDataAccessor::activeOrdersInfoList(const QString &apikey)
{
    QSqlQuery sql(db);
    OrderInfo::List list;
    prepareSql(sql, "select o.order_id, p.pair, o.type, o.start_amount, o.amount, o.rate, o.created, o.status, o.user_id from orders o left join apikeys a  on o.user_id=a.user_id left join pairs p on p.pair_id = o.pair_id where a.apikey=:apikey and o.status = 'active'");
    QVariantMap params;
    params[":apikey"] = apikey;
    if (performSql("get active orders for key ':apikey'", sql, params, true))
        while(sql.next())
        {
            OrderId order_id = sql.value(0).toUInt();
            OrderInfo::Ptr info (new OrderInfo);
            info->pair = sql.value(1).toString();
            //info->pair_ptr = pairInfo(pair);
            info->type = (sql.value(2).toString() == "sell")?OrderInfo::Type::Sell : OrderInfo::Type::Buy;
            info->start_amount = Amount(sql.value(3).toString().toStdString());
            info->amount = Amount(sql.value(4).toString().toStdString());
            info->rate = Rate(sql.value(5).toString().toStdString());
            info->created = sql.value(6).toDateTime();
            info->status = OrderInfo::Status::Active;
            info->user_id = sql.value(7).toUInt();
            info->order_id = order_id;

            list.append(info);
        }
    return list;
}

TradeInfo::List DirectSqlDataAccessor::allTradesInfo(const PairName &pair)
{
    QSqlQuery sql(db);
    TradeInfo::List list;
    prepareSql(sql, "select o.type, o.rate, t.amount, t.trade_id, t.created, t.order_id, t.user_id from trades t left join orders o on o.order_id=t.order_id left join pairs p on o.pair_id=p.pair_id where p.pair=:pair  order by t.trade_id desc");
    QVariantMap params;
    params[":pair"] = pair;
    performSql("get all trades for pair ':pair'", sql, params, true);
    while (sql.next())
    {
        TradeInfo::Ptr info (new TradeInfo);
        if (sql.value(0).toString() == "buy")
            info->type = TradeInfo::Type::Bid;
        else if (sql.value(0).toString() == "sell")
            info->type = TradeInfo::Type::Ask;
        info->rate = Rate(sql.value(1).toString().toStdString());
        info->amount = Amount(sql.value(2).toString().toStdString());
        info->tid = sql.value(3).toUInt();
        info->created = sql.value(4).toDateTime();
        info->order_id = sql.value(5).toUInt();
        info->user_id = sql.value(6).toUInt();

        list.append(info);
    }
    return list;
}

ApikeyInfo::Ptr DirectSqlDataAccessor::apikeyInfo(const ApiKey &apikey)
{
    QSqlQuery sql(db);
    QVariantMap params;
    params[":key"] = apikey;
    prepareSql(sql, "select info, trade, withdraw, user_id, secret, nonce from apikeys where apikey=:key");
    if ( performSql("get info for key ':key'", sql, params, true) && sql.next())
    {
        ApikeyInfo::Ptr info (new ApikeyInfo);
        info->apikey = apikey;
        info->info = sql.value(0).toBool();
        info->trade = sql.value(1).toBool();
        info->withdraw = sql.value(2).toBool();
        info->user_id = sql.value(3).toUInt();
        info->secret = sql.value(4).toByteArray();
        info->nonce = sql.value(5).toUInt();

        return info;
    }
    return nullptr;
}

UserInfo::Ptr DirectSqlDataAccessor::userInfo(UserId user_id)
{
    QSqlQuery sql(db);
    prepareSql(sql, "select c.currency, d.volume, u.name from deposits d left join currencies c on c.currency_id=d.currency_id left join users u on u.user_id=d.user_id where u.user_id=:user_id");
    QVariantMap params;
    params[":user_id"] = user_id;
    if (performSql("get user name and deposits for user id ':user_id'", sql, params, true))
    {
        UserInfo::Ptr info (new UserInfo);
        info->user_id = user_id;
        while(sql.next())
        {
            info->name = sql.value(2).toString();
            info->funds.insert(sql.value(0).toString(), Amount(sql.value(1).toString().toStdString()));
        }
        return info;
    }
    return nullptr;
}

QMap<PairName, BuySellDepth> DirectSqlDataAccessor::allActiveOrdersAmountAgreggatedByRateList(const QList<PairName> &pairs)
{
    QSqlQuery sql(db);
    QMap<PairName, BuySellDepth> map;
    QString strSql = "select pair, type, rate, sum(amount) from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and p.pair in ('%1') group by pair, type, rate order by  pair,  type, rate desc";
    QStringList lst = pairs;
    strSql = strSql.arg(lst.join("', '"));
    if (performSql("get active buy orders for pairs ':pairs'", sql, strSql, true))
    {
        while(sql.next())
        {
            QString pair = sql.value(0).toString();
            OrderInfo::Type type = (sql.value(1).toString()=="buy")?OrderInfo::Type::Buy:OrderInfo::Type::Sell;
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

bool DirectSqlDataAccessor::tradeUpdateDeposit(const UserId &user_id, const QString &currency, const Amount& diff, const QString &userName)
{
    QSqlQuery sql(db);
    prepareSql(sql, "update deposits d left join currencies c on c.currency_id=d.currency_id set d.volume = volume + :diff where user_id=:user_id and c.currency=:currency");
    QVariantMap updateDepParams;
    updateDepParams[":user_id"] = user_id;
    updateDepParams[":currency"] = currency;
    updateDepParams[":diff"] = dec2qstr(diff, 7);
    bool ok;
    ok = performSql("update ':currency' amount by :diff for user :user_id", sql, updateDepParams, true);
    if (ok)
    {
//        std::clog << "\t\t" << QString("%1: %2 %3 %4")
//                     .arg(userName)
//                     .arg((diff.sign() == -1)?"lost":"recieved")
//                     .arg(QString::number(qAbs(diff.getAsDouble()), 'f', 6))
//                     .arg(currency.toUpper())
//                  << std::endl;
    }
    return ok;
}

bool DirectSqlDataAccessor::reduceOrderAmount(OrderId order_id, const Amount& amount)
{
    QSqlQuery sql(db);
    prepareSql(sql, "update orders set amount=amount-:diff where order_id=:order_id");
    QVariantMap params;
    params[":diff"] = dec2qstr(amount, 7);
    params[":order_id"] = order_id;
    if (!performSql("reduce :order_id amount by :diff", sql, params, true))
        return false;

//    std::clog << "\t\tOrder " << order_id << " amount changed by " << amount << std::endl;
    return true;
}

bool DirectSqlDataAccessor::closeOrder(OrderId order_id)
{
    QSqlQuery sql(db);
    prepareSql(sql, "update orders set amount=0, status='done' where order_id=:order_id");
    QVariantMap params;
    params[":order_id"] = order_id;
    if (!performSql("close order :order_id", sql, params, true))
        return false;

//    std::clog << "\t\tOrder " << order_id << "done" << std::endl;

    return true;
}

bool DirectSqlDataAccessor::createNewTradeRecord(UserId user_id, OrderId order_id, const Amount &amount)
{
    QSqlQuery sql(db);
    prepareSql(sql, "insert into trades (user_id, order_id, amount, created) values (:user_id, :order_id, :amount, :created)");
    QVariantMap params;
    params[":user_id"] = user_id;
    params[":order_id"] = order_id;
    params[":created"] = QDateTime::currentDateTime();
    params[":amount"] = dec2qstr(amount, 7);
    if (amount < Amount(0))
    {
        throw 1;
    }
    performSql("create new trade for user :user_id", sql, params, true);
    return true;

}

OrderId DirectSqlDataAccessor::createNewOrderRecord(const PairName &pair, const UserId &user_id, OrderInfo::Type type, const Rate& rate, const Amount& start_amount)
{
    QSqlQuery sql(db);
    QVariantMap params;
    params[":pair_id"]  = pairInfo(pair)->pair_id;
    params[":user_id"]  = user_id;
    params[":type"]     = (type == OrderInfo::Type::Buy)?"buy":"sell";
    params[":rate"]     = dec2qstr(rate, pairInfo(pair)->decimal_places);
    params[":start_amount"] = dec2qstr(start_amount, 7);
    params[":created"] = QDateTime::currentDateTime();
    if (!prepareSql(sql, "insert into orders (pair_id, user_id, type, rate, start_amount, amount, created, status) values (:pair_id, :user_id, :type, :rate, :start_amount, :start_amount, :created, 'active')"))
        return static_cast<OrderId>(-1);
    if (!performSql("create new ':pair' order for user :user_id as :amount @ :rate", sql, params, true ))
        return static_cast<OrderId>(-1);

//    std:: << "new " << ((type == OrderInfo::Type::Buy)?"buy":"sell") << " order for " << start_amount << " @ " << rate << " created" << std::endl;

    return sql.lastInsertId().toUInt();
}


#define TICKER_CACHE_EXPIRE_SECONDS 300

QMap<PairName, PairInfo::Ptr>   LocalCachesSqlDataAccessor::pairInfoCache;
QMap<PairName, TickerInfo::Ptr> LocalCachesSqlDataAccessor::tickerInfoCache;
QCache<OrderId, OrderInfo::Ptr> LocalCachesSqlDataAccessor::orderInfoCache;
QCache<ApiKey, ApikeyInfo::Ptr> LocalCachesSqlDataAccessor::apikeyInfoCache;
QCache<UserId, UserInfo::Ptr>   LocalCachesSqlDataAccessor::userInfoCache;

QMutex LocalCachesSqlDataAccessor::pairInfoCacheRWAccess;
QMutex LocalCachesSqlDataAccessor::tickerInfoCacheRWAccess;
QMutex LocalCachesSqlDataAccessor::orderInfoCacheRWAccess;
QMutex LocalCachesSqlDataAccessor::apikeyInfoCacheRWAccess;
QMutex LocalCachesSqlDataAccessor::userInfoCacheRWAccess;

LocalCachesSqlDataAccessor::LocalCachesSqlDataAccessor(QSqlDatabase &db)
    :DirectSqlDataAccessor(db)
{

}

LocalCachesSqlDataAccessor::~LocalCachesSqlDataAccessor()
{

}

PairInfo::List LocalCachesSqlDataAccessor::allPairsInfoList()
{
    // This function never use cache and always read from DB, invalidating cache
    PairInfo::List ret = DirectSqlDataAccessor::allPairsInfoList();
    QMutexLocker lock(&LocalCachesSqlDataAccessor::pairInfoCacheRWAccess);
    pairInfoCache.clear();
    for(const PairInfo::Ptr& info : ret)
    {
        LocalCachesSqlDataAccessor::pairInfoCache.insert(info->pair, info);
    }
    return ret;
}

PairInfo::Ptr LocalCachesSqlDataAccessor::pairInfo(const PairName& pair)
{
    QMutexLocker rlock(&LocalCachesSqlDataAccessor::pairInfoCacheRWAccess);
    auto p = LocalCachesSqlDataAccessor::pairInfoCache.find(pair);
    if (p != LocalCachesSqlDataAccessor::pairInfoCache.end())
        return p.value();

    PairInfo::Ptr info = DirectSqlDataAccessor::pairInfo(pair);
    if (info)
        LocalCachesSqlDataAccessor::pairInfoCache.insert(pair, info);
    return info;
}

TickerInfo::Ptr LocalCachesSqlDataAccessor::tickerInfo(const PairName& pair)
{
    QMutexLocker lock(&LocalCachesSqlDataAccessor::tickerInfoCacheRWAccess);
    auto p = LocalCachesSqlDataAccessor::tickerInfoCache.find(pair);
    if (     p != LocalCachesSqlDataAccessor::tickerInfoCache.end()
          && p.value()->updated.secsTo(QDateTime::currentDateTime())  > TICKER_CACHE_EXPIRE_SECONDS)
        return p.value();

    TickerInfo::Ptr info = DirectSqlDataAccessor::tickerInfo(pair);
    if (info)
        LocalCachesSqlDataAccessor::tickerInfoCache[pair] = info;
    return info;
}

OrderInfo::Ptr LocalCachesSqlDataAccessor::orderInfo(OrderId order_id)
{
    OrderInfo::Ptr* pinfo = nullptr;
    QMutexLocker lock(&LocalCachesSqlDataAccessor::orderInfoCacheRWAccess);
    pinfo = LocalCachesSqlDataAccessor::orderInfoCache.object(order_id);
    if (pinfo)
        return *pinfo;

    pinfo = new OrderInfo::Ptr(DirectSqlDataAccessor::orderInfo(order_id));
    if (pinfo)
        LocalCachesSqlDataAccessor::orderInfoCache.insert(order_id, pinfo);
    return *pinfo;
}

ApikeyInfo::Ptr LocalCachesSqlDataAccessor::apikeyInfo(const ApiKey& apikey)
{
    QMutexLocker lock(&LocalCachesSqlDataAccessor::apikeyInfoCacheRWAccess);
    ApikeyInfo::Ptr* info = LocalCachesSqlDataAccessor::apikeyInfoCache.object(apikey);
    if (info)
        return *info;

    info = new ApikeyInfo::Ptr (DirectSqlDataAccessor::apikeyInfo(apikey));
    if (info)
        LocalCachesSqlDataAccessor::apikeyInfoCache.insert(apikey, info);
    return *info;
}


UserInfo::Ptr LocalCachesSqlDataAccessor::userInfo(UserId user_id)
{
    QMutexLocker rlock(&LocalCachesSqlDataAccessor::userInfoCacheRWAccess);
    UserInfo::Ptr* info = LocalCachesSqlDataAccessor::userInfoCache.object(user_id);
    if (info)
        return *info;

    info = new UserInfo::Ptr(DirectSqlDataAccessor::userInfo(user_id));
    if (info)
        LocalCachesSqlDataAccessor::userInfoCache.insert(user_id, info);
    return *info;
}

bool LocalCachesSqlDataAccessor::tradeUpdateDeposit(const UserId& user_id, const QString& currency, const Amount& diff, const QString& userName)
{
    bool ok = DirectSqlDataAccessor::tradeUpdateDeposit(user_id, currency, diff, userName);
    if (ok)
    {
        UserInfo::Ptr* info = nullptr;
        {
            QMutexLocker lock(&LocalCachesSqlDataAccessor::userInfoCacheRWAccess);
            info = LocalCachesSqlDataAccessor::userInfoCache.object(user_id);
        }
        if (info)
        {
            QMutexLocker lock(&(*info)->updateAccess);
            (*info)->funds[currency] += diff;
        }
    }
    return ok;
}

bool LocalCachesSqlDataAccessor::reduceOrderAmount(OrderId order_id, const Amount& amount)
{
    bool ok = DirectSqlDataAccessor::reduceOrderAmount(order_id, amount);
    if (ok)
    {
        OrderInfo::Ptr* info = nullptr;
        {
            QMutexLocker lock(&LocalCachesSqlDataAccessor::orderInfoCacheRWAccess);
            info = LocalCachesSqlDataAccessor::orderInfoCache.object(order_id);
        }
        if (info)
        {
            QMutexLocker lock(&(*info)->updateAccess);
            (*info)->amount -= amount;
        }
    }
    return ok;
}

bool LocalCachesSqlDataAccessor::closeOrder(OrderId order_id)
{
    bool ok = DirectSqlDataAccessor::closeOrder(order_id);
    if (ok)
    {
        QMutexLocker lock(&LocalCachesSqlDataAccessor::orderInfoCacheRWAccess);
        LocalCachesSqlDataAccessor::orderInfoCache.remove(order_id);
    }
    return ok;
}

OrderId LocalCachesSqlDataAccessor::createNewOrderRecord(const PairName& pair, const UserId& user_id, OrderInfo::Type type, const Rate& rate, const Amount& start_amount)
{
    OrderId id = DirectSqlDataAccessor::createNewOrderRecord(pair, user_id, type, rate, start_amount);
    if (id)
    {
        OrderInfo::Ptr* pinfo = new OrderInfo::Ptr(new OrderInfo);
        (*pinfo)->pair = pair;
        (*pinfo)->user_id = user_id;
        (*pinfo)->type = type;
        (*pinfo)->rate = rate;
        (*pinfo)->start_amount = start_amount;
        (*pinfo)->amount = start_amount;
        (*pinfo)->order_id = id;
        (*pinfo)->created = QDateTime::currentDateTime();
        (*pinfo)->status = OrderInfo::Status::Active;

        QMutexLocker lock(&LocalCachesSqlDataAccessor::orderInfoCacheRWAccess);
        LocalCachesSqlDataAccessor::orderInfoCache.insert(id, pinfo);
    }
    return id;
}

bool LocalCachesSqlDataAccessor::updateNonce(const ApiKey& key, quint32 nonce)
{
    bool ok  = DirectSqlDataAccessor::updateNonce(key, nonce);
    if (ok)
    {
        ApikeyInfo::Ptr* info = nullptr;
        {
            QMutexLocker lock(&LocalCachesSqlDataAccessor::apikeyInfoCacheRWAccess);
            info = LocalCachesSqlDataAccessor::apikeyInfoCache.object(key);
        }
        if (info)
        {
            QMutexLocker lock(&(*info)->updateAccess);
            (*info)->nonce = nonce;
        }
    }
    return ok;
}

bool LocalCachesSqlDataAccessor::rollback()
{
    {
        QMutexLocker lock(&userInfoCacheRWAccess);
        userInfoCache.clear();
    }
    {
        QMutexLocker lock(&orderInfoCacheRWAccess);
        orderInfoCache.clear();
    }
    return DirectSqlDataAccessor::rollback();
}
