#include "sql_database.h"
#include "utils.h"
#include "tablefield.h"

#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSettings>
#include <QStringList>
#include <QSqlError>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QElapsedTimer>

#ifndef Q_OS_WIN
# include <unistd.h>
#else
# include <Windows.h>
#endif

#define SQL_PARAMS_CHECK

class SqlKeyStorage : public KeyStorage
{
    QString _tableName;
    QSqlDatabase& db;

protected:
    virtual void load() override;
    virtual void store() override;

public:
    SqlKeyStorage(QSqlDatabase& db, const QString& tableName);
};

bool performSql(const QString& message, QSqlQuery& query, const QString& sql, bool silent)
{
    QElapsedTimer executeTimer;
    quint64 elapsed = 0;
    bool ok;
    if (!silent)
        std::clog << QString("[sql] %1:").arg(message) << std::endl;
    executeTimer.start();
    if (sql.isEmpty())
        ok = query.exec();
    else
        ok = query.exec(sql);
    elapsed = executeTimer.elapsed();
    if (!silent)
        std::clog << query.lastQuery() << std::endl;
    if (ok)
    {
        if(!silent)
            std::clog << "ok ";
        if (query.isSelect())
        {
            int count = 0;
            if (query.driver()->hasFeature(QSqlDriver::QuerySize))
                count = query.size();
            else if (!query.isForwardOnly())
            {
                while(query.next())
                    count++;
                query.first();
                query.previous();
            }
            else
                count = -1;

            if (!silent)
                std::clog << QString("(return %1 records). ").arg(count);
        }
        else
            if (!silent)
                std::clog << QString("(affected %1 records). ").arg(query.numRowsAffected());
    }
    else
    {
        if (!silent)
            std::clog << "Fail.";
        std::cerr << "SQL: " << query.lastQuery() << "."
                  << "Reason: " << query.lastError().text();
    }
    if(!silent)
        std::clog << "Done in " << elapsed << "ms" << std::endl;
    if (!ok)
        throw query;
    return ok;
}

bool performSql(const QString& message, QSqlQuery& query, const QVariantMap& binds, bool silent)
{
    for(QString param: binds.keys())
    {
        query.bindValue(param, binds[param]);
        if (!silent)
            std::clog << "\tbind: " << param << " = " << binds[param].toString() << std::endl;
    }
    return performSql(message, query, QString(), silent);
}


Database::Database(QSettings& settings)
    :settings(settings), db_upgraded(false)
{
}

Database::~Database()
{
    db->close();
}

bool Database::init()
{
    try
    {
        connect();

        // orders table is a core table for whole trader, so if it does not exists, we can
        // say that this is new database and we don't need to upgrade it but simply can
        // create tables and put current version into versions table
        bool empty_db = !db->tables().contains("orders");

        if (!empty_db)
        {
            int major = 0;
            int minor = 0;
            performSql("get database version", *sql, "select major, minor from version order by id desc limit 1");
            if (sql->next())
            {
                major = sql->value(0).toInt();
                minor = sql->value(1).toInt();
            }
            else
            {
                // database v1 -- no table present then so no value -- upgrade 1.0 -> 2.0
                major = 1;
                minor = 0;
            }

            while (!(major == DB_VERSION_MAJOR && minor == DB_VERSION_MINOR))
            {
                db_upgraded = execute_upgrade_sql(major, minor);
            }
        }
        else
        {
            create_tables();
            performSql("set database version", *sql, QString("insert into version (major, minor) values (%1, %2)").arg(DB_VERSION_MAJOR).arg(DB_VERSION_MINOR));
        }

        prepare();
    }
    catch (const QSqlQuery& e)
    {
        std::cerr << "Fail to perform " << e.lastQuery() << " : " << e.lastError().text() << std::endl;
        throw;
    }

    return true;
}

bool Database::check_version()
{
    int major = 0;
    int minor = 0;
    performSql("get database version", *sql, "select major, minor from version order by id desc limit 1");
    if (sql->next())
    {
        major = sql->value(0).toInt();
        minor = sql->value(1).toInt();
    }
    else
    {
        // database v1 -- no table present then so no value -- upgrade 1.0 -> 2.0
        major = 1;
        minor = 0;
    }

    return (major == DB_VERSION_MAJOR && minor == DB_VERSION_MINOR);
}

