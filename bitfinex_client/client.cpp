#include "client.h"
#include "utils.h"

#include <QDebug>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QByteArray>

#define API_KEY "qIx1exnyQPFSxkGpaXEdOORMqYSrDW4ONo9ia70atwn"
#define API_SECRET "yb54vszGFfi876fgBpJ0fikGLeJYe17Yx4oPc1Z051c"

Client::Client(QObject *parent) : QObject(parent)
  ,loggedIn(false), serverProtocol(-1)
{
    connect (&heartbeatTimer, &QTimer::timeout, this, &Client::onTimer);
    heartbeatTimer.setSingleShot(false);

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
    connect (this, &Client::authEvent, this, &Client::onAuthEvent);

    emit reconnectRequired();
}

void Client::subscribeChannel(const QString& name, const QString& pair)
{
    QString msg = QString("{'event':'subscribe', 'channel':'%1', 'pair':'%2'}").arg(name).arg(pair).replace("'", "\"");
    wsocket->sendTextMessage(msg);
}

void Client::unsubscribeChannel(quint32 chanId)
{
    QString msg = QString("{'event':'unsubscribe', 'chanId':'%1'}").arg(chanId).replace("'", "\"");
    wsocket->sendTextMessage(msg);
}

void Client::authenticate()
{
    QString nonce = QString::number(QDateTime::currentDateTime().toTime_t());
    QVariantMap v;
    v["event"] = "auth";
    v["apiKey"] = API_KEY;
    v["authNonce"] = nonce;
    v["authPayload"] = QString("AUTH%1").arg(nonce);
    v["authSig"] = hmac_sha384(v["authPayload"].toByteArray(), QByteArray(API_SECRET)).toHex();
    v["filter"] = QStringList({"trading", "wallet", "balance"});

    QString msg = QString::fromUtf8(QJsonDocument::fromVariant(v).toJson());
    wsocket->sendTextMessage(msg);
}

void Client::subscribeAll()
{
    subscribeChannel("ticker", "BTCUSD");
    subscribeChannel("ticker", "LTCUSD");
    subscribeChannel("trades", "BTCUSD");
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
            authenticate();
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
        PublicChannelMessageHandler* handler = nullptr;
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

void Client::onAuthEvent(QVariantMap m)
{
    if (m["status"] == "OK")
    {
        loggedIn = true;
        int userId = m["userId"].toInt();
        qDebug() << "logged in as user" << userId;
        channelHandlers[0] = new PrivateChannelMessageHandler();
    }
    else
    {
        int code = m["code"].toInt();
        qWarning() << "authentication failed with code" << code;
    }

}

void Client::onPongEvent()
{
    pingLatency = pingLanetcyTimer.elapsed();
//    qDebug() << "ping is" << pingLatency << "ms";
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
                if (event == "auth")
                {
                    emit authEvent(m);
                }
                else if (event == "info")
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
        heartbeatTimer.stop();
        emit reconnectRequired();
    }
    for (ChannelMessageHandler* handler: channelHandlers)
    {
        QDateTime checkTime = QDateTime::currentDateTime();
        int noMsgSec = checkTime.secsTo(handler->getLastUpdate());
        if ( noMsgSec > 10 )
        {
            qDebug() << "channel " << handler->getChanId() << "seems to be dead";
        }
        if (noMsgSec > 30 )
        {
            qDebug() << "Channel [" << handler->getChanId() << "]" << handler->getName() << ": no messages for too long, resubscribe";
            unsubscribeChannel(handler->getChanId());
            subscribeChannel(handler->getName(), handler->getPair());
        }
    }

    pingLatency = -1;
    pingLanetcyTimer.start();
    QString pingMsg = "{\"event\":\"ping\"}";

    wsocket->sendTextMessage(pingMsg);
}

