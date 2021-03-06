#include "statusserver.h"
#include <utils.h>

#include <QTcpServer>
#include <QTcpSocket>

#include <iostream>
#include <memory>

StatusServer::StatusServer(int port, QObject *parent)
    : QObject(parent), state(Starting), port(port)
{
}

void StatusServer::onStatusChange(StatusServer::State state)
{
    this->state = state;
}

void StatusServer::start()
{
    statusServer = new QTcpServer(this);
    connect (statusServer, &QTcpServer::newConnection, this, &StatusServer::onNewStatusConnection);
    connect (statusServer, &QTcpServer::acceptError, this, &StatusServer::onStatusServerError);
    statusServer->listen(QHostAddress::AnyIPv4, port);
}

void StatusServer::onNewStatusConnection()
{
    QTcpServer* server = qobject_cast<QTcpServer*>(sender());
    if (!server)
        return;

    QTcpSocket* socket = server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &StatusServer::onSocketReadyRead);
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onSocketError(QAbstractSocket::SocketError)));
}

void StatusServer::onStatusServerError()
{
    QTcpServer* server = qobject_cast<QTcpServer*>(sender());
    if (!server)
        return;

    std::cerr << server->errorString() << std::endl;
}

void StatusServer::onSocketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    if (!socket->canReadLine())
        return;

    QString msg = QString::fromUtf8(socket->readLine()).trimmed();
    if (msg == "get_status")
    {
        switch (state)
        {
            case Starting: socket->write("STARTING\n"); break;
            case Idle: socket->write("IDLE\n"); break;
            case Running: socket->write("RUNNING\n"); break;
            case Db_Issue: socket->write("DB_ISSUE\n"); break;
            case Http_Issue: socket->write("HTTP_ISSUE\n"); break;
            case Unknown_Issue: socket->write("UNKNOWN_ISSUE\n"); break;
            case Btc_Issue: socket->write("BTC_ISSUE\n"); break;
            case Done: socket->write("DONE"); break;
        }
        socket->flush();
    }
    else if (msg=="exit")
    {
        socket->close();
        socket->deleteLater();
    }
    socket->flush();
}

void StatusServer::onSocketError(QAbstractSocket::SocketError)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    std::cerr << socket->errorString() << std::endl;
}
