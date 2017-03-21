#include "client.h"
#include <QDebug>
#include <QWebSocket>

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

void Client::onConnectWSocket()
{
    qDebug() << "web socket connected";
}

void Client::onMessage(const QString& msg)
{
    qDebug() << "[BF ==> cl] " << msg;
}

void Client::onTimer()
{
    QString pingMsg = "{\"event\":\"ping\"}";
    qDebug() << "[BF <== cl] " << pingMsg;
    wsocket->sendTextMessage(pingMsg);
}
