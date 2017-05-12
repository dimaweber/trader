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

QVariantMap getResponce(QSqlDatabase& db, const QueryParser& parser, Method& method);
QVariantMap getResponce(QSqlDatabase& db, const QString& url, Method& method);

QVariantMap getInfoResponce(QSqlDatabase& database, Method& method);
QVariantMap getTickerResponce(QSqlDatabase& database, const QueryParser& httpQuery, Method& method);
QVariantMap getDepthResponce(QSqlDatabase& database, const QueryParser& httpQuery, Method& method);
QVariantMap getTradesResponce(QSqlDatabase& database, const QueryParser& httpQuery, Method& method);

QVariantMap getPrivateInfoResponce(QSqlDatabase& database, const QueryParser& httpQuery, Method &method);

#endif // RESPONCE_H
