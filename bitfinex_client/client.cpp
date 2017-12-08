#include "client.h"
#include "utils.h"
#include "btce.h"

#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QByteArray>

#include <iostream>
#include <iomanip>

QString bitfinex_pair_to_btce_pair(const QString& str)
{
    int pos=0;
    if (str.length()>6)
        pos++;
    return QString("%1_%2").arg(str.mid(pos,3)).arg(str.mid(pos+3,3)).toLower();
}

std::ostream& operator<<(std::ostream& stream, const QString& str)
{
    stream << qPrintable(str);
    return stream;
}
std::ostream& operator<<(std::ostream& stream, const QVariant& lst)
{
    stream << qPrintable(lst.toString());
    return stream;
}

Client::Client(QObject *parent) : QObject(parent)
  ,loggedIn(false), serverProtocol(-1), serverMaintenanceMode(false)
{
    connect (&heartbeatTimer, &QTimer::timeout, this, &Client::onTimer);
    heartbeatTimer.setSingleShot(false);

    connect(&socketPingTimeoutTimer, &QTimer::timeout, this, &Client::onWSocketPingTimeout);
    socketPingTimeoutTimer.setSingleShot(true);

    connect (&connectTimeoutTimer, &QTimer::timeout, this, &Client::onWSocketConnectTimeout);
    connectTimeoutTimer.setSingleShot(true);

//    connect (&pingTimeoutTimer, &QTimer::timeout, this, &Client::onPingTimeout);
//    pingTimeoutTimer.setSingleShot(true);

    wsocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect (wsocket, &QWebSocket::connected, this, &Client::onWSocketConnect);
    connect (wsocket, &QWebSocket::disconnected, this, &Client::onWSocketDisconnect);
    connect (wsocket, &QWebSocket::textMessageReceived, this, &Client::onMessage);
    connect (wsocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onWSocketError(QAbstractSocket::SocketError)));
    connect (wsocket, &QWebSocket::pong, this, &Client::onWSocketPong);
    connect (wsocket, &QWebSocket::stateChanged, this, &Client::onWSocketStateChanged);

    connect (this, &Client::reconnectRequired, this, &Client::connectServer);
//    connect (this, &Client::pongEvent, this, &Client::onPongEvent);
    connect (this, &Client::infoEvent, this, &Client::onInfoEvent);
    connect (this, &Client::errorEvent, this, &Client::onErrorEvent);
    connect (this, &Client::subscribedEvent, this, &Client::onSubscribedEvent);
    connect (this, &Client::unsubscribedEvent, this, &Client::onUnsubscribedEvent);
    connect (this, &Client::resubscribeAllRerquired, this, &Client::resubscribeAll);
    connect (this, &Client::authEvent, this, &Client::onAuthEvent);

    emit reconnectRequired();
}

QByteArray Client::getConfigLine(const char* prefix, const QString& filename)
{
    const size_t prefix_len = strlen(prefix);
    QByteArray value;
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly))
    {
        while(!file.atEnd())
        {
            QByteArray line = file.readLine();
            if (line.startsWith(prefix))
            {
                value = line.right(line.length() - prefix_len).trimmed();
                break;
            }
        }
    }
    else
        std::cerr << "fail to open " << filename << " file" << std::endl;
    return value;
}

QByteArray Client::getApiKey()
{
    static QByteArray apikey;
    if (apikey.isEmpty())
    {
        apikey = getConfigLine("apikey=");
    }
    return apikey;
}

QByteArray Client::getSecret()
{
    static QByteArray secret;
    if (secret.isEmpty())
    {
        secret = getConfigLine("secret=");
    }
    return secret;
}

void Client::subscribeChannel(const QString& name, const QString& pair)
{
    QVariantMap m;
    m["event"] = "subscribe";
    m["channel"] = name;
    m["pair"] = pair;

    sendMessage(m);
}

void Client::unsubscribeChannel(quint32 chanId)
{
    QVariantMap m;
    m["event"] = "unsubscribe";
    m["chanId"] = chanId;

    sendMessage(m);
}

