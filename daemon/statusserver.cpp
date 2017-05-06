#include "statusserver.h"
#include <utils.h>

#include <QTcpSocket>

#include <iostream>
#include <memory>

StatusServer::StatusServer(int port, QObject *parent)
    : QObject(parent), state(Starting)
{
    connect (&statusServer, &QTcpServer::newConnection, this, &StatusServer::onNewStatusConnection);
    connect (&statusServer, &QTcpServer::acceptError, this, &StatusServer::onStatusServerError);
    statusServer.listen(QHostAddress::AnyIPv4, port);
}

void StatusServer::onStatusChange(State state)
{
    this->state = state;
}

void StatusServer::onNewStatusConnection()
{
    QTcpSocket* socket = statusServer.nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &StatusServer::onSocketReadyRead);
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onSocketError(QAbstractSocket::SocketError)));
}

void StatusServer::onStatusServerError()
{
    std::cerr << statusServer.errorString() << std::endl;
}

void StatusServer::onSocketReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    if (!socket->canReadLine())
        return;

    QByteArray msg = socket->readLine();
    if (msg == "get_status")
    {
        switch (state)
        {
            case Starting: socket->write("STARTING"); break;
            case Idle: socket->write("IDLE"); break;
            case Running: socket->write("RUNNING"); break;
            case Db_Issue: socket->write("DB_ISSUE"); break;
            case Http_Issue: socket->write("HTTP_ISSUE"); break;
            case Unknown_Issue: socket->write("UNKNOWN_ISSUE"); break;
            case Btc_Issue: socket->write("BTC_ISSUE"); break;
            case Done: socket->write("DONE"); break;
        }
        socket->flush();
    }
    else if (msg=="exit")
    {
        socket->close();
        socket->deleteLater();
    }
}

void StatusServer::onSocketError(QAbstractSocket::SocketError)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    std::cerr << socket->errorString() << std::endl;
}
