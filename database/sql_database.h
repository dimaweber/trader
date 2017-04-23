#ifndef SQLVAULT_H
#define SQLVAULT_H

#include "key_storage.h"

#include <memory>

#define DB_VERSION_MAJOR 2
#define DB_VERSION_MINOR 3

#define ORDER_STATUS_TRANSITION "-2"
#define ORDER_STATUS_CHECKING "-1"
#define ORDER_STATUS_ACTIVE "0"
#define ORDER_STATUS_DONE "1"
#define ORDER_STATUS_CANCEL "2"
#define ORDER_STATUS_PARTIAL "3"
#define ORDER_STATUS_INSTANT "4"

class QSqlQuery;
class QSqlDatabase;
class QSettings;

bool performSql(const QString& message, QSqlQuery& query, const QVariantMap& binds = QVariantMap(), bool silent=false);
bool performSql(const QString& message, QSqlQuery& query, const QString& sql, bool silent=false);

class Database : public IKeyStorage
{
public:
    explicit Database(QSettings& settings);
    virtual ~Database();

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
    std::unique_ptr<QSqlQuery> archiveRound;
    std::unique_ptr<QSqlQuery> getRoundId;
    std::unique_ptr<QSqlQuery> roundBuyStat;
    std::unique_ptr<QSqlQuery> roundSellStat;
    std::unique_ptr<QSqlQuery> depositIncrease;
    std::unique_ptr<QSqlQuery> orderTransition;
    std::unique_ptr<QSqlQuery> setRoundsDepUsage;
    std::unique_ptr<QSqlQuery> insertRate;
    std::unique_ptr<QSqlQuery> insertDep;
    std::unique_ptr<QSqlQuery> updateOrdersCheckToActive;
    std::unique_ptr<QSqlQuery> getPrevRoundId;
    std::unique_ptr<QSqlQuery> markForTransition;
    std::unique_ptr<QSqlQuery> transitOrders;

    std::unique_ptr<QSqlQuery> insertIntoQueue;
    std::unique_ptr<QSqlQuery> getFromQueue;
    std::unique_ptr<QSqlQuery> markQueueDone;
//    std::unique_ptr<QSqlQuery> deleteFromQueue;

    bool init();
    bool check_version();

    bool transaction();
    bool commit();
    bool rollback();

    virtual void setPassword(const QByteArray& pwd) override final { keyStorage->setPassword(pwd); }
    virtual bool setCurrent(int id) override final { return keyStorage->setCurrent(id); }
    virtual const QByteArray& apiKey() override final { return keyStorage->apiKey(); }
    virtual const QByteArray& secret() override final { return keyStorage->secret(); }
    virtual void changePassword() override final { keyStorage->changePassword(); }
    virtual QList<int> allKeys() override final { return keyStorage->allKeys(); }

    void pack_db();
    bool isDbUpgradePerformed() const { return db_upgraded;}

    QSqlQuery getQuery();
private:
    QSettings& settings;
    std::unique_ptr<QSqlDatabase> db;
    std::unique_ptr<QSqlQuery> sql;
    std::unique_ptr<IKeyStorage> keyStorage;

    QList<std::unique_ptr<QSqlQuery>*> preparedQueriesCollection;

    bool db_upgraded;
};


#endif // SQLVAULT_H
