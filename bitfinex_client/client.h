#ifndef CLIENT_H
#define CLIENT_H

#include "ratesdb.h"

#include <QtCore/QObject>
#include <QtCore/qglobal.h>
#include <QTimer>
#include <QVariantMap>
#include <QMap>
#include <QDateTime>
#include <QElapsedTimer>

#include  <functional>

#define BITFINEX_WEBSOCKET_API_URL "wss://api.bitfinex.com/ws/2"

class QWebSocket;

class ChannelMessageHandler
{
protected:
    QDateTime lastUpdate;
    quint32 chanId;
public:
    explicit ChannelMessageHandler(quint32 chanId):chanId(chanId){}
    virtual bool processMessage(const QVariantList& msg) =0;
    virtual ~ChannelMessageHandler(){}

    QDateTime getLastUpdate() const;
    quint32 getChanId() const;

    virtual QString getName() const =0;
    virtual QString getPair() const =0;
};

class PublicChannelMessageHandler : public ChannelMessageHandler
{
protected:
    QString name;
    QString pair;
public:
    PublicChannelMessageHandler(quint32 chanId, const QString& pair, const QString& name) : ChannelMessageHandler (chanId), name(name), pair(pair){}

    virtual bool processMessage(const QVariantList& msg) override = 0;
    virtual QString getName() const override;
    virtual QString getPair() const override;
};

class TickerChannelMessageHandler : public PublicChannelMessageHandler
{
public:
    TickerChannelMessageHandler(quint32 chanId, const QString& pair)
        :PublicChannelMessageHandler(chanId, pair, "ticker")
    {}
    virtual bool processMessage(const QVariantList& msg) override;
};

class TickerChannelMessageHandler_v2 : public TickerChannelMessageHandler
{
public:
    TickerChannelMessageHandler_v2(quint32 chanId, const QString& pair)
        :TickerChannelMessageHandler(chanId, pair)
    {}
    virtual bool processMessage(const QVariantList& msg) override;
};

class TradeChannelMessageHandler : public PublicChannelMessageHandler
{
public:
    TradeChannelMessageHandler(quint32 chanId, const QString& pair)
        :PublicChannelMessageHandler(chanId, pair, "trades")
    {}
    virtual bool processMessage(const QVariantList& msg) override;

    //typedef void (*NewTradeCallback)(quint32 exch_id, const QDateTime& time, float price, float amount, const QString& pair);
    using NewTradeCallback = std::function<void(quint32, const QDateTime&, float, float, const QString&)>;
    void setTradeCallback(NewTradeCallback callbackFunc);

protected:
    NewTradeCallback newTradeCallback;
    virtual bool parseUpdate(const QVariantList& msg);
    virtual bool parseSnapshot(const QVariantList& msg);
    virtual void printTrade(quint32 id, const QDateTime &timestamp, float price, float amount);
};

class TradeChannelMessageHandler_v2: public TradeChannelMessageHandler
{
public:
    TradeChannelMessageHandler_v2(quint32 chanId, const QString& pair)
        :TradeChannelMessageHandler(chanId, pair)
    {}
protected:
    virtual bool parseSnapshot(const QVariantList& msg);
    virtual bool parseUpdate(const QVariantList& msg) override;
};

class PrivateChannelMessageHandler: public ChannelMessageHandler
{
public:
    PrivateChannelMessageHandler():ChannelMessageHandler(0){}
    virtual bool processMessage(const QVariantList& msg) override;
    virtual QString getName() const override {return QString();}
    virtual QString getPair() const override {return QString();}
protected:
    void parseWallet(const QVariantList& wallet);
    void parseOrder(const QVariantList& order);
    void parseBalance(const QVariantList& balance);
    void parseWalletsSnapshot(const QVariantList &v);
    void parseOrdersSnapshot(const QVariantList &v);
    void parseHistoryOrdersSnapshot(const QVariantList& v);
};

class Client : public QObject
{
    Q_OBJECT
    bool loggedIn;
    qint64 pingLatency;
    QElapsedTimer pingLanetcyTimer;
    QTimer heartbeatTimer;
    QWebSocket* wsocket;
    QMap<quint32, ChannelMessageHandler*> channelHandlers;
    int serverProtocol;
    RatesDB rates;

public:
    explicit Client(QObject *parent = 0);

signals:
    void reconnectRequired();
    void resubscribeAllRerquired();

    void subscribedEvent(QVariantMap);
    void unsubscribedEvent(QVariantMap);
    void infoEvent(QVariantMap);
    void errorEvent(QVariantMap);
    void authEvent(QVariantMap);
    void pongEvent();
    void updateRecieved(QVariantList);

public slots:
    void connectServer();
    void subscribeAll();
    void unsubscribeAll();
    void resubscribeAll();

private slots:
    void onConnectWSocket();
    void onMessage(const QString& msg);
    void onTimer();
    void onInfoEvent(QVariantMap m);
    void onSubscribedEvent(QVariantMap m);
    void onUnsubscribedEvent(QVariantMap m);
    void onErrorEvent(QVariantMap m);
    void onAuthEvent(QVariantMap m);
    void onPongEvent();
    void subscribeChannel(const QString& name, const QString &pair);
    void unsubscribeChannel(quint32 chanId);
    void authenticate();
};

#endif // CLIENT_H
