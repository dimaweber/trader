#include "responce.h"
#include "query_parser.h"

#include <QSqlQuery>
#include <QDateTime>

QVariantMap getInfoResponce(QSqlQuery& query)
{
    QVariantMap var;
    QVariantMap pairs;
    var["server_time"] = QDateTime::currentDateTime().toTime_t();
    if (query.exec( "select pair, decimal_places, min_price, max_price, min_amount, hidden, fee from pairs" ))
    {
        while (query.next())
        {
            QVariantMap pair;
            pair["decimal_places"] = query.value(1).toInt();
            pair["min_price"] = query.value(2).toDouble();
            pair["max_price"] = query.value(3).toDouble();
            pair["min_amount"] = query.value(4).toDouble();
            pair["hidden"] = query.value(5).toInt();
            pair["fee"] = query.value(6).toDouble();

            pairs[query.value(0).toString()] = pair;
        }
    }
    var["pairs"] = pairs;
    return var;
}

QVariantMap getTickerResponce(QSqlQuery& query, const QueryParser& httpQuery)
{
    QVariantMap var;
    query.prepare("select high, low, avg, vol, vol_cur, last, buy, sell, updated from ticker where pair=:name");
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
        query.bindValue(":name", pairName);
        if (query.exec())
        {
            if (query.next())
            {
                QVariantMap pair;
                pair["high"] = query.value(0).toFloat();
                pair["low"] = query.value(1).toDouble();
                pair["avg"] = query.value(2).toDouble();
                pair["vol"] = query.value(3).toDouble();
                pair["vol_cur"] = query.value(4).toInt();
                pair["last"] = query.value(5).toDouble();
                pair["buy"] = query.value(6).toFloat();
                pair["sell"] = query.value(7).toFloat();
                pair["updated"] = query.value(8).toInt();

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

QVariantMap getResponce(QSqlDatabase& db, const QueryParser& parser, Method& method)
{
    QString methodName = parser.method();
    QSqlQuery sqlQuery(db);
    QVariantMap var;

    if (methodName == "info")
    {
        var = getInfoResponce(sqlQuery);
        method = PublicInfo;
    }
    else if (methodName == "ticker" )
    {
        var = getTickerResponce(sqlQuery, parser);
        method = PublicTicker;
    }
    else if (methodName == "depth")
    {
        var = getDepthResponce(sqlQuery, parser);
        method = PublicDepth;
    }
    else
    {
        var["success"] = 0;
        var["error"] = "Invalid method";
        method = Invalid;
    }

    return var;
}

QVariantMap getDepthResponce(QSqlQuery& query, const QueryParser& httpQuery)
{
    QVariantMap var;
    query.prepare("select type, rate, sum(start_amount - amount) from active_orders where status = 0 and pair=:name group by pair, type, rate order by pair, type, rate");
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
        query.bindValue(":name", pairName);
        if (query.exec())
        {
            while(query.next())
            {
                QString type = query.value(0).toString();
                float rate = query.value(1).toFloat();
                float amount = query.value(2).toFloat();

                QVariantMap mapPair;
                if (var.contains(pairName))
                    mapPair = var[pairName].toMap();
                QVariantList list;
                if (mapPair.contains(type))
                    list = mapPair[type].toList();
                QVariantList values;
                values << rate << amount;
                QVariant castedValue = values;
                list << castedValue;
                mapPair[type] = list;
                var[pairName] = mapPair;
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
