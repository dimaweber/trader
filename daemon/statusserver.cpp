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
    std::unique_ptr<QTcpSocket> socket = std::make_unique<QTcpSocket>(statusServer.nextPendingConnection());
    socket->write("OK\n");
    socket->flush();
    //if (socket->canReadLine())
    {
        //QByteArray msg = socket->readLine();
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
    }
    socket->flush();
}

void StatusServer::onStatusServerError()
{
    std::cerr << statusServer.errorString() << std::endl;
}
