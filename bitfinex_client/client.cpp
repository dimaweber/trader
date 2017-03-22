#include "client.h"
#include <QDebug>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonParseError>

Client::Client(QObject *parent) : QObject(parent)
  ,pingLatency(1), serverProtocol(-1)
{
    connect (&pingTimer, &QTimer::timeout, this, &Client::onTimer);
    pingTimer.setSingleShot(false);
    pingTimer.start(10 * 1000);

    wsocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect (wsocket, &QWebSocket::connected, this, &Client::onConnectWSocket);
    connect (wsocket, &QWebSocket::textMessageReceived, this, &Client::onMessage);

    connect (this, &Client::reconnectRequired, this, &Client::connectServer);
    connect (this, &Client::pongEvent, this, &Client::onPongEvent);
    connect (this, &Client::infoEvent, this, &Client::onInfoEvent);
    connect (this, &Client::errorEvent, this, &Client::onErrorEvent);
    connect (this, &Client::subscribedEvent, this, &Client::onSubscribedEvent);
    connect (this, &Client::unsubscribedEvent, this, &Client::onUnsubscribedEvent);
    connect (this, &Client::resubscribeAllRerquired, this, &Client::resubscribeAll);

    emit reconnectRequired();
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

void Client::unsubscribeChannel(quint32 chanId)
{
    QString msg = QString("{'event':'unsubscribe', 'chanId':'%1'}").arg(chanId).replace("'", "\"");
    wsocket->sendTextMessage(msg);
}

void Client::subscribeAll()
{
    subscribeTickerChannel("BTCUSD");
    subscribeTickerChannel("LTCUSD");
    subscribeTradesChannel("BTCUSD");
}

void Client::unsubscribeAll()
{
    for (quint32 chanId: channelHandlers.keys())
    {
         unsubscribeChannel(chanId);
    }
}

void Client::resubscribeAll()
{
    unsubscribeAll();
    subscribeAll();
}

void Client::onConnectWSocket()
{
    qDebug() << "web socket connected";
}

void Client::onInfoEvent(QVariantMap m)
{
    if (m.contains("version"))
    {
        serverProtocol = m["version"].toInt();
        if (serverProtocol == 1 || serverProtocol == 2)
        {
            subscribeAll();
        }
        else
        {
            qWarning() << "unsupported protocol version " << serverProtocol;
        }
    }
    else if (m.contains("code"))
    {
        int code = m["code"].toInt();
        qDebug() << "[" << code << "]" << m["msg"].toString();
        switch (code)
        {
            case 20051:
                emit reconnectRequired();
                break;
        case 20060:
                break;
        case 20061:
                emit resubscribeAllRerquired();
        }
    }
}

void Client::onSubscribedEvent(QVariantMap m)
{
    if (!m.contains("channel"))
        qDebug() << "no channel name in subscribed reply";
    else
    {
        ChannelMessageHandler* handler = nullptr;
        QString channelName = m["channel"].toString();
        quint32 chanId = m["chanId"].toUInt();

        if (channelName == "ticker")
        {
            if (serverProtocol == 1)
                handler = new TickerChannelMessageHandler(chanId, m["pair"].toString());
            else if (serverProtocol == 2)
                handler = new TickerChannelMessageHandler_v2(chanId, m["pair"].toString());
        }
        else if (channelName == "trades")
        {

            if (serverProtocol == 1)
                handler = new TradeChannelMessageHandler(chanId, m["pair"].toString());
            else if (serverProtocol == 2)
                handler = new TradeChannelMessageHandler_v2(chanId, m["pair"].toString());
        }

        if (handler)
            channelHandlers[chanId] = handler;
    }
}

void Client::onUnsubscribedEvent(QVariantMap m)
{
    if (m.contains("status") && m["status"] == "OK")
    {
        quint32 chanId = m["chanId"].toUInt();
        if (channelHandlers.contains(chanId))
        {
            delete channelHandlers[chanId];
            channelHandlers.remove(chanId);
        }
    }
}

void Client::onErrorEvent(QVariantMap m)
{
    int code = m["code"].toInt();
    qWarning() << "Error [" << code << "]" << m["msg"].toString();

}

void Client::onPongEvent()
{
    pingLatency = pingLanetcyTimer.elapsed();
    qDebug() << "ping is" << pingLatency << "ms";
}

void Client::onMessage(const QString& msg)
{
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
            {
                event = m["event"].toString();
                if (event == "info")
                {
                    emit infoEvent(m);
                }
                else if (event == "subscribed")
                {
                    emit subscribedEvent(m);
                }
                else if (event == "unsubscribed")
                {
                    emit unsubscribedEvent(m);
                }
                else if (event == "error")
                {
                    emit errorEvent(m);
                }
                else if (event=="pong")
                {
                    emit pongEvent();
                }
                else
                {
                    qWarning() << "unknown event";
                }
            }
            else
            {
                qWarning() << "broken reply";
            }
        }
        else if (v.canConvert(QVariant::List))
        {
            QVariantList list = v.toList();
            emit updateRecieved(list);

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
    if (pingLatency < 0)
    {
        qCritical() << "connection to server is lost";
    }
    pingLatency = -1;
    pingLanetcyTimer.start();
    QString pingMsg = "{\"event\":\"ping\"}";

    wsocket->sendTextMessage(pingMsg);

    for (ChannelMessageHandler* handler: channelHandlers)
    {
        QDateTime checkTime = QDateTime::currentDateTime();
        if (checkTime.secsTo(handler->getLastUpdate()) > 10 )
        {
            qDebug() << "channel " << handler->getChanId() << "seems to be dead";
        }
    }
}

void Client::connectServer()
{
    wsocket->close(QWebSocketProtocol::CloseCodeNormal, "controlled reconnect");
    QUrl url(BITFINEX_WEBSOCKET_API_URL);
    wsocket->open(url);
}

bool TickerChannelMessageHandler::processMessage(const QVariantList& msg)
{
    if (ChannelMessageHandler::processMessage(msg))
        return true;

    if (msg.length() < 11)
        return false;

    int idx=1;
    float bid = msg[idx++].toFloat(); //	Price of last highest bid
    float bid_size = msg[idx++].toFloat(); // Size of the last highest bid
    float ask = msg[idx++].toFloat(); //	Price of last lowest ask
    float ask_size = msg[idx++].toFloat(); //	Size of the last lowest ask
    float daily_change = msg[idx++].toFloat(); //	Amount that the last price has changed since yesterday
    float daily_change_perc = msg[idx++].toFloat(); //	Amount that the price has changed expressed in percentage terms
    float last_price = msg[idx++].toFloat(); //	Price of the last trade.
    float volume = msg[idx++].toFloat(); //	Daily volume
    float high = msg[idx++].toFloat(); //	Daily high
    float low = msg[idx++].toFloat(); //	Daily low

    qDebug() << "pair: " << pair
             << "   last:" << last_price
             << "   high:" << high
             << "   low:" << low;

    return true;
}

bool TradeChannelMessageHandler::processMessage(const QVariantList &msg)
{
    if (ChannelMessageHandler::processMessage(msg))
        return true;

    bool ret = true;
    if (msg[1].canConvert(QVariant::List))
    {
        QVariantList recordsList = msg[1].toList();
        for(const QVariant& rec: recordsList)
        {
            QVariantList lst = rec.toList();
            ret = parseSnapshot(lst);
            if (!ret)
                break;
        }
    }
    else
    {
        ret = parseUpdate(msg);
    }
    return ret;

}

bool TradeChannelMessageHandler::parseUpdate(const QVariantList &msg)
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
        printTrade(id, QDateTime::fromTime_t(timestamp), price, amount);

    return true;
}

