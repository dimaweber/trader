#ifndef RESPONCE_H
#define RESPONCE_H

#include <QVariantMap>

class QueryParser;
class QSqlDatabase;
class QSqlQuery;

enum Method {Invalid, PublicInfo, PublicTicker, PublicDepth, PublicTrades,
             PrivateGetInfo, PrivateTrade, PrivateActiveOrders, PrivateOrderInfo,
             PrivateCanelOrder, PrivateTradeHistory, PrivateTransHistory,
             PrivateCoinDepositAddress,
             PrivateWithdrawCoin, PrivateCreateCupon, PrivateRedeemCupon
            };

bool initialiazeResponce(QSqlDatabase& db);
QVariantMap getResponce(const QueryParser& parser, Method& method);

#endif // RESPONCE_H