bool Database::transaction() { return db->transaction(); /* TODO: check sql friver has Transaction feature. use sql.exec("START TRANSACTION") if not*/}

bool Database::commit() {return db->commit(); }

bool Database::rollback() {return db->rollback();}

void Database::pack_db()
{
    if (settings.value("debug/pack_db", "false").toBool())
        performSql("remove cancelled orders", *sql, "delete from orders where status_id=2");
}

QSqlQuery Database::getQuery()
{
    return QSqlQuery(*db);
}

bool Database::connect()
{
    settings.sync();
    settings.beginGroup("database");

    if (db && db->isOpen())
    {
        db->close();
    }

    while (!db || !db->isOpen())
    {
        QString db_type = settings.value("type", "unknown").toString();
        if (db_type == "mysql")
        {
            std::clog << "use mysql database" << std::endl;
            foreach (std::unique_ptr<QSqlQuery>* q, preparedQueriesCollection)
            {
                q->reset();
            }
            QSqlDatabase::removeDatabase("trader_db");
            db.reset(new QSqlDatabase(QSqlDatabase::addDatabase("QMYSQL", "trader_db")));
        }
        else
        {
            throw std::runtime_error("unsupported database type");
        }
        db->setHostName(settings.value("host", "localhost").toString());
        db->setUserName(settings.value("user", "user").toString());
        db->setPassword(settings.value("password", "password").toString());
        db->setDatabaseName(settings.value("database", "db").toString());
        db->setPort(settings.value("port", 3306).toInt());
        QString optionsString = QString("MYSQL_OPT_RECONNECT=%1").arg(settings.value("options.reconnect", true).toBool()?"TRUE":"FALSE");
        db->setConnectOptions(optionsString);

        std::clog << "connecting to database ... ";
        if (!db->open())
        {
            std::clog << " FAIL. " << db->lastError().text() << std::endl;
#ifndef Q_OS_WIN
            usleep(1000 * 1000 * 5);
#else
            Sleep(1000);
#endif
        }
        else
        {
            std::clog << " ok" << std::endl;
        }
    }
    settings.endGroup();
    sql.reset(new QSqlQuery(*db));

    std::clog << "Initialize key storage ... ";
    keyStorage.reset(new SqlKeyStorage(*db, "secrets"));
    keyStorage->setPassword(settings.value("secrets/password", "password").toByteArray());
    std::clog << "ok" << std::endl;

    return true;
}

