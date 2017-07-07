#ifndef BTCETRADECLIENT_H
#define BTCETRADECLIENT_H

#include <ratesdb.h>
#include <QThread>


class BtcETradeClient : public QThread
{
    Q_OBJECT

    RatesDB* pDb;
public:
    BtcETradeClient();
    ~BtcETradeClient();
protected:
    void run();
};

#endif // BTCETRADECLIENT_H
