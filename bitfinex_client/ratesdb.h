#ifndef RATESDB_H
#define RATESDB_H

#include <QObject>

class QSqlDatabase;
class QSqlQuery;

class RatesDB : public QObject
{
    Q_OBJECT
    QSqlDatabase* db;
    QSqlQuery* query;
public:
    explicit RatesDB(QObject *parent = nullptr);

signals:

public slots:
    void newRate(const QString& ex, int exch_id, const QString &pair, const QDateTime& time, double rate, double amount, const QString& type);
};

#endif // RATESDB_H
