#include "pusherclient.h"
#include <QWebSocket>
#include <QDebug>
#include <QJsonParseError>

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
    QUrl url("ws://ws.pusherapp.com:80/app/c354d4d129ee0faa5c92?client=linux-weberLib&version=1.0&protocol=7");
    pSocket->open(url);
}

void PusherClient::subscribeChannel(const QString &channel)
{
    QVariantMap map;
    map["event"] ="pusher:subscribe";
    QVariantMap data;
    data["channel"]  = channel;
    map["data"] = data;

    qDebug() << QJsonDocument::fromVariant(map).toJson();
    pSocket->sendTextMessage(QJsonDocument::fromVariant(map).toJson());
}

void PusherClient::onConnectWSocket()
{
    qDebug() << "web socket connected";
}

void PusherClient::onMessage(const QString &msg)
{
    qDebug() << msg;
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qWarning() << "fail to parse json from server: " << error.errorString();
        return;

    }

    QVariant v = doc.toVariant();
    if (!v.canConvert(QVariant::Map))
    {
       qWarning() << "broken json reply" << v.toString();
       return;
    }

    QVariantMap m = v.toMap();
    if (!m.contains("event"))
    {
        qWarning() << "broken reply";
        return;
    }

    QString event = m["event"].toString();
    qDebug() << "event: " << event;

    QString strData = m["data"].toString();
    QJsonDocument docData = QJsonDocument::fromJson(strData.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qWarning() << "fail to parse data json: " << error.errorString();
        return;
    }
    QVariant vData = docData.toVariant();

    if (event == "pusher:connection_established")
    {
        if (!vData.canConvert(QVariant::Map))
        {
            qWarning() << "broken data json " << vData.toString();
            return;
        }

        QVariantMap data = vData.toMap();

        QString socket_id = data["socket_id"].toString();
        int activity_timeout  = data["activity_timeout"].toInt();
        qDebug() << "socket id: " << socket_id;
        emit connected();
    }
    else if (event == "pusher:error")
    {
        if (!vData.canConvert(QVariant::Map))
        {
            qWarning() << "broken data json " << vData.toString();
            return;
        }

        QVariantMap data = vData.toMap();

        QString msg = data["message"].toString();
        int code = data["code"].toInt();
        qWarning() << "error: " <<msg;
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
        for (const QVariant& v: data)
        {
            QVariantList trade = v.toList();
            qDebug() << pair
                     << "    type=" << trade[0].toString()
                     << "    rate=" << trade[1].toDouble()
                     << "  amount=" << trade[2].toDouble();
        }
    }
}

void PusherClient::onError(QAbstractSocket::SocketError error)
{
    qWarning() << error;
}

void PusherClient::onSslErrors(const QList<QSslError> &errors)
{
    for (auto& error: errors)
        qWarning() << error.errorString();
}

void PusherClient::onConnected()
{
    subscribeChannel("btc_usd.trades");
}
