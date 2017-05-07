#ifndef STATUSSERVER_H
#define STATUSSERVER_H

#include <QtCore/QObject>
#include <QtCore/qglobal.h>
#include <QTcpServer>

class StatusServer : public QObject
{
    Q_OBJECT
public:
    enum State { Starting, Idle, Running, Db_Issue, Http_Issue, Unknown_Issue, Btc_Issue, Done};
    Q_ENUM(State)

    explicit StatusServer(int port = 5010, QObject *parent = 0);

signals:

public slots:
    void onStatusChange(StatusServer::State state);
    void start();
protected slots:
    void onNewStatusConnection();
    void onStatusServerError();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError);
private:
    QTcpServer statusServer;
    State state;
    int port;
};

#endif // STATUSSERVER_H
