#include "create_close_order.h"

BtcTradeApi::OrderInfo cancel_order(Database& database, BtcObjects::Order::Id order_id, BtcObjects::Funds& funds, bool silent_http, bool silent_sql)
{
    BtcTradeApi::CancelOrder cancel(database, funds, order_id);
    performTradeRequest(QString("cancel order %1").arg(order_id), cancel, silent_http);
    BtcTradeApi::OrderInfo info(database, order_id);
    if (performTradeRequest("get canceled order info", info, silent_http))
    {
        QVariantMap upd_param;
        upd_param[":order_id"] = order_id;
        upd_param[":status"] = info.order.status;
        upd_param[":amount"] = info.order.amount;
        upd_param[":start_amount"] = info.order.start_amount;
        upd_param[":rate"] = info.order.rate;

        /// BUG: if STOP here -- db inconsistent with exchanger!!!!!
        performSql(QString("update order %1").arg(order_id), *database.updateSetCanceled, upd_param, silent_sql);
    }

    return info;
}

bool create_order (Database& database, quint32 round_id, const QString& pair_name, BtcObjects::Order::OrderType type, double rate, double amount, BtcObjects::Funds& funds, bool silent_http, bool silent_sql)
{
    static quint32 auto_executed_counter = 1;
    QVariantMap insertOrderParam;
    BtcTradeApi::Trade order(database, funds, pair_name, type, rate, amount);
    if (performTradeRequest(QString("create %1 order %2 @ %3").arg("sell").arg(amount).arg(rate), order, silent_http))
    {
        insertOrderParam[":order_id"] = (order.order_id==0)?(round_id*1000+auto_executed_counter++):order.order_id;
        insertOrderParam[":status"] = (order.order_id==0)?ORDER_STATUS_INSTANT:ORDER_STATUS_ACTIVE;
        insertOrderParam[":type"] = (type==BtcObjects::Order::Buy)?"buy":"sell";
        insertOrderParam[":amount"] = order.remains;
        insertOrderParam[":start_amount"] = order.received + order.remains;
        insertOrderParam[":rate"] = rate;
        insertOrderParam[":round_id"] = round_id;
        /// BUG: if order is instant -- actual rate might be significally lower/higher then  asked -- this lead to improper sell rate calculation and extra goods on balance.

        /// BUG: if STOP here -- db inconsistent with exchanger!!!!!
        return performSql("insert  order record", *database.insertOrder, insertOrderParam, silent_sql);
    }
    return false;
}
