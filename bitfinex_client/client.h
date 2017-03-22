#ifndef CLIENT_H
#define CLIENT_H

#include <QtCore/QObject>
#include <QtCore/qglobal.h>
#include <QTimer>
#include <QVariantMap>
#include <QMap>
#include <QDateTime>
#include <QElapsedTimer>

#define BITFINEX_WEBSOCKET_API_URL "wss://api.bitfinex.com/ws"

class QWebSocket;

class ChannelMessageHandler
{
protected:
    QDateTime lastUpdate;
    quint32 chanId;
public:
    ChannelMessageHandler(quint32 chanId) : chanId(chanId){}
    virtual bool processMessage(const QVariantList& msg) =0;
    virtual ~ChannelMessageHandler(){}
    QDateTime getLastUpdate() const;
    quint32 getChanId() const;
};

class TickerChannelMessageHandler : public ChannelMessageHandler
{
protected:
    QString pair;
public:
    TickerChannelMessageHandler(quint32 chanId, const QString& pair)
        :ChannelMessageHandler(chanId), pair(pair)
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

class TradeChannelMessageHandler : public ChannelMessageHandler
{
public:
    TradeChannelMessageHandler(quint32 chanId, const QString& pair)
        :ChannelMessageHandler(chanId), pair(pair)
    {}
    virtual bool processMessage(const QVariantList& msg) override;
protected:
    QString pair;
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
    virtual bool parseUpdate(const QVariantList& msg) override;
};

class Client : public QObject
{
    Q_OBJECT
    qint64 pingLatency;
    QElapsedTimer pingLanetcyTimer;
    QTimer pingTimer;
    QWebSocket* wsocket;
    QMap<quint32, ChannelMessageHandler*> channelHandlers;
    int serverProtocol;
public:
    explicit Client(QObject *parent = 0);

    void subscribeTickerChannel(const QString &pair);
    void subscribeTradesChannel(const QString &pair);

signals:
    void reconnectRequired();
    void resubscribeAllRerquired();

    void subscribedEvent(QVariantMap);
    void unsubscribedEvent(QVariantMap);
    void infoEvent(QVariantMap);
    void errorEvent(QVariantMap);
    void pongEvent();

    void updateRecieved(QVariantList);

public slots:
    void onConnectWSocket();
    void onMessage(const QString& msg);
    void onTimer();
    void connectServer();

    void unsubscribeChannel(quint32 chanId);

    void subscribeAll();
    void unsubscribeAll();
    void resubscribeAll();

private slots:
    void onInfoEvent(QVariantMap m);
    void onSubscribedEvent(QVariantMap m);
    void onUnsubscribedEvent(QVariantMap m);
    void onErrorEvent(QVariantMap m);
    void onPongEvent();
};

#endif // CLIENT_H
