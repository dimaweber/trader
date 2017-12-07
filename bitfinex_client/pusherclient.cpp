#include "pusherclient.h"
#include <QWebSocket>
#include <QJsonParseError>

#include <iostream>
#include <iomanip>

PusherClient::PusherClient(QObject *parent)
    : QObject(parent)
{
    pSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect (pSocket, &QWebSocket::connected, this, &PusherClient::onConnectWSocket);
    connect (pSocket, &QWebSocket::textMessageReceived, this, &PusherClient::onMessage);
    connect (pSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onError(QAbstractSocket::SocketError)));
    connect (pSocket, &QWebSocket::sslErrors, this, &PusherClient::onSslErrors);

    connect (this, &PusherClient::connected, this, &PusherClient::onConnected);

    connectServer();
}

void PusherClient::connectServer()
{
    //QUrl url("ws://ws.pusherapp.com:80/app/ee987526a24ba107824c?client=linux-weberLib&version=1.0&protocol=7");
    QUrl url("ws://ws-eu.pusher.com:80/app/ee987526a24ba107824c?client=linux-weberLib&version=1.0&protocol=7");
    pSocket->open(url);
}

void PusherClient::subscribeChannel(const QString &channel)
{
    QVariantMap map;
    map["event"] ="pusher:subscribe";
    QVariantMap data;
    data["channel"]  = channel;
    map["data"] = data;

    std::cout << qPrintable(QJsonDocument::fromVariant(map).toJson()) << std::endl;
    pSocket->sendTextMessage(QJsonDocument::fromVariant(map).toJson());
}

void PusherClient::onConnectWSocket()
{
    std::cout << "pusher web socket connected" << std::endl;
}

void PusherClient::onMessage(const QString &msg)
{
//    std::cout << qPrintable(msg) << std::endl;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        std::cerr << "fail to parse json from server: " << qPrintable(error.errorString()) << std::endl;
        return;

    }

    QVariant v = doc.toVariant();
    if (!v.canConvert(QVariant::Map))
    {
       std::cerr << "broken json reply" << qPrintable(v.toString()) << std::endl;
       return;
    }

    QVariantMap m = v.toMap();
    if (!m.contains("event"))
    {
        std::cerr << "broken reply" << qPrintable(v.toString()) << std::endl;
        return;
    }

    QString event = m["event"].toString();
//    std::cout << "event: " << qPrintable(event) << std::endl;

    QString strData = m["data"].toString();
    QJsonDocument docData = QJsonDocument::fromJson(strData.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        std::cerr << "fail to parse data json: " << qPrintable(error.errorString()) << std::endl;
        return;
    }
    QVariant vData = docData.toVariant();

    if (event == "pusher:connection_established")
    {
        if (!vData.canConvert(QVariant::Map))
        {
            std::cerr << "broken data json " << qPrintable(vData.toString()) << std::endl;
            return;
        }

        QVariantMap data = vData.toMap();

        QString socket_id = data["socket_id"].toString();
        int activity_timeout  = data["activity_timeout"].toInt();
        std::cout << "socket id: " << qPrintable(socket_id) << std::endl;
        emit connected();
    }
    else if (event == "pusher:error")
    {
        if (!vData.canConvert(QVariant::Map))
        {
            std::cerr << "broken data json " << qPrintable(vData.toString());
            return;
        }

        QVariantMap data = vData.toMap();

        QString msg = data["message"].toString();
        int code = data["code"].toInt();
        std::cerr << "error: " << qPrintable(msg) << std::endl;
    }
    else if (event == "pusher_internal:subscription_succeeded")
    {

    }

    if (m.contains("channel"))
    {
        QString channel = m["channel"].toString();
        QString event = m["event"].toString();
        QVariantList data = vData.toList();
        QString pair = channel.left(7);
        if (event == "trades")
        {
            for (const QVariant& v: data)
            {
                QVariantList trade = v.toList();
                QString type = trade[0].toString();
                double rate = trade[1].toDouble();
                double amount = trade[2].toDouble();
                int id = QDateTime::currentMSecsSinceEpoch();
                db.newRate("btc-e", id, pair, QDateTime::currentDateTimeUtc(), rate, amount, type);
                std::cout << qPrintable(QDateTime::currentDateTime().toString(Qt::ISODate))
                          << " BTC-E [" << id << "] "
                          << qPrintable(pair) << " "
                          << std::setw(5) << qPrintable(type) << " "
                          << amount
                          << " @ "
                          << rate
                          << std::endl;
            }
        }
    }
}

void PusherClient::onError(QAbstractSocket::SocketError error)
{
    std::cerr << error << std::endl;
}

void PusherClient::onSslErrors(const QList<QSslError> &errors)
{
    for (auto& error: errors)
        std::cerr << qPrintable(error.errorString()) << std::endl;
}

void PusherClient::onConnected()
{
    subscribeChannel("btc_usd.trades");
    subscribeChannel("eth_usd.trades");
}
