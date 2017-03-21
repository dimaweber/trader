#ifndef CLIENT_H
#define CLIENT_H

#include <QtCore/QObject>
#include <QtCore/qglobal.h>
#include <QTimer>


class QWebSocket;

class Client : public QObject
{
    Q_OBJECT
    QTimer pingTimer;
    QWebSocket* wsocket;

public:
    explicit Client(QObject *parent = 0);

signals:

public slots:
    void onConnectWSocket();
    void onMessage(const QString& msg);
    void onTimer();
};

#endif // CLIENT_H