void Client::authenticate()
{
    static long n = QDateTime::currentDateTime().toTime_t() - 1499332582;
    QString nonce = QString::number(++n);
    QVariantMap v;
    v["event"] = "auth";
    v["apiKey"] = getApiKey();
    v["authNonce"] = nonce;
    v["authPayload"] = QString("AUTH%1").arg(nonce);
    v["authSig"] = hmac_sha384(v["authPayload"].toByteArray(), getSecret()).toHex();
    v["filter"] = QStringList() << "trading" << "wallet" << "balance";

    sendMessage(v);
}

void Client::subscribeAll()
{
    subscribeChannel("ticker", "BTCUSD");
    subscribeChannel("ticker", "LTCUSD");
    subscribeChannel("ticker", "ETHUSD");
    subscribeChannel("trades", "BTCUSD");
    subscribeChannel("trades", "ETHUSD");
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

bool Client::sendMessage(const QVariantMap &data)
{
    if (serverMaintenanceMode)
        return false;

    QString msg = QString::fromUtf8(QJsonDocument::fromVariant(data).toJson());
    wsocket->sendTextMessage(msg);
    return true;
}

void Client::onWSocketConnect()
{
    std::cout << "web socket connected" << std::endl;
    connectTimeoutTimer.stop();
}

void Client::onWSocketDisconnect()
{
    std::cout << "websocket disconnected: [" << wsocket->closeCode() << "] " << wsocket->closeReason() << std::endl;
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
            std::cerr << "unsupported protocol version " << serverProtocol << std::endl;
        }
    }
    else if (m.contains("code"))
    {
        int code = m["code"].toInt();
        std::cout << "[" << code << "] " << m["msg"].toString() << std::endl;
        switch (code)
        {
            case 20051:
                emit reconnectRequired();
                break;
            case 20060:
                serverMaintenanceMode = true;
                break;
            case 20061:
                serverMaintenanceMode = false;
                emit resubscribeAllRerquired();
        }
    }
}

void Client::onSubscribedEvent(QVariantMap m)
{
    if (!m.contains("channel"))
        std::cout << "no channel name in subscribed reply" << std::endl;
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

            auto dbWriteCallback = [this](quint32 exch_id, const QDateTime& time, float price, float amount, const QString& pair)
            {
                if (!rates.newRate("bitfinex", exch_id, bitfinex_pair_to_btce_pair(pair), time.toUTC(), price, qAbs(amount), amount<0?"sell":"buy"))
                    std::cerr << "fail to write to database" << std::endl;
            };

            TradeChannelMessageHandler* p = nullptr;
            if (serverProtocol == 1)
                p = new TradeChannelMessageHandler(chanId, m["pair"].toString());
            else if (serverProtocol == 2)
                p = new TradeChannelMessageHandler_v2(chanId, m["pair"].toString());
            p->setTradeCallback(dbWriteCallback);
            handler = p;
        }

        if (channelHandlers.contains(chanId))
        {
            std::cerr << "there is already handler for channel " << chanId << std::endl;
            delete channelHandlers[chanId];
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
    std::cerr << "Error [" << code << "]" << m["msg"].toString() << std::endl;

}

void Client::onAuthEvent(QVariantMap m)
{
    if (m["status"] == "OK")
    {
        loggedIn = true;
        int userId = m["userId"].toInt();
        std::cout << "logged in as user " << userId << std::endl;
        channelHandlers[0] = new PrivateChannelMessageHandler();
    }
    else
    {
        int code = m["code"].toInt();
        QString msg = m["msg"].toString();
        std::cerr << "***CRITICAL*** authentication failed with code" << code << " :    " << msg << std::endl;

        authenticate();
    }

}

//void Client::onPongEvent()
//{
//    pingLatency = pingLanetcyTimer.elapsed();
//    std::cout << "ping is " << pingLatency << "ms" << std::endl;
//    pingTimeoutTimer.stop();
//}

