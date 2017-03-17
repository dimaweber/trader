#ifndef SQLVAULT_H
#define SQLVAULT_H

#include "key_storage.h"

#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSettings>
#include <memory>

#define DB_VERSION_MAJOR 2
#define DB_VERSION_MINOR 1

#   define SQL_NOW "now()"
#   define SQL_TRUE "TRUE"
#   define SQL_FALSE "FALSE"
#   define SQL_AUTOINCREMENT "auto_increment"
#   define SQL_UTF8SUPPORT "character set utf8 COLLATE utf8_general_ci"
#   define LEAST "least"
#   define MINUTES_DIFF(y) "timestampdiff(MINUTE, " #y ", now())"

#define ORDER_STATUS_CHECKING "-1"
#define ORDER_STATUS_ACTIVE "0"
#define ORDER_STATUS_DONE "1"
#define ORDER_STATUS_CANCEL "2"
#define ORDER_STATUS_PARTIAL "3"

#define CLOSED_ORDER "1"
#define ACTIVE_ORDER "0"

class Database : public IKeyStorage
{
public:
    Database(QSettings& settings);

    bool connect();
    bool prepare();
    bool create_tables();
    bool execute_upgrade_sql(int& major, int& minor);

    std::unique_ptr<QSqlQuery> insertOrder;
    std::unique_ptr<QSqlQuery> updateActiveOrder;
    std::unique_ptr<QSqlQuery> updateSetCanceled;
    std::unique_ptr<QSqlQuery> selectSellOrder;
    std::unique_ptr<QSqlQuery> selectSettings;
    std::unique_ptr<QSqlQuery> selectCurrentRoundActiveOrdersCount;
    std::unique_ptr<QSqlQuery> selectCurrentRoundGain;
    std::unique_ptr<QSqlQuery> checkMaxBuyRate;
    std::unique_ptr<QSqlQuery> selectOrdersWithChangedStatus;
    std::unique_ptr<QSqlQuery> selectOrdersFromPrevRound;
    std::unique_ptr<QSqlQuery> selectPrevRoundSellRate;
    std::unique_ptr<QSqlQuery> selectMaxTransHistoryId;
    std::unique_ptr<QSqlQuery> insertTransaction;
    std::unique_ptr<QSqlQuery> currentRoundPayback;
    std::unique_ptr<QSqlQuery> insertRound;
    std::unique_ptr<QSqlQuery> updateRound;
    std::unique_ptr<QSqlQuery> closeRound;
    std::unique_ptr<QSqlQuery> getRoundId;
    std::unique_ptr<QSqlQuery> roundBuyStat;
    std::unique_ptr<QSqlQuery> roundSellStat;
    std::unique_ptr<QSqlQuery> depositIncrease;
    std::unique_ptr<QSqlQuery> orderTransition;
    std::unique_ptr<QSqlQuery> setRoundsDepUsage;
    std::unique_ptr<QSqlQuery> insertRate;
    std::unique_ptr<QSqlQuery> insertDep;
    std::unique_ptr<QSqlQuery> updateOrdersCheckToActive;

    bool init();

    bool transaction() { return db.transaction(); }
    bool commit() {return db.commit(); }
    bool rollback() {return db.rollback();}

    virtual void setPassword(const QByteArray& pwd) override final { keyStorage->setPassword(pwd); }
    virtual bool setCurrent(int id) override final { return keyStorage->setCurrent(id); }
    virtual const QByteArray& apiKey() override final { return keyStorage->apiKey(); }
    virtual const QByteArray& secret() override final { return keyStorage->secret(); }
    virtual void changePassword() override final { keyStorage->changePassword(); }
    virtual QList<int> allKeys() override final { return keyStorage->allKeys(); }

    void pack_db();
private:
    QSettings& settings;
    QSqlDatabase db;
    std::unique_ptr<QSqlQuery> sql;
    std::unique_ptr<KeyStorage> keyStorage;

};


#endif // SQLVAULT_H