bool Database::prepare()
{
    auto prepareSql = [this](const QString& str, std::unique_ptr<QSqlQuery>& sql)
    {
        sql.reset(new QSqlQuery(*db));
        if (!sql->prepare(str))
            throw *sql;
        preparedQueriesCollection << &sql;
    };

    prepareSql("UPDATE orders set status_id=" ORDER_STATUS_CHECKING
               " where status_id=" ORDER_STATUS_ACTIVE, updateOrdersCheckToActive);

    prepareSql("INSERT INTO orders (order_id, status_id, type, amount, start_amount, rate, round_id, created, modified) "
                          " values (:order_id, :status, :type, :amount, :start_amount, :rate, :round_id, now(), now())", insertOrder);

    prepareSql("UPDATE orders set status_id=" ORDER_STATUS_ACTIVE ", amount=:amount, rate=:rate "
               " where order_id=:order_id", updateActiveOrder);

    prepareSql("UPDATE orders set status_id=:status, amount=:amount, start_amount=:start_amount, rate=:rate, modified=now() "
               " where order_id=:order_id", updateSetCanceled);

    prepareSql("SELECT order_id, start_amount, rate from orders o "
               " where o.status_id < " ORDER_STATUS_DONE " and o.round_id=:round_id and o.type='sell'", selectSellOrder);

    prepareSql("SELECT id, comission, first_step, martingale, dep, coverage, count, currency, goods, secret_id, dep_inc from settings"
               " where enabled=1" , selectSettings);

    prepareSql("SELECT count(*) from orders o "
               " where o.status_id= " ORDER_STATUS_ACTIVE" and o.round_id=:round_id", selectCurrentRoundActiveOrdersCount);

    prepareSql("select r.settings_id, sum(o.start_amount - o.amount)*(1-s.comission) as amount, sum((o.start_amount - o.amount) * o.rate) as payment, sum((o.start_amount - o.amount) * o.rate) / sum(o.start_amount - o.amount)/(1-s.comission)/(1-s.comission)*(1+s.profit) as sell_rate, s.profit as profit from orders o left join rounds r on r.round_id=o.round_id left join settings s  on r.settings_id = s.id "
               " where o.type='buy' and r.round_id=:round_id group by r.round_id", selectCurrentRoundGain);

    //  / (1-s.first_step)
    prepareSql("select max(o.rate) * (1+s.first_step * 2) from orders o left join rounds r on r.round_id = o.round_id left join settings s on s.id = r.settings_id "
               " where o.status_id= " ORDER_STATUS_ACTIVE" and o.type='buy' and o.round_id=:round_id group by o.round_id", checkMaxBuyRate);

    prepareSql("select sum(start_amount-amount) from orders o "
               " where o.round_id=:round_id and o.type='sell' and o.status_id = " ORDER_STATUS_PARTIAL, currentRoundPayback);

    prepareSql("SELECT order_id from orders o "
               " where o.status_id=" ORDER_STATUS_CHECKING " and o.round_id=:round_id order by o.type desc", selectOrdersWithChangedStatus);

    prepareSql("SELECT order_id from orders o "
               " where o.round_id=:round_id and o.status_id < " ORDER_STATUS_DONE, selectOrdersFromPrevRound);

    prepareSql("select  least(:last_price, MIN(o.rate) + (:last_price - MIN(o.rate)) / 10 * least(timestampdiff(MINUTE, r.end_time, now()), 10)) from rounds r left join orders o on r.round_id=o.round_id "
               " where r.settings_id=:settings_id and r.reason='archive' and o.type='sell' and o.status_id in (" ORDER_STATUS_DONE ", " ORDER_STATUS_INSTANT ", " ORDER_STATUS_PARTIAL ") group by r.round_id", selectPrevRoundSellRate);

    prepareSql("select count(id) from transactions t "
               " where t.secret_id=:secret_id", selectMaxTransHistoryId);

    prepareSql("insert into transactions (id, type, amount, currency, description, status, secret_id, timestamp, order_id) values (:id, :type, :amount, :currency, :description, :status, :secret_id, :timestamp, :order_id)", insertTransaction);

    prepareSql("insert into rounds (settings_id, start_time, income) values (:settings_id, now(), 0)", insertRound);

    prepareSql("update rounds set income=:income, c_in=:c_in, c_out=:c_out, g_in=:g_in, g_out=:g_out "
               " where round_id=:round_id", updateRound);

    prepareSql("update rounds set reason='done' "
               " where round_id=:round_id", closeRound);

    prepareSql("update rounds set end_time=now(), reason='archive' "
               " where round_id=:round_id", archiveRound);

    prepareSql("select round_id from rounds r "
               " where r.settings_id=:settings_id and reason='active'", getRoundId);

    prepareSql("select round_id from rounds r "
               " where r.settings_id=:settings_id and reason='archive'", getPrevRoundId);

    prepareSql("select sum(start_amount - amount) * (1-comission) as goods_in, sum((start_amount-amount)*rate)  as currency_out from orders o left join rounds r on r.round_id=o.round_id left join settings s on s.id=r.settings_id "
               " where o.type='buy' and o.round_id=:round_id group by o.round_id", roundBuyStat);

    prepareSql("select sum(start_amount-amount) as goods_out, sum((start_amount-amount)*rate)*(1-comission) as currency_in from orders o left join rounds r on r.round_id=o.round_id left join settings s on s.id=r.settings_id "
               " where o.type='sell' and o.round_id=:round_id group by o.round_id", roundSellStat);

    prepareSql("update settings set dep = dep+:dep_inc "
               " where id=:settings_id", depositIncrease);

    prepareSql("update orders set round_id=:round_id, modified=now() "
               " where order_id=:order_id", orderTransition);

    prepareSql("update rounds r left join orders o on o.round_id=r.round_id set dep_usage=dep_usage - :decrease where o.order_id=:order_id", decreaseRoundDepUsage);
    prepareSql("update rounds set dep_usage=dep_usage + :increase where round_id=:round_id", increaseRoundDepUsage);

    prepareSql("insert into rates (time, currency, goods, buy_rate, sell_rate, last_rate, currency_volume, goods_volume) values (:time, :currency, :goods, :buy, :sell, :last, :currency_volume, :goods_volume)", insertRate);

    prepareSql("insert into dep (time, name, secret_id, value, on_orders) values (:time, :name, :secret, :dep, :orders)", insertDep);

    prepareSql("update orders set status_id=" ORDER_STATUS_TRANSITION ", modified=now() where order_id=:order_id", markForTransition);

    prepareSql("update orders set status_id=-1, modified=now(), round_id=:round_id_to where round_id=:round_id_from and status_id=" ORDER_STATUS_TRANSITION, transitOrders);

    prepareSql("insert into queue (settings_id, amount, rate, put_time) values (:settings_id, :amount, :rate, now())", insertIntoQueue);
    prepareSql("select queue_id, amount, rate from queue where settings_id=:settings_id and executed = 0", getFromQueue);
    prepareSql("update queue set executed=1, status=:status, exec_time=now() where queue_id=:queue_id", markQueueDone);
//    prepareSql("delete from  queue where queue_id=:queue_id", deleteFromQueue);

    return true;
}

