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

    explicit StatusServer(int port = 5010, QObject *parent = 0);

signals:

public slots:
    void onStatusChange(State state);
protected slots:
    void onNewStatusConnection();
    void onStatusServerError();
private:
    QTcpServer statusServer;
    State state;
};

#endif // STATUSSERVER_H