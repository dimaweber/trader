#include "client.h"
#include <QDebug>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonParseError>

Client::Client(QObject *parent) : QObject(parent)
{
    connect (&pingTimer, &QTimer::timeout, this, &Client::onTimer);
    pingTimer.setSingleShot(false);
    pingTimer.start(10 * 1000);

    QUrl url("wss://api.bitfinex.com/ws/");
    wsocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect (wsocket, &QWebSocket::connected, this, &Client::onConnectWSocket);
    connect (wsocket, &QWebSocket::textMessageReceived, this, &Client::onMessage);
    wsocket->open(url);
}

void Client::subscribeTradesChannel(const QString& pair)
{
    QString msg = QString("{'event':'subscribe', 'channel':'trades', 'pair':'%1'}").arg(pair).replace("'", "\"");
    wsocket->sendTextMessage(msg);
}

void Client::subscribeTickerChannel(const QString& pair)
{
    QString msg = QString("{'event':'subscribe', 'channel':'ticker', 'pair':'%1'}").arg(pair).replace("'", "\"");
    wsocket->sendTextMessage(msg);
}

void Client::onConnectWSocket()
{
    qDebug() << "web socket connected";
}

void Client::onMessage(const QString& msg)
{
//    qDebug() << "[BF ==> cl] " << msg;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qDebug() << "fail to parse json from server: " << error.errorString();
    }
    else
    {
        QVariant v = doc.toVariant();
        if (v.canConvert(QVariant::Map))
        {
            QVariantMap m = v.toMap();
            QString event;
            if (m.contains("event"))
                event = m["event"].toString();
            if (event == "info")
            {
                subscribeTickerChannel("BTCUSD");
                subscribeTickerChannel("LTCUSD");
                subscribeTradesChannel("BTCUSD");
            }
            if (event == "subscribed")
            {
                if (!m.contains("channel"))
                    qDebug() << "no channel name in subscribed reply";
                else
                {
                    QString channelName = m["channel"].toString();
                    quint32 chanId = m["chanId"].toUInt();
                    if (channelName == "ticker")
                    {
                        ChannelMessageHandler* handler = new TickerChannelMessageHandler(chanId, m["pair"].toString());
                        channelHandlers[chanId] = handler;
                    }
                    if (channelName == "trades")
                    {
                        ChannelMessageHandler* handler = new TradeChannelMessageHandler(chanId, m["pair"].toString());
                        channelHandlers[chanId] = handler;
                    }
                }
            }
        }
        else if (v.canConvert(QVariant::List))
        {
            QVariantList list = v.toList();
            quint32 chanId = list[0].toUInt();
            channelHandlers[chanId]->processMessage(list);
        }
        else
        {
            qDebug() << "broken json reply" << v.toString();
        }
    }
}

void Client::onTimer()
{
    QString pingMsg = "{\"event\":\"ping\"}";
    //qDebug() << "[BF <== cl] " << pingMsg;
    wsocket->sendTextMessage(pingMsg);
}

bool TickerChannelMessageHandler::processMessage(const QVariantList &msg)
{
    if (ChannelMessageHandler::processMessage(msg))
        return true;

    if (msg.length() == 11)
    {
        float bid = msg[1].toFloat(); //	Price of last highest bid
        float bid_size = msg[2].toFloat(); // Size of the last highest bid
        float ask = msg[3].toFloat(); //	Price of last lowest ask
        float ask_size = msg[4].toFloat(); //	Size of the last lowest ask
        float daily_change = msg[5].toFloat(); //	Amount that the last price has changed since yesterday
        float daily_change_perc = msg[6].toFloat(); //	Amount that the price has changed expressed in percentage terms
        float last_price = msg[7].toFloat(); //	Price of the last trade.
        float volume = msg[8].toFloat(); //	Daily volume
        float high = msg[9].toFloat(); //	Daily high
        float low = msg[10].toFloat(); //	Daily low

        qDebug() << "pair: " << pair
                 << "   last:" << last_price
                 << "   high:" << high
                 << "   low:" << low;

        return true;
    }
    return false;
}

bool TradeChannelMessageHandler::processMessage(const QVariantList &msg)
{
    if (ChannelMessageHandler::processMessage(msg))
        return true;


    auto printTrade = [this](quint32 id, int timestamp, float price, float amount)
    {
        qDebug() << QDateTime::fromTime_t(timestamp).toString(Qt::ISODate) << pair << (amount>0?"buy":"sell") << qAbs(amount) << '@' << price << "[" << id << "]";
    };

    if (msg[1].canConvert(QVariant::List))
    {
        QVariantList recordsList = msg[1].toList();
        for(const QVariant& rec: recordsList)
        {
            QVariantList lst = rec.toList();
            //snapshot
            if (lst.size() == 4)
            {
                int idx = 0;
                quint32 id = lst[idx++].toUInt();
                int timestamp = lst[idx++].toInt();
                float price = lst[idx++].toFloat();
                float amount = lst[idx++].toFloat();

                printTrade(id, timestamp, price, amount);
            }
            else
            {
                qWarning() << "something wrong";
            }
        }
    }
    else
    {
        int idx=1;
        QString tue = msg[idx++].toString();
        QString seq = msg[idx++].toString();
        quint32 id = 0;
        if (tue=="tu")
        {
            id = msg[idx++].toUInt();
        }
        int timestamp = msg[idx++].toInt();
        float price = msg[idx++].toFloat();
        float amount = msg[idx++].toFloat();

        if (tue == "tu")
            printTrade(id, timestamp, price, amount);
    }
    return false;

}

bool ChannelMessageHandler::processMessage(const QVariantList &msg)
{
    if (msg[1].toString() == "hb")
    {
        // heartbeat
        return true;
    }
    return false;
}
