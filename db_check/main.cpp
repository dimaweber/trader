#include <QtCore/QCoreApplication>
#include <QtCore/qglobal.h>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlDriver>
#include <QSqlError>

#include <iostream>

#include <unistd.h>

#include "utils.h"

#define USE_SQLITE

int main(int argc, char *argv[])
{
    QSqlDatabase db;
    while (!db.isOpen())
    {
#ifdef USE_SQLITE
        std::clog << "use sqlite database" << std::endl;
        db = QSqlDatabase::addDatabase("QSQLITE", "trader_db");
        db.setDatabaseName("../data/trader.db");
#else
        std::clog << "use mysql database" << std::endl;
        db = QSqlDatabase::addDatabase("QMYSQL", "trader_db");
        db.setHostName("localhost");
        db.setUserName("trader");
        db.setPassword("traderpassword");
        db.setDatabaseName("trade");
        db.setConnectOptions("MYSQL_OPT_RECONNECT=true");
#endif

        std::clog << "connecting to database ... ";
        if (!db.open())
        {
            std::clog << " FAIL. " << db.lastError().text() << std::endl;
            usleep(1000 * 1000 * 5);
        }
        else
        {
            std::clog << " ok" << std::endl;
#ifndef USE_SQLITE
            QSqlQuery sql(db);
            performSql("set utf8", sql, "SET NAMES utf8");
#endif
        }
    }

    QSqlQuery sql(db);

    std::clog << "Tables existing check: ";
    QStringList tables = {"orders", "settings", "secrets", "rounds", "transactions", "dep", "rates"};
    for(const QString& tableName: tables)
    {
        std::clog << "Table '" << tableName << "': ";
        if (!db.tables().contains(tableName))
            std::clog << " FAIL!";
        std::clog << std::endl;
    }

    std::clog << "check each setting_id has onlu one ective round: " << std::endl;
    performSql("check each setting_id has only one ective round", sql, "select settings_id, count(*) from rounds where reason ='active' group by settings_id", true);
    while (sql.next())
    {
        uint settings_id = sql.value(0).toUInt();
        uint active_rounds_count = sql.value(1).toUInt();
        if (active_rounds_count > 1)
            std::clog << "settings_id " << settings_id << " has more then 1 active round!" << std::endl;
    }

    std::clog << "check orphan orders: "  << std::endl;

    return 0;
}
