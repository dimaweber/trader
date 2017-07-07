#include "btcetradeclient.h"
#include "btce.h"


#include <QDebug>

BtcETradeClient::BtcETradeClient()
{
    pDb = new RatesDB;

    QString serverAddress = "https://btc-e.com";
    BtcPublicApi::Api::setServer(serverAddress);

    BtcPublicApi::Info btceInfo;
    btceInfo.performQuery();
}

BtcETradeClient::~BtcETradeClient()
{
    delete pDb;
}

void BtcETradeClient::run()
{
    BtcPublicApi::Trades trades(500);
    while (true)
    {
        trades.performQuery();

        for (BtcObjects::Pair& pair: BtcObjects::Pairs::ref())
        {
            QList<BtcObjects::Trade>& tradeList = pair.trades;
            for (const BtcObjects::Trade& trade: tradeList)
            {
                pDb->newRate("btc-e", trade.id, pair.name, trade.timestamp.toUTC(), trade.price, trade.amount, trade.type==BtcObjects::Trade::Type::Ask?"sell":"buy");
            }
            pair.trades.clear();
        }

        sleep(10);
    }
}
