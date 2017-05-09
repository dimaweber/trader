#ifndef RESPONCE_H
#define RESPONCE_H

#include <QVariantMap>

class QueryParser;
class QSqlDatabase;
class QSqlQuery;

enum Method {Invalid, PublicInfo, PublicTicker, PublicDepth};
QVariantMap getResponce(QSqlDatabase& db, const QueryParser& parser, Method& method);
QVariantMap getInfoResponce(QSqlQuery& query);
QVariantMap getTickerResponce(QSqlQuery& query, const QueryParser& httpQuery);
QVariantMap getDepthResponce(QSqlQuery& query, const QueryParser& httpQuery);

#endif // RESPONCE_H
