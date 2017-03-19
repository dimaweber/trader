#include "btce.h"
#include "utils.h"
#include "key_storage.h"
#include "sql_database.h"

#include <QCoreApplication>
#include <QString>
#include <QDateTime>
#include <QtMath>
#include <QElapsedTimer>
#include <QRegExp>
#include <QVariant>
#include <QVector>

#include <QFileInfo>

#include <QSettings>

#include <iostream>

#include <unistd.h>
#include <signal.h>


/// TODO: util to add buy orders to rounds (params: setting_id rate amount)
/// TODO: dep / rates to separate db
/// TODO: transactions this db, but separate process
/// TODO: db_check as callable function, app just wrapp it
/// TODO: sanity checks severity -- critical, repairable, warning
/// TODO: parallel processing secrets
/// TODO: switch to backup db


static CurlWrapper w;

static bool exit_asked = false;

void sig_handler(int signum)
{
    if (signum == SIGINT)
        exit_asked = true;
}

BtcTradeApi::OrderInfo cancel_order(Database& database, BtcObjects::Order::Id order_id, BtcObjects::Funds& funds, bool silent_http, bool silent_sql)
{
    BtcTradeApi::CancelOrder cancel(database, funds, order_id);
    performTradeRequest(QString("cancel order %1").arg(order_id), cancel, silent_http);
    BtcTradeApi::OrderInfo info(database, order_id);
    if (performTradeRequest("get canceled order info", info, silent_http))
    {
        QVariantMap upd_param;
        upd_param[":order_id"] = order_id;
        upd_param[":status"] = info.order.status;
        upd_param[":amount"] = info.order.amount;
        upd_param[":start_amount"] = info.order.start_amount;
        upd_param[":rate"] = info.order.rate;

        /// BUG: if STOP here -- db inconsistent with exchanger!!!!!
        performSql(QString("update order %1").arg(order_id), *database.updateSetCanceled, upd_param, silent_sql);
    }

    return info;
}

