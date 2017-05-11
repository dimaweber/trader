#include "responce.h"
#include "query_parser.h"
#include "sql_database.h"

#include <QSqlQuery>
#include <QDateTime>

QVariantMap getResponce(QSqlDatabase& db, const QueryParser& parser, Method& method)
{
    QString methodName = parser.method();
    QVariantMap var;

    QueryParser::Scope scope = parser.apiScope();
    if (scope == QueryParser::Scope::Public)
    {
        if (methodName == "info")
        {
            var = getInfoResponce(db);
            method = PublicInfo;
        }
        else if (methodName == "ticker" )
        {
            var = getTickerResponce(db, parser);
            method = PublicTicker;
        }
        else if (methodName == "depth")
        {
            var = getDepthResponce(db, parser);
            method = PublicDepth;
        }
        else if (methodName == "trades")
        {
            var = getTradesResponce(db, parser);
            method = PublicTrades;
        }
    }
    else if (scope == QueryParser::Scope::Private)
    {
        static Authentificator auth(db);
        QString authErrMsg;
        QString key = parser.key();
        if (!auth.authOk(key, parser.sign(), parser.nonce(), parser.signedData(), authErrMsg))
        {
            var["success"] = 0;
            var["error"] = authErrMsg;
            method = Invalid;
        }
        else if (methodName == "getInfo")
        {
            if (auth.hasInfo(key))
            {
                var = getPrivateInfoResponce(db, parser);
                method = PrivateGetInfo;
            }
            else
            {
                var["success"] = 0;
                var["error"] = "api key dont have info permission";
            }
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

QVariantMap getInfoResponce(QSqlDatabase& database)
{
    QVariantMap var;
    QVariantMap pairs;
    QSqlQuery query(database);
    var["server_time"] = QDateTime::currentDateTime().toTime_t();
    if (performSql("get info", query, "select pair, decimal_places, min_price, max_price, min_amount, hidden, fee from pairs", true))
    {
        while (query.next())
        {
            QVariantMap pair;
            pair["decimal_places"] = query.value(1).toInt();
            pair["min_price"] = query.value(2).toFloat();
            pair["max_price"] = query.value(3).toFloat();
            pair["min_amount"] = query.value(4).toFloat();
            pair["hidden"] = query.value(5).toInt();
            pair["fee"] = query.value(6).toFloat();

            pairs[query.value(0).toString()] = pair;
        }
    }
    var["pairs"] = pairs;
    return var;
}

QVariantMap getTickerResponce(QSqlDatabase& database, const QueryParser& httpQuery)
{
    QVariantMap var;
    QSqlQuery query(database);
    QVariantMap tickerParams;
    query.prepare("select high, low, avg, vol, vol_cur, last, buy, sell, updated from ticker t left join pairs p on p.pair_id = t.pair_id where p.pair=:name");
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
        if (performSql("get ticker", query, tickerParams, true))
        {
            if (query.next())
            {
                QVariantMap pair;
                pair["high"] = query.value(0);
                pair["low"] = query.value(1);
                pair["avg"] = query.value(2);
                pair["vol"] = query.value(3);
                pair["vol_cur"] = query.value(4);
                pair["last"] = query.value(5);
                pair["buy"] = query.value(6);
                pair["sell"] = query.value(7);
                pair["updated"] = query.value(8);

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

void appendDepthToMap(QVariantMap& var, QSqlQuery& query, const QString& pairName, int limit)
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

QVariantMap getDepthResponce(QSqlDatabase& database, const QueryParser& httpQuery)
{
    QVariantMap var;
    int limit = httpQuery.limit();
    QSqlQuery bidsQuery(database);
    QSqlQuery asksQuery(database);
    bidsQuery.prepare("select type, rate, sum(amount), decimal_places from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and type='bids' and p.pair=:name group by pair, type, rate order by pair, type, rate desc");
    asksQuery.prepare("select type, rate, sum(amount), decimal_places from orders o left join pairs p on p.pair_id=o.pair_id where status = 'active' and type='asks' and p.pair=:name group by pair, type, rate order by pair, type, rate asc");
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
        appendDepthToMap(var, bidsQuery, pairName, limit);
        appendDepthToMap(var, asksQuery, pairName, limit);
    }
    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Empty pair list";
    }
    return var;
}

QVariantMap getTradesResponce(QSqlDatabase &database, const QueryParser &httpQuery)
{
    QVariantMap var;
    int limit = httpQuery.limit();
    QSqlQuery query(database);
    query.prepare("select o.type, o.rate, t.amount, t.trade_id, t.created from trades t left join orders o on o.order_id=t.order_id left join pairs p on o.pair_id=p.pair_id where p.pair=:name  order by t.trade_id desc");
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
        if (performSql("select trades", query, tradesParams, true))
        {
            QVariantList list;
            QVariantMap tr;
            QString type;
            while(query.next())
            {

                if (list.size() >= limit)
                    break;

                if (query.value(0).toString() == "asks")
                    type = "bid";
                else
                    type = "ask";

                tr["type"] = type;
                tr["price"] = query.value(1);
                tr["amount"] = query.value(2);
                tr["tid"] = query.value(3);
                tr["timestamp"] = query.value(4).toDateTime().toTime_t();

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

    if (var.isEmpty())
    {
        var["success"] = 0;
        var["error"] = "Empty pair list";
    }
    return var;

}

QVariantMap getResponce(QSqlDatabase &db, const QString &url, Method &method)
{
    QueryParser parser(url);
    return getResponce(db, parser, method);
}

QVariantMap getPrivateInfoResponce(QSqlDatabase &database, const QueryParser &httpQuery)
{
    QVariantMap var;
    QVariantMap funds;
    QVariantMap rights;
    static std::unique_ptr<QSqlQuery> getFundsInfoQuery;
    static std::unique_ptr<QSqlQuery> getRightsInfoQuery;
    static std::unique_ptr<QSqlQuery> getActiveOrdersCountQuery;

    if (!getFundsInfoQuery)
    {
        getFundsInfoQuery.reset(new QSqlQuery(database));
        getFundsInfoQuery->prepare("select c.currency, d.volume from deposits d left join currencies c on c.currency_id=d.currency_id left join apikeys a on a.owner_Id=d.owner_id where apikey=:key");
    }
    if (!getRightsInfoQuery)
    {
        getRightsInfoQuery.reset(new QSqlQuery(database));
        getRightsInfoQuery->prepare("select info,trade,withdraw from apikeys where apikey=:key");
    }
    if (!getActiveOrdersCountQuery)
    {
        getActiveOrdersCountQuery.reset(new QSqlQuery(database));
        getActiveOrdersCountQuery->prepare("select count(*) from apikeys a left join orders o on o.owner_id=a.owner_id where a.apikey=:key and o.status = 0");
    }

    QVariantMap params;
    params[":key"] = httpQuery.key();
    if (performSql("get funds", *getFundsInfoQuery, params, true))
    {
        while(getFundsInfoQuery->next())
        {
            QString currency = getFundsInfoQuery->value(0).toString();
            funds[currency] = getFundsInfoQuery->value(1);
        }
        var["funds"] = funds;
    }
    if (performSql("get rights", *getRightsInfoQuery, params))
    {
        if (getRightsInfoQuery->next())
        {
            rights["info"] = getRightsInfoQuery->value(0).toInt();
            rights["trade"] = getRightsInfoQuery->value(1).toInt();
            rights["withdraw"] = getRightsInfoQuery->value(2).toInt();
        }
        var["rights"] = rights;
    }
    if (performSql("get orders count", *getActiveOrdersCountQuery, params))
    {
        if (getActiveOrdersCountQuery->next())
            var["open_orders"] = getActiveOrdersCountQuery->value(0).toUInt();
    }
    if (var.contains("rights") && var.contains("funds") && var.contains("open_orders"))
    {
        var["success"] = 1;
        var["transaction_count"] = 0;
        var["server_time"] = QDateTime::currentDateTime().toTime_t();
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