bool Database::create_tables()
{
    QMap<QString, QStringList> createSqls;
    createSqls["version"]
            << "id integer primary key auto_increment"
            << "major int not null default 0"
            << "minor int not null default 0"
            << "unique (major, minor)"
            << "upgrade_date timestamp not null default CURRENT_TIMESTAMP"
               ;

    createSqls["queue"]
            << "queue_id INTEGER PRIMARY KEY AUTO_INCREMENT"
            << "settings_id INTEGER NOT NULL"
            << "FOREIGN KEY(settings_id) REFERENCES settings(id) ON UPDATE CASCADE ON DELETE CASCADE"
            << "amount DECIMAL(14,6) not null check(amount>0)"
            << "rate DECIMAL(14,6) not null check (rate>0)"
            << "executed INTEGER not null default 0"
            << "status varchar(255) default null"
            << TableField("put_time", TableField::Datetime).notNull()
            << TableField("exec_time", TableField::Datetime).notNull()
               ;

    createSqls["secrets"]
            << TableField("id", TableField::Integer).primaryKey(true)
            << TableField("apikey", TableField::Char, 255).notNull()
            << TableField("secret", TableField::Char, 255).notNull()
            << TableField("is_crypted", TableField::Boolean).notNull().defaultValue(0)
            ;

    createSqls["settings"]
             << TableField("id", TableField::Integer).primaryKey(true)
             << TableField("profit", TableField::Decimal, 6, 4).notNull().defaultValue(0.0100)
             << TableField("comission", TableField::Decimal, 6, 4).notNull().defaultValue(0.0020)
             << TableField("first_step", TableField::Decimal, 6,4).notNull().defaultValue(0.0500)
             << TableField("martingale", TableField::Decimal, 6,4).notNull().defaultValue(0.0500)
             << TableField("dep", TableField::Decimal, 10,4).notNull().defaultValue(100.0)
             << TableField("coverage", TableField::Decimal, 6,4).notNull().defaultValue(0.1500)
             << TableField("count", TableField::Integer, 11).notNull().defaultValue(10)
             << TableField("currency", TableField::Char, 3).notNull().defaultValue("usd")
             << TableField("goods", TableField::Char, 3).notNull().defaultValue("btc")
             << TableField("dep_inc", TableField::Decimal, 5, 2).notNull().defaultValue(0)
             << TableField("enabled", TableField::Boolean).notNull().defaultValue(1)
             << TableField("secret_id", TableField::Integer).notNull().references("secrets", {"id"})
             << "FOREIGN KEY(secret_id) REFERENCES secrets(id) ON UPDATE CASCADE ON DELETE RESTRICT"
             ;

    createSqls["rounds"]
            << TableField("round_id", TableField::Integer).primaryKey(true)
            << TableField("start_time", TableField::Datetime).notNull()
            << "end_time DATETIME"
            << "income DECIMAL(14,6) default 0"
            << "reason ENUM ('active', 'arvhive', 'done') not null default 'active'"
            << "g_in decimal(14,6) not null default 0"
            << "g_out decimal(14,6) not null default 0"
            << "c_in decimal(14,6) not null default 0"
            << "c_out decimal(14,6) not null default 0"
            << "dep_usage decimal(14,6) not null default 0"
            << TableField("settings_id", TableField::Integer).notNull().references("settings", {"id"})
            << "FOREIGN KEY(settings_id) REFERENCES settings(id) ON UPDATE CASCADE ON DELETE RESTRICT"
            ;

    createSqls["orders"]
             << TableField("order_id", TableField::Integer).primaryKey(false)
             << TableField("status_id", TableField::Integer, 11).notNull().defaultValue(0).references("order_status", {"status_id"})
             << "type ENUM ('buy', 'sell') not null default 'buy'"
             << TableField("amount", TableField::Decimal, 11, 6).notNull().defaultValue(0)
             << TableField("rate", TableField::Decimal, 11, 6).notNull().defaultValue(0)
             << TableField("start_amount", TableField::Decimal, 11, 6).notNull().defaultValue(0)
             << TableField("round_id", TableField::Integer).notNull().references("rounds", {"round_id"})
             << TableField("created", TableField::Datetime).notNull()
             << TableField("modified", TableField::Datetime).notNull()
             << "FOREIGN KEY(round_id) REFERENCES rounds(round_id) ON UPDATE CASCADE ON DELETE RESTRICT"
             << "FOREIGN KEY(status_id) REFERENCES order_status(status_id) ON UPDATE RESTRICT ON DELETE RESTRICT"
             ;

    createSqls["transactions"]
            << TableField("id", TableField::BigInt).primaryKey(false)
            << TableField("type").notNull().check("type < 6 and type > 0")
            << TableField("amount", TableField::Double).notNull().check("amount>=0")
            << TableField("currency", TableField::Char, 3).notNull()
            << TableField("description", TableField::Varchar, 255)
            << TableField("status").check("status>0 and status<5")
            << TableField("timestamp", TableField::Datetime).notNull()
            << TableField("secret_id", TableField::Integer).references("secrets", {"id"})
            << TableField("order_id", TableField::Integer).notNull().defaultValue(0).references("orders", {"order_id"})
            << "FOREIGN KEY(secret_id) REFERENCES secrets(id) ON UPDATE CASCADE ON DELETE RESTRICT"
         //   << "FOREIGN KEY(order_id) REFERENCES orders(order_id) ON UPDATE CASCADE ON DELETE RESTRICT"
            ;

    createSqls["rates"]
            << "time DATETIME not null"
            << "currency char(3) not null"
            << "goods char(3) not null"
            << "buy_rate decimal(14,6) not null"
            << "sell_rate decimal(14,6) not null"
            << "last_rate decimal(14,6) not null"
            << "currency_volume decimal(14,6) not null"
            << "goods_volume decimal(14,6) not null"
            << "CONSTRAINT uniq_rate UNIQUE (time, currency, goods)"
            ;

    createSqls["dep"]
            << "time DATETIME not null"
            << "name char(3) not null"
            << "value decimal(14,6) not null"
            << "on_orders decimal(14,6) not null default 0"
            << "secret_id integer not null references secrets(id)"
            << "FOREIGN KEY(secret_id) REFERENCES secrets(id) ON UPDATE CASCADE ON DELETE RESTRICT"
            << "CONSTRAINT uniq_rate UNIQUE (time, name, secret_id)"
            ;

    createSqls["order_status"]
            << TableField("status_id", TableField::Integer).primaryKey(false)
            << TableField("status", TableField::Char, 16)
            << "CONSTRAINT uniq_status UNIQUE (status)"
            ;

    createSqls["currencies"]
            << TableField("currency_id").primaryKey(true)
            << TableField("name", TableField::Char, 3)
            << "CONSTRAINT uniq_name UNIQUE (name)"
            ;

    sql->exec("SET FOREIGN_KEY_CHECKS = 0");
    for (const QString& tableName : createSqls.keys())
    {
        QStringList& fields = createSqls[tableName];
        QString createSql = QString("CREATE TABLE IF NOT EXISTS %1 (%2) character set utf8 COLLATE utf8_general_ci")
                            .arg(tableName)
                            .arg(fields.join(','))
                            ;
        QString caption = QString("create %1 table").arg(tableName);

        performSql(caption, *sql, createSql);
    }
    sql->exec("SET FOREIGN_KEY_CHECKS = 1");

    return true;
}

