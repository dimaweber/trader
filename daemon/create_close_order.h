#ifndef CREATE_CLOSE_ORDER_H
#define CREATE_CLOSE_ORDER_H

#include "sql_database.h"
#include "btce.h"

bool create_order (Database& database, quint32 round_id, const QString& pair, BtcObjects::Order::Type type, double rate, double amount, BtcObjects::Funds& funds, bool silent_http, bool silent_sql);
BtcTradeApi::OrderInfo cancel_order(Database& database, BtcObjects::Order::Id order_id, BtcObjects::Funds& funds, bool silent_http, bool silent_sql);

#endif // CREATE_CLOSE_ORDER_H
