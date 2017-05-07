#ifndef TRADER_H
#define TRADER_H
#include "btce.h"
#include "statusserver.h"

#include <QtCore/QObject>
#include <QtCore/qglobal.h>
#include <QMap>
#include <QTimer>

class QSettings;
class Database;

class Trader : public QObject
{
    Q_OBJECT
    QSettings& settings;
    Database& database;
    QTimer runTimer;
    volatile bool& exit_asked;
    QMap<int, BtcObjects::Funds> allFunds;
    BtcPublicApi::Info pinfo;
    BtcPublicApi::Ticker pticker;
    BtcPublicApi::Depth pdepth;
public:
    explicit Trader(QSettings& settings, Database& database, bool& exit_asked, QObject* parent = nullptr);

signals:
    void done();
    void statusChanged(int);

public slots:
    void process();
};

#endif // TRADER_H