//void Client::onPingTimeout()
//{
//    std::cerr << "***CRITICAL*** " << "connection to server is lost"  << std::endl;
//    heartbeatTimer.stop();
//    emit reconnectRequired();
//}

void Client::onMessage(const QString& msg)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        std::cerr << "fail to parse json from server: " << error.errorString() << std::endl;
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
                    std::cerr << "unknown event" << std::endl;
                }
            }
            else
            {
                std::cerr << "broken reply" << std::endl;
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
            std::cerr << "broken json reply" << v.toString() << std::endl;
        }
    }
}

void Client::onWSocketError(QAbstractSocket::SocketError err)
{
    QIODevice* sock = qobject_cast<QIODevice*>(sender());
    if (sock)
        std::cerr << "***CRITICAL*** " << err << sock->errorString() << std::endl;
    else
        std::cerr << "***CRITICAL*** " << err << std::endl;
}

void Client::onWSocketPong(quint64 elapsedTime, const QByteArray& /*payload*/)
{
    std::cout << "socket answer in " << elapsedTime << "ms" << std::endl;
    socketPingTimeoutTimer.stop();
}

void Client::onWSocketPingTimeout()
{
    std::cerr << "socket pong fail" << std::endl;
    emit reconnectRequired();
}

void Client::socketPing()
{
    wsocket->ping("socket ping");
    socketPingTimeoutTimer.start(5 * 1000);
}

//void Client::ping()
//{
//    pingLatency = -1;
//    pingLanetcyTimer.start();
//    QVariantMap m;
//    m["event"] = "ping";

//    sendMessage(m);
//}

void Client::onTimer()
{
    socketPing();
//    ping();

    for (ChannelMessageHandler* handler: channelHandlers)
    {
        QDateTime checkTime = QDateTime::currentDateTime();
        int noMsgSec = checkTime.secsTo(handler->getLastUpdate());
        if ( noMsgSec > 10 )
        {
            std::cerr << "channel " << handler->getChanId() << "seems to be dead" << std::endl;
        }
        if (noMsgSec > 30 )
        {
            std::cerr << "Channel [" << handler->getChanId() << "]" << handler->getName() << ": no messages for too long, resubscribe" << std::endl;
            unsubscribeChannel(handler->getChanId());
            subscribeChannel(handler->getName(), handler->getPair());
        }
    }
}

void Client::onWSocketConnectTimeout()
{
    std::cerr << "connection to  server timed out" << std::endl;
    QAbstractSocket::SocketState state = wsocket->state();
    wsocket->close();
}

void Client::onWSocketStateChanged(QAbstractSocket::SocketState state)
{
    std::cout << "new web socket state: " << state << std::endl;
}

