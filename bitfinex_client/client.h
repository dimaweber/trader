#ifndef CLIENT_H
#define CLIENT_H

#include <QtCore/QObject>
#include <QtCore/qglobal.h>
#include <QTimer>
#include <QMap>

class QWebSocket;

class ChannelMessageHandler
{
protected:
    quint32 chanId;
public:
    ChannelMessageHandler(quint32 chanId) : chanId(chanId){}
    virtual bool processMessage(const QVariantList& msg) =0;
};

class TickerChannelMessageHandler : public ChannelMessageHandler
{
    QString pair;
public:
    TickerChannelMessageHandler(quint32 chanId, const QString& pair)
        :ChannelMessageHandler(chanId), pair(pair)
    {}
    virtual bool processMessage(const QVariantList& msg) override;
};

class TradeChannelMessageHandler : public ChannelMessageHandler
{
    QString pair;
public:
    TradeChannelMessageHandler(quint32 chanId, const QString& pair)
        :ChannelMessageHandler(chanId), pair(pair)
    {}
    virtual bool processMessage(const QVariantList& msg) override;
};

class Client : public QObject
{
    Q_OBJECT
    QTimer pingTimer;
    QWebSocket* wsocket;
    QMap<quint32, ChannelMessageHandler*> channelHandlers;

public:
    explicit Client(QObject *parent = 0);

    void subscribeTickerChannel(const QString &pair);
    void subscribeTradesChannel(const QString &pair);
signals:

public slots:
    void onConnectWSocket();
    void onMessage(const QString& msg);
    void onTimer();
};

#endif // CLIENT_H
