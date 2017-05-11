#ifndef TRADER_H
#define TRADER_H

#include <QtCore/QObject>
#include <QtCore/qglobal.h>

class Trader : public QObject
{
    Q_OBJECT
public:
    explicit Trader(QObject *parent = 0);

signals:

public slots:
};

#endif // TRADER_H