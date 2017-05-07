#include "sql_database.h"
#include "trader.h"
#include "statusserver.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QThread>

#ifndef Q_OS_WIN
# include <signal.h>
#endif

/// TODO: dep / rates to separate db
/// TODO: transactions this db, but separate process
/// TODO: db_check as callable function, app just wrapp it
/// TODO: sanity checks severity -- critical, repairable, warning
/// TODO: parallel processing secrets
/// TODO: switch to backup db


static bool exit_asked = false;
#ifndef Q_OS_WIN
void sig_handler(int signum)
{
    if (signum == SIGINT)
        exit_asked = true;
}
#endif

int main(int argc, char *argv[])
{
#ifndef Q_OS_WIN
    signal(SIGINT, sig_handler);
#endif

    QCoreApplication app(argc, argv);

    QString iniFilePath = QCoreApplication::applicationDirPath() + "/../data/trader.ini";
    if (!QFileInfo(iniFilePath).exists())
    {
        std::cerr << "*** No INI file!" << std::endl;
        return 0;
    }
    QSettings settings(iniFilePath, QSettings::IniFormat);

    Database database(settings);
    database.init();
    if (database.isDbUpgradePerformed())
    {
        std::clog << " *** Database has been upgraded, please run db_check, check warnings and re-run this app" << std::endl;
        return 0;
    }
    BtcTradeApi::enableTradeLog(QCoreApplication::applicationDirPath() + "/../data/trade.log");

    QThread statusServerThread;
    StatusServer statusServer;
    statusServer.moveToThread(&statusServerThread);
    statusServerThread. connect (&statusServerThread, SIGNAL(started()), &statusServer, SLOT(start()));
    Trader trader(settings, database, exit_asked);

    app.connect(&trader, SIGNAL(statusChanged(StatusServer::State)), &statusServer, SLOT(onStatusChange(StatusServer::State)));
    app.connect (&trader, &Trader::done, &statusServerThread, &QThread::quit);
    app.connect (&trader, &Trader::done, &app, &QCoreApplication::quit);

    int ret = app.exec();
    statusServerThread.wait();

    return ret;
}

