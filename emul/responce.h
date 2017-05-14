#ifndef RESPONCE_H
#define RESPONCE_H

#include <QVariantMap>

class QueryParser;
class QSqlDatabase;
class QSqlQuery;

#define EXCHNAGE_OWNER_ID 1000

enum Method {Invalid, AuthIssue, AccessIssue,
             PublicInfo, PublicTicker, PublicDepth, PublicTrades,
             PrivateGetInfo, PrivateTrade, PrivateActiveOrders, PrivateOrderInfo,
             PrivateCanelOrder, PrivateTradeHistory, PrivateTransHistory,
             PrivateCoinDepositAddress,
             PrivateWithdrawCoin, PrivateCreateCupon, PrivateRedeemCupon
            };

bool initialiazeResponce(QSqlDatabase& db);
QVariantMap getResponce(const QueryParser& parser, Method& method);

QByteArray getRandomValidKey(bool info, bool trade, bool withdraw);
QByteArray signWithKey(const QByteArray& message, const QByteArray& key);


QVariantMap exchangeBalance();


#endif // RESPONCE_H
