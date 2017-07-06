#include "ratesdb.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#include <QDebug>

RatesDB::RatesDB()
{
    db = new QSqlDatabase (QSqlDatabase::addDatabase("QMYSQL", "conn"));

    db->setDatabaseName("rates");
    db->setHostName("mysql-master.vm.dweber.lan");
    db->setPassword("rates");
    db->setUserName("rates");

    qDebug() << "Connecting to database";
    if (!db->open())
    {
        qCritical() << db->lastError().text();
    }

    query = new QSqlQuery(*db);
    if (!query->prepare("insert into rates (exchange, exch_id, pair, time, rate, amount, type) values (:exchange, :exch_id, :pair, :time, :rate, :amount, :type)  on duplicate key update exch_id=exch_id"))
    {
        qCritical() << query->lastError().text();
    }

}

bool RatesDB::newRate(const QString& ex, int exch_id, const QString& pair, const QDateTime &time, double rate, double amount, const QString& type)
{
    query->bindValue(":exchange", ex);
    query->bindValue(":exch_id", exch_id);
    query->bindValue(":pair", pair);
    query->bindValue(":time", time);
    query->bindValue(":rate", rate);
    query->bindValue(":amount", qAbs(amount));
    query->bindValue(":type", type);

    if (!query->exec())
    {
        qWarning() << query->lastError().text();
        return false;
    }

    return true;
}
