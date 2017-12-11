#ifndef PUSHERCLIENT_H
#define PUSHERCLIENT_H

#include "ratesdb.h"

#include <QObject>
#include <QAbstractSocket>
#include <QSslError>
#include <QTimer>

class QWebSocket;

class PusherClient : public QObject
{
    Q_OBJECT
public:
    explicit PusherClient(QObject *parent = nullptr);

signals:
    void connected();

public slots:
    void connectServer();

private:
    QWebSocket* pSocket;
    RatesDB db;
    QTimer messagesTimeout;
    QTimer connectionTimeout;
    QTimer pingTimeout;
    QTimer reconnectTimeout;

    void subscribeChannel(const QString& channel);

private slots:
    void onWSocketConnect();
    void onWSocketDisconnect();
    void onWSocketTimeout();
    void onWSocketConnectionTimeout();
    void onWSocketPingTimeout();
    void onWSocketPong();
    void onMessage(const QString& msg);
    void onError(QAbstractSocket::SocketError);
    void onSslErrors(const QList<QSslError> &errors);

    void onConnected();
};

#endif // PUSHERCLIENT_H
