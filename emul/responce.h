#ifndef RESPONCE_H
#define RESPONCE_H

#include <QVariantMap>

class QueryParser;
class QSqlDatabase;
class QSqlQuery;

enum Method {Invalid, PublicInfo, PublicTicker, PublicDepth, PublicTrades, PrivateGetInfo};

QVariantMap getResponce(QSqlDatabase& db, const QueryParser& parser, Method& method);
QVariantMap getResponce(QSqlDatabase& db, const QString& url, Method& method);

QVariantMap getInfoResponce(QSqlDatabase& database);
QVariantMap getTickerResponce(QSqlDatabase& database, const QueryParser& httpQuery);
QVariantMap getDepthResponce(QSqlDatabase& database, const QueryParser& httpQuery);
QVariantMap getTradesResponce(QSqlDatabase& database, const QueryParser& httpQuery);

QVariantMap getPrivateInfoResponce(QSqlDatabase& database, const QueryParser& httpQuery);

#endif // RESPONCE_H
