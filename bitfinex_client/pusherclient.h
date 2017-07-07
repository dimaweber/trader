#ifndef PUSHERCLIENT_H
#define PUSHERCLIENT_H

#include <QObject>
#include <QAbstractSocket>
#include <QSslError>

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

    void subscribeChannel(const QString& channel);

private slots:
    void onConnectWSocket();
    void onMessage(const QString& msg);
    void onError(QAbstractSocket::SocketError);
    void onSslErrors(const QList<QSslError> &errors);

    void onConnected();
};

#endif // PUSHERCLIENT_H
