#ifndef BTCETRADECLIENT_H
#define BTCETRADECLIENT_H

#include <QThread>

class QSqlDatabase;
class QSqlQuery;

class BtcETradeClient : public QThread
{
    Q_OBJECT
    QSqlDatabase* db;
    QSqlQuery* query;
public:
    BtcETradeClient();
protected:
    void run();
};

#endif // BTCETRADECLIENT_H
