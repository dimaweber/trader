#ifndef STATUSSERVER_H
#define STATUSSERVER_H

#include <QAbstractSocket>

#include <QtCore/QObject>
#include <QtCore/qglobal.h>

class QTcpServer;

class StatusServer : public QObject
{
    Q_OBJECT
public:
    enum State { Starting, Idle, Running, Db_Issue, Http_Issue, Unknown_Issue, Btc_Issue, Done};
    Q_ENUM(StatusServer::State)

    explicit StatusServer(int port = 5010, QObject *parent = 0);

signals:

public slots:
    void onStatusChange(StatusServer::State);
    void start();
protected slots:
    void onNewStatusConnection();
    void onStatusServerError();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError);
private:
    QTcpServer* statusServer;
    State state;
    int port;
};

Q_DECLARE_METATYPE(StatusServer::State)

#endif // STATUSSERVER_H