bool Database::execute_upgrade_sql(int& major, int& minor)
{
    transaction();
    try
    {
        QFile file;
        QString sqlFilePath = QString(":/sql/db_upgrade_v%1.%2.sql").arg(major).arg(minor);
        file.setFileName(sqlFilePath);
        if (!file.exists())
        {
            std::cerr << "no sql for upgrading database version " << major << "." << minor << std::endl;
            throw std::runtime_error("unable to upgrade database");
        }
        if (!file.open(QFile::ReadOnly))
        {
            std::cerr << "cannot open database upgrafe file " << sqlFilePath << std::endl;
            throw std::runtime_error("unable to upgrade database");
        }
        QTextStream stream(&file);
        QString line;
        QString comment;
        while (!stream.atEnd())
        {
            line = stream.readLine();
            if (line.startsWith("--"))
            {
                comment = line;
                line = stream.readLine();
            }
            else
                comment = "upgrade database";
            if (!line.isEmpty())
                performSql(comment, *sql, line);
        }
        // last line in upgrade script should be next:
        if (!line.startsWith("insert into version(major, minor)"))
        {
            std::cerr << "broken sql upgrade script -- no version update in last line!";
            throw std::runtime_error("unable to upgrade database");
        }

    }
    catch (std::runtime_error& e)
    {
        rollback();
        throw;
    }
    commit();

    performSql("get database version", *sql, "select major, minor from version order by id desc limit 1");
    if (sql->next())
    {
        major = sql->value(0).toInt();
        minor = sql->value(1).toInt();
    }
    else
        throw std::runtime_error("unable to upgrade database");

    return true;
}