void Client::connectServer()
{
    for (ChannelMessageHandler* handler: channelHandlers)
    {
        delete handler;
    }
    channelHandlers.clear();
    wsocket->close(QWebSocketProtocol::CloseCodeNormal, "controlled reconnect");
    QUrl url(BITFINEX_WEBSOCKET_API_URL);
    wsocket->open(url);
    pingLatency = 1;
    heartbeatTimer.start(10 * 1000);
}

bool TickerChannelMessageHandler::processMessage(const QVariantList& msg)
{
    if (PublicChannelMessageHandler::processMessage(msg))
        return true;

    if (msg.length() < 11)
        return false;

    int idx=1;
    /*float bid =*/ msg[idx++].toFloat(); //	Price of last highest bid
    /*float bid_size =*/ msg[idx++].toFloat(); // Size of the last highest bid
    /*float ask =*/ msg[idx++].toFloat(); //	Price of last lowest ask
    /*float ask_size =*/ msg[idx++].toFloat(); //	Size of the last lowest ask
    /*float daily_change =*/ msg[idx++].toFloat(); //	Amount that the last price has changed since yesterday
    /*float daily_change_perc =*/ msg[idx++].toFloat(); //	Amount that the price has changed expressed in percentage terms
    /*float last_price =*/ msg[idx++].toFloat(); //	Price of the last trade.
    /*float volume =*/ msg[idx++].toFloat(); //	Daily volume
    /*float high =*/ msg[idx++].toFloat(); //	Daily high
    /*float low =*/ msg[idx++].toFloat(); //	Daily low

//    qDebug() << "pair: " << pair << "   last:" << last_price << "   high:" << high << "   low:" << low;

    return true;
}

bool TradeChannelMessageHandler::processMessage(const QVariantList &msg)
{
    if (PublicChannelMessageHandler::processMessage(msg))
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
    QString type = msg[idx++].toString();
    QString seq = msg[idx++].toString();
    quint32 id = 0;
    if (type=="tu")
    {
        id = msg[idx++].toUInt();
    }
    int timestamp = msg[idx++].toInt();
    float price = msg[idx++].toFloat();
    float amount = msg[idx++].toFloat();

    if (type == "tu")
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
    qDebug() << timestamp.toString(Qt::ISODate)
             << "[" << id << "]"
             << pair
             << (amount>0?"buy":"sell")
             << qAbs(amount) << '@' << price;
}

bool PublicChannelMessageHandler::processMessage(const QVariantList &msg)
{
    if (ChannelMessageHandler::processMessage(msg))
        return true;

    if (msg.length() < 1)
        return false;

    return false;
}

quint32 ChannelMessageHandler::getChanId() const
{
    return chanId;
}

QString PublicChannelMessageHandler::getName() const
{
    return name;
}

QString PublicChannelMessageHandler::getPair() const
{
    return pair;
}

bool ChannelMessageHandler::processMessage(const QVariantList &msg)
{
    lastUpdate = QDateTime::currentDateTime();
    if (msg[1].toString() == "hb")
    {
        // heartbeat
        return true;
    }
    return false;
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
    if (PublicChannelMessageHandler::processMessage(msg))
        return true;

    if (msg.length() != 2)
        return false;

    if (!msg[1].canConvert(QVariant::List))
        return false;

    QVariantList lst = msg[1].toList();
    if (lst.length() < 10)
        return false;

    int idx=0;
    /*float bid =*/ lst[idx++].toFloat(); //	Price of last highest bid
    /*float bid_size =*/ lst[idx++].toFloat(); // Size of the last highest bid
    /*float ask =*/ lst[idx++].toFloat(); //	Price of last lowest ask
    /*float ask_size =*/ lst[idx++].toFloat(); //	Size of the last lowest ask
    /*float daily_change =*/ lst[idx++].toFloat(); //	Amount that the last price has changed since yesterday
    /*float daily_change_perc =*/ lst[idx++].toFloat(); //	Amount that the price has changed expressed in percentage terms
    float last_price = lst[idx++].toFloat(); //	Price of the last trade.
    /*float volume =*/ lst[idx++].toFloat(); //	Daily volume
    float high = lst[idx++].toFloat(); //	Daily high
    float low = lst[idx++].toFloat(); //	Daily low

    qDebug() << "pair: " << pair
             << "   last:" << last_price
             << "   high:" << high
             << "   low:" << low;

    return true;
}