bool create_order (Database& database, quint32 round_id, const BtcObjects::Pair& pair, BtcObjects::Order::OrderType type, double rate, double amount, BtcObjects::Funds& funds, bool silent_http, bool silent_sql)
{
    static quint32 auto_executed_counter = 1;
    QVariantMap insertOrderParam;
    BtcTradeApi::Trade order(database, funds, pair.name, type, rate, amount);
    if (performTradeRequest(QString("create %1 order %2 @ %3").arg("sell").arg(amount).arg(rate), order, silent_http))
    {
        insertOrderParam[":order_id"] = (order.order_id==0)?(round_id*1000+auto_executed_counter++):order.order_id;
        insertOrderParam[":status"] = (order.order_id==0)?ORDER_STATUS_INSTANT:ORDER_STATUS_ACTIVE;
        insertOrderParam[":type"] = (type==BtcObjects::Order::Buy)?"buy":"sell";
        insertOrderParam[":amount"] = order.remains;
        insertOrderParam[":start_amount"] = order.received + order.remains;
        insertOrderParam[":rate"] = rate;
        insertOrderParam[":round_id"] = round_id;

        /// BUG: if STOP here -- db inconsistent with exchanger!!!!!
        return performSql("insert sell order record", *database.insertOrder, insertOrderParam, silent_sql);
    }
    return false;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sig_handler);

    QCoreApplication app(argc, argv);

    QString iniFilePath = QCoreApplication::applicationDirPath() + "/../data/trader.ini";
    if (!QFileInfo(iniFilePath).exists())
    {
        throw std::runtime_error("*** No INI file!");
    }
    QSettings settings(iniFilePath, QSettings::IniFormat);

    Database database(settings);
    database.init();
    if (database.isDbUpgradePerformed())
    {
        std::clog << " *** Database has been upgraded, please run db_check, check warning and re-run this app" << std::endl;
        return 0;
    }

    BtcTradeApi::enableTradeLog(QCoreApplication::applicationDirPath() + "/../data/trade.log");

    QMap<int, BtcObjects::Funds> allFunds;
    BtcPublicApi::Info pinfo;
    BtcPublicApi::Ticker pticker;
    BtcPublicApi::Depth pdepth(1000);

    std::clog << "get currencies info ...";
    if (!pinfo.performQuery())
        std::clog << "fail" << std::endl;
    else
        std::clog << "ok" << std::endl;

    int settings_id = 1;
    quint32 round_id = 0;
    quint32 prev_round_id = 0;
    double dep = 3.00;
    double first_step = 0.01;
    double martingale = 0.05;
    double coverage = 0.15;
    int count = 8;
    int secret_id = 0;
    QString currency = "usd";
    QString goods = "ppc";
    double dep_inc = 0;

    QElapsedTimer timer;
    timer.start();

    std::clog << "start main cycle" << std::endl;
    while (!exit_asked)
    {
        settings.sync();
        bool silent_sql = settings.value("debug/silent_sql", true).toBool();
        bool silent_http = settings.value("debug/silent_http", true).toBool();
        QDateTime ratesUpdateTime = QDateTime::currentDateTime();
        try
        {
            timer.restart();
            std::clog << std::endl;
            std::clog << ratesUpdateTime.toString() << std::endl;
            std::clog << "update currencies rate info ...";
            if (!pticker.performQuery())
                std::clog << "fail" << std::endl;
            else
            {
                std::clog << "ok" << std::endl;

                QVariantMap params;
                try
                {
                    database.transaction();
                    for (const BtcObjects::Pair& pair: BtcObjects::Pairs::ref().values())
                    {
                        params[":time"] = ratesUpdateTime; //pair.ticker.updated;
                        params[":currency"] = pair.currency();
                        params[":goods"] = pair.goods();
                        params[":buy"] = pair.ticker.buy;
                        params[":sell"] = pair.ticker.sell;
                        params[":last"] = pair.ticker.last;
                        params[":goods_volume"] = pair.ticker.vol;
                        params[":currency_volume"] = pair.ticker.vol_cur;
                        performSql("Add rate", *database.insertRate, params, silent_sql);
                    }
                    database.commit();
                }
                catch(const QSqlQuery& )
                {
                    database.rollback();
                }
            }

            std::clog << "update orders depth ...";
            if (!pdepth.performQuery())
                std::clog << "fail" << std::endl;
            else
                std::clog << "ok" << std::endl;

            performSql("Mark all active orders as unknown status", *database.updateOrdersCheckToActive, QVariantMap(), silent_sql);

            BtcObjects::Funds onOrders;
            for(int id: database.allKeys())
            {
                std::clog << "for keypair "  << id << " mark active orders status: " ORDER_STATUS_CHECKING " --> " ORDER_STATUS_ACTIVE  << std::endl;
                database.setCurrent(id);

                BtcTradeApi::Info info(database, allFunds[id]);
                performTradeRequest(QString("get funds info for keypair %1").arg(id), info, silent_http);

                BtcTradeApi::ActiveOrders activeOrders(database);
                try
                {
                    if (performTradeRequest(QString("get active orders for keypair %1").arg(id), activeOrders, silent_http))
                    {
                        database.transaction();
                        for (BtcObjects::Order& order: activeOrders.orders)
                        {
                            QVariantMap params;
                            params[":order_id"] = order.order_id;
                            params[":amount"]  = order.amount;
                            params[":rate"] = order.rate;
                            performSql(QString("Order %1 still active, mark it").arg(order.order_id), *database.updateActiveOrder, params, silent_sql);

                            if (order.type == BtcObjects::Order::Buy)
                                onOrders[order.currency()] += order.amount * order.rate;
                            else
                                onOrders[order.goods()] += order.amount;
                        }
                        database.commit();
                    }
                }
                catch (std::runtime_error& e)
                {
                    if (std::string(e.what()) == "no orders")
                    {
                        // no orders yet - just ignore
                    }
                    else
                        throw e;
                }

                BtcObjects::Funds& funds = allFunds[id];
                QVariantMap params;
                params[":secret"] = id;
                try
                {
                    database.transaction();
                    for (const QString& name: funds.keys())
                    {
                        if (name == key_field)
                            continue;

                        double value = funds[name];
                        params[":name"] = name;
                        params[":dep"] = value;
                        params[":time"] = ratesUpdateTime;
                        params[":orders"] = onOrders[name];
                        performSql("Add dep", *database.insertDep, params, silent_sql);
                    }
                    database.commit();
                }
                catch(const QSqlQuery& )
                {
                    database.rollback();
                }

                database.transaction();
                BtcTradeApi::TransHistory hist(database);
                QVariantMap hist_param;
                hist_param[":secret_id"] = id;
                performSql("get max transaction id", *database.selectMaxTransHistoryId, hist_param, silent_sql);
                if (database.selectMaxTransHistoryId->next())
                    hist.setFrom(database.selectMaxTransHistoryId->value(0).toInt());
                hist.setCount(100).setOrder(false);
                try {
                    performTradeRequest("get history", hist, silent_http);
                    QRegExp order_rx (":order:([0-9]*):");
                    for(BtcObjects::Transaction transaction: hist.trans)
                    {
                        QVariantMap ins_params;
                        BtcObjects::Order::Id order_id = 0;
                        if (order_rx.indexIn(transaction.desc) > -1)
                            order_id = order_rx.cap(1).toULongLong();
                        ins_params[":id"] = transaction.id;
                        ins_params[":type"] = transaction.type;
                        ins_params[":amount"] = transaction.amount;
                        ins_params[":currency"] = transaction.currency;
                        ins_params[":description"] = transaction.desc;
                        ins_params[":status"] = transaction.status;
                        ins_params[":timestamp"] = transaction.timestamp;
                        ins_params[":secret_id"] = id;
                        ins_params[":order_id"] = order_id;
                        performSql("insert transaction info", *database.insertTransaction, ins_params, silent_sql);
                    }
                }
                catch (std::runtime_error& )
                {

                }

                database.commit();
            }

            if (!performSql("get settings list", *database.selectSettings, QVariantMap(), silent_sql))
            {
                std::clog << "Sleep for 10 seconds" << std::endl;
                usleep(1000 * 1000 * 10);
                continue;
            }

            while (database.selectSettings->next())
            {
                settings_id = database.selectSettings->value(0).toInt();
//				comission = vault.selectSettings->value(1).toDouble();
                first_step = database.selectSettings->value(2).toDouble();
                martingale = database.selectSettings->value(3).toDouble();
                dep = database.selectSettings->value(4).toDouble();
                coverage = database.selectSettings->value(5).toDouble();
                count = database.selectSettings->value(6).toInt();
                currency = database.selectSettings->value(7).toString();
                goods = database.selectSettings->value(8).toString();
                secret_id = database.selectSettings->value(9).toInt();
                dep_inc = database.selectSettings->value(10).toDouble();

                QString pairName = QString("%1_%2").arg(goods, currency);

                if (!BtcObjects::Pairs::ref().contains(pairName))
                {
                    std::cerr << "no pair" << pairName << "available" <<std::endl;
                    continue;
                }

                database.setCurrent(secret_id);
                BtcObjects::Pair& pair = BtcObjects::Pairs::ref(pairName);
                BtcObjects::Funds& funds = allFunds[secret_id];

                bool round_in_progress = false;

                QVariantMap param;

                param[":settings_id"] = settings_id;

                performSql("get current round id", *database.getRoundId, param, silent_sql);
                if (database.getRoundId->next())
                    round_id = database.getRoundId->value(0).toUInt();
                else
                    round_id = 0;
                param[":round_id"] = round_id;

                performSql("get previous round id", *database.getPrevRoundId, param, silent_sql);
                if (database.getPrevRoundId->next())
                    prev_round_id = database.getPrevRoundId->value(0).toUInt();
                else
                    prev_round_id = 0;
                param[":prev_round_id"] = round_id;


                std::clog << QString("\n -------     Processing settings_id %1. Pair: %2   [%3]   --------------- ").arg(settings_id).arg(pairName).arg(round_id) << std::endl;
                std::clog << QString("Available: %1 %2, %3 %4") .arg(funds[pair.currency()])
                                                                .arg(pair.currency())
                                                                .arg(funds[pair.goods()])
                                                                .arg(pair.goods())
                         << std::endl;

                std::clog << QString("last buy rate: %1. last sell rate: %2").arg(pair.ticker.buy).arg(pair.ticker.sell) << std::endl;

                if (performSql("get active orders count", *database.selectCurrentRoundActiveOrdersCount, param, silent_sql))
                    if (database.selectCurrentRoundActiveOrdersCount->next())
                    {
                        int count = database.selectCurrentRoundActiveOrdersCount->value(0).toInt();
                        round_in_progress = count > 0;
                        std::clog << QString("active orders count: %1").arg(count)  << std::endl;
                    }

                bool sell_order_executed = false;
                database.transaction();
                if (performSql("check if any orders have status changed", *database.selectOrdersWithChangedStatus, param, silent_sql))
                {
                    while (database.selectOrdersWithChangedStatus->next())
                    {
                        BtcObjects::Order::Id order_id = database.selectOrdersWithChangedStatus->value(0).toUInt();
                        BtcTradeApi::OrderInfo info(database, order_id);
                        performTradeRequest(QString("get info for order %1").arg(order_id), info, silent_http);

                        std::clog << QString("order %1 changed status to %2").arg(order_id).arg(info.order.status) << std::endl;
                        QVariantMap upd_param;
                        upd_param[":order_id"] = order_id;
                        upd_param[":status"] = info.order.status;
                        upd_param[":amount"] = info.order.amount;
                        upd_param[":start_amount"] = info.order.start_amount;
                        upd_param[":rate"] = info.order.rate;

                        performSql(QString("update order %1").arg(order_id), *database.updateSetCanceled, upd_param, silent_sql);

                        // orders are sorted by type field, so sell orders come first
                        // if  buy order is changed and sell orders have changed also -- we translate buy order to next round
                        if (info.order.type == BtcObjects::Order::Sell)
                        {
                            std::clog << QString("sell order changed status to %1").arg(info.order.status) << std::endl;
                            sell_order_executed = true;

                            double currency_out = 0;
                            double currency_in = 0;
                            double goods_out = 0;
                            double goods_in = 0;
                            QVariantMap round_upd;
                            round_upd[":round_id"] = round_id;

                            performSql("get round buy stats", *database.roundBuyStat, round_upd, silent_sql);
                            if (database.roundBuyStat->next())
                            {
                                goods_in = database.roundBuyStat->value(0).toDouble();
                                currency_out = database.roundBuyStat->value(1).toDouble();
                            }
                            performSql("get round sell stats", *database.roundSellStat, round_upd, silent_sql);
                            if (database.roundSellStat->next())
                            {
                                goods_out = database.roundSellStat->value(0).toDouble();
                                currency_in = database.roundSellStat->value(1).toDouble();
                            }

                            double income = currency_in - currency_out;
                            round_upd[":income"] = income;
                            round_upd[":c_in"] = currency_in;
                            round_upd[":c_out"] = currency_out;
                            round_upd[":g_in"] = goods_in;
                            round_upd[":g_out"] = goods_out;
                            performSql("update round", *database.updateRound, round_upd, silent_sql);

                            QVariantMap round_close;
                            round_close[":round_id"] = round_id;
                            performSql("archive round", *database.archiveRound, round_close, silent_sql);
                            round_close[":round_id"] = prev_round_id;
                            performSql("close round", *database.closeRound, round_close, silent_sql);

                            QVariantMap dep_upd = param;
                            dep += income * dep_inc;
                            dep_upd[":settings_id"] = settings_id;
                            dep_upd[":dep_inc"] = income * dep_inc;
                            performSql("increase deposit", *database.depositIncrease, dep_upd, silent_sql);

                            prev_round_id = round_id;
                            round_id = 0;
                            round_in_progress = false;
                        }
                        else
                        {
                            // this is buy order
                            if (sell_order_executed)
                            {
                                // and round has finished (thus -- sell order exists)
                                QVariantMap transitParams;
                                transitParams[":order_id"] = order_id;
                                performSql("mark order for transition", *database.markForTransition, transitParams, silent_sql);
                            }
                            else
                                round_in_progress = true;
                        }
                    }
                }
                database.commit();

                double amount_gain = 0;
                double sell_rate = 0;
                performSql("Get current round amoumt gain", *database.selectCurrentRoundGain, param, silent_sql);
                if(database.selectCurrentRoundGain->next())
                {
                    amount_gain = database.selectCurrentRoundGain->value(1).toDouble();
                    double payment = database.selectCurrentRoundGain->value(2).toDouble();
                    sell_rate = database.selectCurrentRoundGain->value(3).toDouble();
                    double profit = database.selectCurrentRoundGain->value(4).toDouble();

                    std::clog << QString("in current round we got %1 %2 and payed %3 %4 for it. To get %6% profit we need to sell it back with rate %5")
                                 .arg(amount_gain).arg(pair.goods()).arg(payment).arg(pair.currency()).arg(sell_rate).arg(profit * 100)
                              << std::endl;

                    // adjust sell rate -- increase it till lowest price that is greater then sell_rate - 0.001
                    // for example we decide to sell @ 122.340, but currently depth looks like 123.000, 122.890, 122.615, 122.112
                    // so there is no point to create 122.340 order, we can create 122.614 order

                    double calculated_sell_rate = sell_rate;
                    double adjusted_sell_rate =  sell_rate + 1;
                    double decimal_fix = 0;
//                  decimal_fix = qPow(10, -pair.decimal_places);
                    for (BtcObjects::Depth::Position& pos: pair.depth.asks)
                    {
                        if (pos.rate > calculated_sell_rate && pos.rate < adjusted_sell_rate)
                            adjusted_sell_rate = pos.rate;
                    }
                    sell_rate = qMax(adjusted_sell_rate - decimal_fix, calculated_sell_rate);

                    std::clog << QString("After depth lookup, we adjusted sell rate to %1").arg(sell_rate) << std::endl;
                }

                if (qFuzzyIsNull(amount_gain))
                {
                    std::clog << "nothing bought yet in this round. check -- may be we should increase buy rates" << std::endl;
                    performSql("Get maximum buy rate", *database.checkMaxBuyRate, param, silent_sql);
                    if (database.checkMaxBuyRate->next())
                    {
                        double rate = database.checkMaxBuyRate->value(0).toDouble();
                        std::clog << QString("max buy rate is %1, last rate is %2").arg(rate).arg(pair.ticker.last) << std::endl;
                        if (rate > 0 && pair.ticker.last > rate)
                        {
                            std::clog << QString("rate for %1 too high (%2)").arg(pair.name).arg(rate) << std::endl;
                            round_in_progress = false;
                        }
                    }
                }

                if (!round_in_progress)
                {
                    performSql("check for orders left from previous round", *database.selectOrdersFromPrevRound, param, silent_sql);
                    while(database.selectOrdersFromPrevRound->next())
                    {
                        BtcObjects::Order::Id order_id = database.selectOrdersFromPrevRound->value(0).toUInt();

                        cancel_order(database, order_id, funds, silent_http, silent_sql);

                    }

                    std::clog << "New round start. Calculate new buy orders parameters" << std::endl;

                    if (round_id == 0)
                    {
                        performSql("create new round record", *database.insertRound, param, silent_sql);
                        round_id = database.insertRound->lastInsertId().toUInt();
                    }

                    double sum =0;

                    for (int j=0; j<count; j++)
                        sum += qPow(1+martingale, j) * ( 1 - first_step - (coverage - first_step) * j/(count-1));

                    double execute_rate = pair.ticker.last;
                    param[":last_price"] = execute_rate;
                    if (performSql("get prev round sell price", *database.selectPrevRoundSellRate, param, silent_sql))
                    {
                        if (database.selectPrevRoundSellRate->next())
                        {
                            execute_rate = database.selectPrevRoundSellRate->value(0).toDouble();
                        }
                    }
                    double u = dep / execute_rate / sum;
                    //u = qMax(qMin(funds[currency], dep) / execute_rate / sum, pair.min_amount / (1-comission));
                    double total_currency_spent = 0;
                    QVariantMap usage_params;
                    usage_params[":round_id"] = round_id;
                    for(int j=0; j<count; j++)
                    {
                        double amount = u * qPow(1+martingale, j);
                        double rate = execute_rate * ( 1 - first_step - (coverage - first_step) * j/(count-1));

                        if (amount * rate > funds[currency] ||
                            total_currency_spent + amount*rate > dep+0.00001)
                        {
                            std::clog << QString("Not enought %1 for full bids").arg(pair.currency()) << std::endl;
                            break;
                        }

                        if (amount < pair.min_amount)
                        {
                            std::clog << "proposed trade amount " << amount << " too low (min: " << pair.min_amount << ") -- skip order" << std::endl;
                        }
                        else
                        {
                            if (create_order(database, round_id, pair, BtcObjects::Order::Buy, rate, amount, funds, silent_http, silent_sql))
                            {
                                total_currency_spent += amount * rate;
                                std::clog << QString("%1 bid: %2@%3").arg(j+1).arg(amount).arg(rate) << std::endl;
                            }
                        }
                    }
                    /// BUG: if STOP here -- db inconsistent with exchanger!!!!! Update dep usage when creating buy order insted -- in transaction!
                    usage_params[":usage"] = total_currency_spent;
                    performSql("set dep_usage", *database.setRoundsDepUsage, usage_params, silent_sql);

                    round_in_progress = true;
                    std::clog << QString("total bid: %1 %2").arg(total_currency_spent).arg(pair.currency())
                              << std::endl;

                    QVariantMap transParams;
                    transParams[":settings_id"] = settings_id;
                    transParams[":round_id_to"] = round_id;
                    transParams[":prev_round_from"] = prev_round_id;
                    performSql("transit orders from previous round", *database.transitOrders, transParams, silent_sql);
                }
                else
                {
                //	std::cout << qPrintable(QString("round for %1(%2) already in progress").arg(settings_id).arg(pair.name)) << std::endl;
                }

                if (round_in_progress)
                {
                    if (pair.min_amount > amount_gain)
                    {
                        std::clog << "An amount we have ("<< amount_gain <<") is less then minimal trade amount ("<< pair.min_amount <<"). skip creating sell order" << std::endl;
                        continue;
                    }

                    bool need_recreate_sell = !sell_order_executed;
                    BtcObjects::Order::Id sell_order_id = 0;
                    performSql("get sell order id and amount", *database.selectSellOrder, param, silent_sql);
                    if (database.selectSellOrder->next())
                    {
                        sell_order_id = database.selectSellOrder->value(0).toUInt();
                        double sell_order_amount = database.selectSellOrder->value(1).toDouble();
                        double sell_order_rate = database.selectSellOrder->value(2).toDouble();
                        double closed_sells_sold_amount = 0;
                        performSql("get canelled sell orders sold amount ", *database.currentRoundPayback, param, silent_sql);
                        if (database.currentRoundPayback->next())
                        {
                            closed_sells_sold_amount = database.currentRoundPayback->value(0).toDouble();
                            std::clog << QString("cancelled sells sold %1 %2").arg(closed_sells_sold_amount).arg(pair.goods()) << std::endl;
                        }

                        need_recreate_sell = (amount_gain - sell_order_amount - closed_sells_sold_amount > pair.min_amount && funds[pair.goods()] > 0)
                                || qAbs(sell_rate - sell_order_rate) > qPow(10, -pair.decimal_places);

                        std::clog << QString("found sell order %1 for %2 amount, %4 rate. Need recreate sell order: %3")
                                     .arg(sell_order_id).arg(sell_order_amount).arg(need_recreate_sell?"yes":"no").arg(sell_order_rate)
                                  << std::endl;

                    }

                    if (need_recreate_sell)
                    {
                        if (sell_order_id > 0)
                        {
                            BtcTradeApi::OrderInfo info = cancel_order(database, sell_order_id, funds, silent_http, silent_sql);
                            if (!qFuzzyCompare(info.order.amount, info.order.start_amount))
                                amount_gain -= (info.order.start_amount - info.order.amount);
                        }

                        amount_gain = qMin(amount_gain, funds[pair.goods()]);

                        if (funds[pair.goods()] - amount_gain < pair.min_amount)
                        {
                            amount_gain = funds[pair.goods()];
                        }

                        if (amount_gain > pair.min_amount)
                        {
                            create_order(database, round_id, pair, BtcObjects::Order::Sell, sell_rate, amount_gain, funds, silent_http, silent_sql);
                        }
                    }
                }
            }
            database.pack_db();

            qint64 t = timer.elapsed();
            std::clog << QString("iteration done in %1 ms").arg(t) << std::endl << std::endl << std::endl;
            qint64 ms_sleep = 10 * 1000;
            if (t < ms_sleep)
                usleep(1000 * (ms_sleep-t));
        }
        catch(const QSqlQuery& e)
        {
            std::cerr << "Fail sql query: " << e.executedQuery() << " [" << e.lastError().number() << "] " << e.lastError().text() << std::endl;
            database.init();
            usleep(1000 * 1000 * 30);
        }
        catch (const BtcTradeApi::Api& e)
        {
            std::cerr << "Fail http query: " << e.error() << std::endl;
            usleep(1000 * 1000 * 60);
        }
        catch (const HttpError& e)
        {
            std::cerr << "Http error: " << e.what() << std::endl;
            usleep(1000 * 1000 * 60);
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Runtime error: " << e.what() << std::endl;
            usleep(1000 * 1000 * 60);
        }
    }
    return 0;
}