void SqlKeyStorage::load()
{
    QSqlQuery selectQ(db);
    QString sql = QString("SELECT apiKey, secret, id, is_crypted from %1").arg(_tableName);
    QSqlQuery cryptQuery(db);
    if (!cryptQuery.prepare("UPDATE secrets set apikey=:apikey, secret=:secret, is_crypted=1 where id=:id"))
        std::cerr << cryptQuery.lastError().text() << std::endl;

    if (selectQ.exec(sql))
    {
        while (selectQ.next())
        {
            QByteArray ivec = "thiswillbechanged";

            bool is_crypted = selectQ.value(3).toBool();
            int id = selectQ.value(2).toInt();
            vault[id].secret = selectQ.value(1).toByteArray();
            vault[id].apikey = selectQ.value(0).toByteArray();

            if (is_crypted)
            {
                vault[id].apikey = QByteArray::fromHex(selectQ.value(0).toByteArray());
                vault[id].secret = QByteArray::fromHex(selectQ.value(1).toByteArray());
                decrypt(vault[id].apikey, getPassword(false), ivec );
                decrypt(vault[id].secret, getPassword(false), ivec );
            }
            else
            {
                QByteArray apikey = vault[id].apikey;
                QByteArray secret = vault[id].secret;

                encrypt(apikey, getPassword(false), ivec);
                encrypt(secret, getPassword(false), ivec);

                cryptQuery.bindValue(":id", id);
                cryptQuery.bindValue(":apikey", apikey.toHex());
                cryptQuery.bindValue(":secret", secret.toHex());
                if (!cryptQuery.exec())
                {
                    std::cerr << cryptQuery.lastError().text() << std::endl;
                }
            }
        }
    }
    else
    {
        std::cerr << QString("fail to retrieve secrets: %1").arg(selectQ.lastError().text()) << std::endl;
    }
}

void SqlKeyStorage::store()
{
    throw 1;
}

SqlKeyStorage::SqlKeyStorage(QSqlDatabase& db, const QString& tableName) : KeyStorage(), _tableName(tableName), db(db){}
