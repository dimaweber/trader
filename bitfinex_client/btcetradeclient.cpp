#include "btcetradeclient.h"
#include "btce.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

BtcETradeClient::BtcETradeClient()
{
    db = new QSqlDatabase(QSqlDatabase::addDatabase("QMYSQL", "btce_conn"));

    db->setDatabaseName("rates");
    db->setHostName("mysql-master.vm.dweber.lan");
    db->setPassword("rates");
    db->setUserName("rates");

    std::cout << "Connecting to database" << std::endl;
    if (!db->open())
    {
        std::cerr << qPrintable(db->lastError().text()) << std::endl;
    }

    query = new QSqlQuery(*db);
    if (!query->prepare("insert into rates (exchange, exch_id, pair, time, rate, amount, type) values (:exchange, :exch_id, :pair, :time, :rate, :amount, :type)"))
    {
        std::cerr << qPrintable(db->lastError().text()) << std::endl;
    }

    QString serverAddress = "https://btc-e.com";
    BtcPublicApi::Api::setServer(serverAddress);

    BtcPublicApi::Info btceInfo;
    btceInfo.performQuery();
}

void BtcETradeClient::run()
{
    BtcPublicApi::Trades trades(500);

    QMap<QString, quint32> id;
    QStringList pairs {"btc_usd", "eth_usd"};
    while (true)
    {
        trades.performQuery();

        for (auto& p: pairs)
        {
            BtcObjects::Pair& pair = BtcObjects::Pairs::ref(p);
            QList<BtcObjects::Trade>& tradeList = pair.trades;
            for (const BtcObjects::Trade& trade: tradeList)
            {
                if (id[p] < trade.id)
                {
                    id[p] = trade.id;
                    query->bindValue(":exchange", "btc-e");
                    query->bindValue(":exch_id", trade.id);
                    query->bindValue(":pair", pair.name);
                    query->bindValue(":time", trade.timestamp.toUTC());
                    query->bindValue(":rate", trade.price);
                    query->bindValue(":amount", trade.amount);
                    query->bindValue(":type", trade.type==BtcObjects::Trade::Type::Ask?"sell":"buy");

                    if (!query->exec())
                    {
                        std::cerr << qPrintable(query->lastError().text()) << std::endl;
                    }
                }
            }
        }

        sleep(10);
    }
}
