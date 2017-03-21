#include "client.h"

#include <QtCore/QCoreApplication>
#include <QtCore/qglobal.h>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Client client;

    return a.exec();
}
