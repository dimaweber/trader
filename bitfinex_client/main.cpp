#include "client.h"
#include "btcetradeclient.h"
#include "pusherclient.h"

#include <QtCore/QCoreApplication>
#include <QtCore/qglobal.h>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Client client;

    BtcETradeClient btceTrade;
    btceTrade.start();

    PusherClient cl;

    return a.exec();
}
