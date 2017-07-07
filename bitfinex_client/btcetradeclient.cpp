#include "btcetradeclient.h"
#include "btce.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

BtcETradeClient::BtcETradeClient()
{
    db = new QSqlDatabase(QSqlDatabase::addDatabase("QMYSQL", "btce_conn"));

    db->setDatabaseName("rates");
    db->setHostName("mysql-master.vm.dweber.lan");
    db->setPassword("rates");
    db->setUserName("rates");

    qDebug() << "Connecting to database";
    if (!db->open())
    {
        qCritical() << db->lastError().text();
    }

    query = new QSqlQuery(*db);
    if (!query->prepare("insert into rates (exchange, exch_id, pair, time, rate, amount, type) values (:exchange, :exch_id, :pair, :time, :rate, :amount, :type) on duplicate key update exch_id=exch_id"))
    {
        qCritical() << query->lastError().text();
    }

    QString serverAddress = "https://btc-e.com";
    BtcPublicApi::Api::setServer(serverAddress);

    BtcPublicApi::Info btceInfo;
    btceInfo.performQuery();
}

void BtcETradeClient::run()
{
    BtcPublicApi::Trades trades(500);
    while (true)
    {
        trades.performQuery();

        for (BtcObjects::Pair& pair: BtcObjects::Pairs::ref())
        {
            //db->transaction();
            QList<BtcObjects::Trade>& tradeList = pair.trades;
            for (const BtcObjects::Trade& trade: tradeList)
            {
                query->bindValue(":exchange", "btc-e");
                query->bindValue(":exch_id", trade.id);
                query->bindValue(":pair", pair.name);
                query->bindValue(":time", trade.timestamp.toUTC());
                query->bindValue(":rate", trade.price);
                query->bindValue(":amount", trade.amount);
                query->bindValue(":type", trade.type==BtcObjects::Trade::Type::Ask?"sell":"buy");

                if (!query->exec())
                {
                    qWarning() << query->lastError().text();
                }
            }
           // db->commit();
            pair.trades.clear();
        }

        sleep(10);
    }
}