bool TradeChannelMessageHandler::parseSnapshot(const QVariantList &lst)
{
    if (lst.size() != 4)
    {
        qWarning() << "something wrong";
        return false;
    }

    int idx = 0;
    quint32 id = lst[idx++].toUInt();
    int timestamp = lst[idx++].toInt();
    float price = lst[idx++].toFloat();
    float amount = lst[idx++].toFloat();

    printTrade(id, QDateTime::fromTime_t(timestamp), price, amount);
    return true;
}

void TradeChannelMessageHandler::printTrade(quint32 id, const QDateTime& timestamp, float price, float amount)
{
    qDebug() << timestamp.toString(Qt::ISODate) << pair << (amount>0?"buy":"sell") << qAbs(amount) << '@' << price << "[" << id << "]";
}

bool ChannelMessageHandler::processMessage(const QVariantList &msg)
{
    lastUpdate = QDateTime::currentDateTime();
    if (msg.length() < 1)
        return false;

    if (msg[1].toString() == "hb")
    {
        // heartbeat
        return true;
    }
    return false;
}

quint32 ChannelMessageHandler::getChanId() const
{
    return chanId;
}

QDateTime ChannelMessageHandler::getLastUpdate() const
{
    return lastUpdate;
}

bool TradeChannelMessageHandler_v2::parseUpdate(const QVariantList &msg)
{
    QString tue = msg[1].toString();
    QVariantList updList = msg[2].toList();

    if (updList.length() < 4)
        return false;

    quint32 id =0;
    QDateTime timestamp = QDateTime::currentDateTime();
    float amount =0;
    float price =0;
    if (tue == "tu")
    {
        int idx=0;
        id = updList[idx++].toUInt();
        timestamp = QDateTime::fromMSecsSinceEpoch(updList[idx++].toULongLong());
        amount = updList[idx++].toFloat();
        price = updList[idx++].toFloat();
    }
    else if (tue == "te")
    {
        int idx=0;
        id = updList[idx++].toUInt();
        timestamp = QDateTime::fromMSecsSinceEpoch(updList[idx++].toULongLong());
        amount = updList[idx++].toFloat();
        price = updList[idx++].toFloat();
    }
    else
    {

    }

    printTrade(id, timestamp, price, amount);
    return true;
}

bool TickerChannelMessageHandler_v2::processMessage(const QVariantList &msg)
{
    if (ChannelMessageHandler::processMessage(msg))
        return true;

    if (msg.length() != 2)
        return false;

    if (!msg[1].canConvert(QVariant::List))
        return false;

    QVariantList lst = msg[1].toList();
    if (lst.length() < 10)
        return false;

    int idx=0;
    float bid = lst[idx++].toFloat(); //	Price of last highest bid
    float bid_size = lst[idx++].toFloat(); // Size of the last highest bid
    float ask = lst[idx++].toFloat(); //	Price of last lowest ask
    float ask_size = lst[idx++].toFloat(); //	Size of the last lowest ask
    float daily_change = lst[idx++].toFloat(); //	Amount that the last price has changed since yesterday
    float daily_change_perc = lst[idx++].toFloat(); //	Amount that the price has changed expressed in percentage terms
    float last_price = lst[idx++].toFloat(); //	Price of the last trade.
    float volume = lst[idx++].toFloat(); //	Daily volume
    float high = lst[idx++].toFloat(); //	Daily high
    float low = lst[idx++].toFloat(); //	Daily low

    qDebug() << "pair: " << pair
             << "   last:" << last_price
             << "   high:" << high
             << "   low:" << low;

    return true;
}
