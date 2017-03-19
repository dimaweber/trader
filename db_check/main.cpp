#include "utils.h"
#include "sql_database.h"

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


#define DB_VERSION_MAJOR 2
#define DB_VERSION_MINOR 3

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    bool run_fixes = false;

    if (argc>1)
        if (strcmp(argv[1],"--fix") == 0)
            run_fixes = true;

    QString iniFilePath = QCoreApplication::applicationDirPath() + "/../data/trader.ini";
    if (!QFileInfo(iniFilePath).exists())
    {
        throw std::runtime_error("*** No INI file!");
    }
    QSettings settings(iniFilePath, QSettings::IniFormat);

    Database database(settings);
    if (!database.connect())
    {
        throw std::runtime_error("failt to connect to db");
        return 1;
    }

    QSqlQuery sql = database.getQuery();

    QFile file;
    QString sqlFilePath;

    sqlFilePath = QString("%1/../sql/sanity_check_v%2.%3.sql").arg(QCoreApplication::applicationDirPath()).arg(DB_VERSION_MAJOR).arg(DB_VERSION_MINOR);
    file.setFileName(sqlFilePath);
    if (!file.exists())
    {
        sqlFilePath = QString(":/sql/sanity_check_v%1.%2.sql").arg(DB_VERSION_MAJOR).arg(DB_VERSION_MINOR);
        file.setFileName(sqlFilePath);
        if (!file.exists())
        {
            std::cerr << "no sql for sanity check database version " << DB_VERSION_MAJOR << "." << DB_VERSION_MINOR << std::endl;
            throw std::runtime_error("unable to upgrade database");
        }
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

    bool is_fixable = false;
    bool is_round = false;

    while (!stream.atEnd())
    {
        line = stream.readLine();
        if (line.startsWith("--"))
        {
            comment = line;
            is_fixable = comment[2] == 'F';
            is_round = comment[3] == 'R';
            comment.remove(0, 5);
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

                    if (run_fixes && is_fixable)
                    {
                        if (is_round)
                        {
                            QString sqlStr = "create temporary table T as " + line;
                            try
                            {
                                database.transaction();
                                if (!sql.exec(sqlStr))
                                    throw std::runtime_error(QString("fail to create temporary table: %1").arg(sql.lastError().text()).toUtf8().constData());
                                if (!sql.exec("delete from rounds where round_id in (select round_id from T) and reason='done'"))
                                    throw std::runtime_error(QString("fail to delete rounds: %1").arg(sql.lastError().text()).toUtf8().constData());
                                if (!sql.exec("drop table T"))
                                    throw std::runtime_error(QString("fail to drop temporary table: %1").arg(sql.lastError().text()).toUtf8().constData());
                                database.commit();
                                std::clog << "\t FIXED" << std::endl;
                            }
                            catch(std::runtime_error& e)
                            {
                                database.rollback();
                                throw e;
                            }
                        }
                    }
                }
                else
                    std::clog << colorGreen << " ok" << colorDefault << std::endl;


            }
        }
    }

    return 0;
}
