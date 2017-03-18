#include <QtCore/QCoreApplication>
#include <QtCore/qglobal.h>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlDriver>
#include <QSqlError>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>

#include <iostream>

#include <unistd.h>

#include "utils.h"

#define DB_VERSION_MAJOR 2
#define DB_VERSION_MINOR 2

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString iniFilePath = QCoreApplication::applicationDirPath() + "/../data/trader.ini";
    if (!QFileInfo(iniFilePath).exists())
    {
        throw std::runtime_error("*** No INI file!");
    }
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("database");

    QSqlDatabase db;
    while (!db.isOpen())
    {
        std::clog << "use mysql database" << std::endl;
        db = QSqlDatabase::addDatabase("QMYSQL", "trader_db");
        if (settings.value("type", "unknown").toString() != "mysql")
            throw std::runtime_error("unsupported database type");

        db.setHostName(settings.value("host", "localhost").toString());
        db.setUserName(settings.value("user", "user").toString());
        db.setPassword(settings.value("password", "password").toString());
        db.setDatabaseName(settings.value("database", "db").toString());
        db.setPort(settings.value("port", 3306).toInt());
        QString optionsString = QString("MYSQL_OPT_RECONNECT=%1").arg(settings.value("options.reconnect", true).toBool()?"TRUE":"FALSE");
        db.setConnectOptions(optionsString);

        std::clog << "connecting to database ... ";
        if (!db.open())
        {
            std::clog << " FAIL. " << db.lastError().text() << std::endl;
            usleep(1000 * 1000 * 5);
        }
        else
        {
            std::clog << " ok" << std::endl;
        }
    }
    settings.endGroup();

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

    QFile file;
    QString sqlFilePath = QString("%1/../sql/sanity_check_v%2.%3.sql").arg(QCoreApplication::applicationDirPath()).arg(DB_VERSION_MAJOR).arg(DB_VERSION_MINOR);
    file.setFileName(sqlFilePath);
    if (!file.exists())
    {
        std::cerr << "no sql for sanity check database version " << DB_VERSION_MAJOR << "." << DB_VERSION_MINOR << std::endl;
        throw std::runtime_error("unable to upgrade database");
    }
    if (!file.open(QFile::ReadOnly))
    {
        std::cerr << "cannot open database sanity check file " << sqlFilePath << std::endl;
        throw std::runtime_error("unable to upgrade database");
    }
    QTextStream stream(&file);
    QString line;
    QString comment;
    quint32 cnt = 0;
    const char* colorRed = "\033[0;31m";
    const char* colorGreen = "\033[0;32m";
    const char* colorDefault = "\033[0m";
    const char* boldText = "\033[1m";

    while (!stream.atEnd())
    {
        line = stream.readLine();
        if (line.startsWith("--"))
        {
            comment = line;
            comment.remove(0, 3);
            line = stream.readLine();
        }
        else
            comment = "check database";
        if (!line.isEmpty())
        {
            std::clog << boldText << QString::number(++cnt).rightJustified(3) << ". " << colorDefault << comment.leftJustified(80, ' ', true) << "  ... ";
            if (!sql.exec(line))
                std::clog << "Fail to execute query " << sql.lastQuery() << ": " << sql.lastError().text() << std::endl;
            else
            {
                if (sql.next())
                {
                    std::clog << colorRed << " FAIL" << colorDefault << std::endl;
                    do
                    {
                        std::clog << "\t" << sql.value(0).toString() << std::endl;
                    } while (sql.next());
                }
                else
                    std::clog << colorGreen << " ok" << colorDefault << std::endl;
            }
        }
    }

    return 0;
}