void Client::connectServer()
{
    heartbeatTimer.stop();

    for (ChannelMessageHandler* handler: channelHandlers)
    {
        delete handler;
    }
    channelHandlers.clear();

    if (wsocket->state() == QAbstractSocket::ConnectedState)
        wsocket->close(QWebSocketProtocol::CloseCodeNormal, "controlled reconnect");

    QUrl url(BITFINEX_WEBSOCKET_API_URL);
    wsocket->open(url);
    connectTimeoutTimer.start(2000);
//    pingLatency = 0;
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

//    std::cout << "pair: " << pair << "   last:" << last_price << "   high:" << high << "   low:" << low << std::endl;

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

void TradeChannelMessageHandler::setTradeCallback(TradeChannelMessageHandler::NewTradeCallback callbackFunc)
{
    newTradeCallback = callbackFunc;
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
    {
        QDateTime time = QDateTime::fromTime_t(timestamp);
        if (newTradeCallback == nullptr)
            printTrade(id, time, price, amount);
        else
            newTradeCallback(id, time, price, amount, pair);
    }

    return true;
}

bool TradeChannelMessageHandler::parseSnapshot(const QVariantList &lst)
{
    if (lst.size() != 4)
    {
        std::cerr << "something wrong" << std::endl;
        return false;
    }

    int idx = 0;
    quint32 id = lst[idx++].toUInt();
    int timestamp = lst[idx++].toInt();
    float price = lst[idx++].toFloat();
    float amount = lst[idx++].toFloat();

    QDateTime time = QDateTime::fromTime_t(timestamp);
    if (newTradeCallback == nullptr)
        printTrade(id, time, price, amount);
    else
        newTradeCallback(id, time, price, amount, pair);

    return true;
}

void TradeChannelMessageHandler::printTrade(quint32 id, const QDateTime& timestamp, float price, float amount)
{
    QString type = amount>0?"buy":"sell";
    std::cout << timestamp.toString(Qt::ISODate)
             << " BTFNX [" << id << "] "
             << bitfinex_pair_to_btce_pair(pair) << " "
             << std::setw(5) << type << " "
             << qAbs(amount)
             << " @ "
             << price
             << std::endl;

    QString p = bitfinex_pair_to_btce_pair(pair);
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

bool TradeChannelMessageHandler_v2::parseSnapshot(const QVariantList &lst)
{
    if (lst.size() != 4)
    {
        std::cerr << "something wrong" << std::endl;
        return false;
    }

    int idx = 0;
    quint32 id = lst[idx++].toUInt();
    qulonglong timestamp = lst[idx++].toULongLong();
    float amount = lst[idx++].toFloat();
    float price = lst[idx++].toFloat();

    /*
    QDateTime time = QDateTime::fromMSecsSinceEpoch(timestamp);
    if (newTradeCallback == nullptr)
        printTrade(id, time, price, amount);
    else
        newTradeCallback(id, time, price, amount, pair);
    */
    return true;
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
    int idx=0;
    id = updList[idx++].toUInt();
    timestamp = QDateTime::fromMSecsSinceEpoch(updList[idx++].toULongLong());
    amount = updList[idx++].toFloat();
    price = updList[idx++].toFloat();

    if (tue == "tu")
    {
        printTrade(id, timestamp, price, amount);
        if (newTradeCallback)
            newTradeCallback(id, timestamp, price, amount, pair);
    }

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

    std::cout << "pair: " << pair
             << "   last:" << last_price
             << "   high:" << high
             << "   low:" << low
             << std::endl;

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
    else if (type == "ou" || type == "on" || type == "oc")
        parseOrder(data);
    else if (type == "hos")
        parseHistoryOrdersSnapshot(data);
    else if (type == "bs" || type == "bu")
        parseBalance(data);
    else
        std::cout << type << " " << data << std::endl;

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

    std::cout << walletType << " "
              << currency   << " "
              << balance    << " "
              << unsettledInterest << " "
              << balanceAvailable << " "
              << std::endl;
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

    BtcObjects::Order o;
    o.type = (amount<0) ? BtcObjects::Order::Type::Sell : BtcObjects::Order::Type::Buy;
    o.amount = qAbs(amount);
    o.rate = rate;
    o.pair = bitfinex_pair_to_btce_pair(pair);
    o.timestamp_created = QDateTime::fromTime_t(timestamp_create);
    o.order_id = id;
    o.start_amount = amount_orig;
    if (status == "ACTIVE")
        o.status = BtcObjects::Order::Active;
    else if (status == "EXECUTED")
        o.status = BtcObjects::Order::Done;
    else if (status == "PARTIALLY FILLED")
        o.status = BtcObjects::Order::CanceledPartiallyDone;
    else if (status == "CANCELED")
        o.status = BtcObjects::Order::Canceled;

    o.display();
}

void PrivateChannelMessageHandler::parseBalance(const QVariantList &balance)
{
    int idx = 0;
    float aus = balance[idx++].toFloat(); // Total Assets Under Management
    float aus_net = balance[idx++].toFloat(); //Net Assets Under Management
    //QString wallet_type = balance[idx++].toString(); //, Trading, or Deposit
    //QString currency = balance[idx++].toString(); //"BTC", "USD", etc

    std::cout << aus << " "
              << aus_net << " "
              << std::endl; // << wallet_type << currency;
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