bool PrivateChannelMessageHandler::processMessage(const QVariantList &msg)
{
    if (ChannelMessageHandler::processMessage(msg))
        return true;

    QString type = msg[1].toString();
    QVariantList data = msg[2].toList();
    if (type == "ws")
        parseWalletsSnapshot(data);
    else if (type == "wu")
        parseWallet(data);
    else if (type == "os")
        parseOrdersSnapshot(data);
    else if (type == "ou")
        parseOrder(data);
    else if (type == "hos")
        parseHistoryOrdersSnapshot(data);
    else if (type == "bs" || type == "bu")
        parseBalance(data);
    else
        qDebug() << type << data;

    return true;
}

void PrivateChannelMessageHandler::parseWallet(const QVariantList &wallet)
{
    int idx=0;
    QString walletType = wallet[idx++].toString();
    QString currency = wallet[idx++].toString();
    float balance = wallet[idx++].toFloat();
    float unsettledInterest = wallet[idx++].toFloat();
    float balanceAvailable = wallet[idx++].toFloat();

    qDebug() << walletType << currency << balance << unsettledInterest << balanceAvailable;
}

void PrivateChannelMessageHandler::parseOrder(const QVariantList &order)
{
    int idx=0;
    quint32 id = order[idx++].toInt();
    quint32 gid = order[idx++].toInt();
    quint32 cid = order[idx++].toInt();
    QString pair = order[idx++].toString();
    quint32 timestamp_create = order[idx++].toUInt();
    quint32 timestamp_update = order[idx++].toUInt();
    float amount = order[idx++].toFloat();
    float amount_orig = order[idx++].toFloat();
    QString type = order[idx++].toString();
    QString type_prev = order[idx++].toString();
    idx++;
    idx++;
    /*quint32 flags =*/ order[idx++].toUInt();
    QString status = order[idx++].toString();
    idx++;
    idx++;
    float rate = order[idx++].toFloat();
    float rate_avg = order[idx++].toFloat();
    /*float rate_trailing =*/ order[idx++].toFloat();
    /*float rate_aux_Limit =*/ order[idx++].toFloat();
    idx++;
    idx++;
    idx++;
    /*bool notify =*/ order[idx++].toBool();
    /*bool hidden =*/ order[idx++].toBool();
    /*quint32 place_id =*/ order[idx++].toUInt();

    qDebug() << id << gid << cid << pair << timestamp_create << timestamp_update << amount << amount_orig << type << type_prev << status << rate << rate_avg;
}

void PrivateChannelMessageHandler::parseBalance(const QVariantList &balance)
{
    int idx = 0;
    float aus = balance[idx++].toFloat(); // Total Assets Under Management
    float aus_net = balance[idx++].toFloat(); //Net Assets Under Management
    //QString wallet_type = balance[idx++].toString(); //Exchange, Trading, or Deposit
    //QString currency = balance[idx++].toString(); //"BTC", "USD", etc

    qDebug() << aus << aus_net; // << wallet_type << currency;
}

void PrivateChannelMessageHandler::parseWalletsSnapshot(const QVariantList& v)
{
    for (const QVariant& w: v )
    {
        parseWallet(w.toList());
    }
}

void PrivateChannelMessageHandler::parseOrdersSnapshot(const QVariantList& v)
{
    for (const QVariant& o: v )
    {
        parseOrder(o.toList());
    }
}

void PrivateChannelMessageHandler::parseHistoryOrdersSnapshot(const QVariantList &v)
{
    for (const QVariant& o: v )
    {
        parseOrder(o.toList());
    }
}
