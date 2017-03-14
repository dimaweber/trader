#include "btce.h"
#include "utils.h"
#include "key_storage.h"

#include <QString>
#include <QDateTime>
#include <QtMath>
#include <QElapsedTimer>
#include <QRegExp>
#include <QVariant>
#include <QVector>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlDriver>
#include <QSqlError>

#include <iostream>

#include <unistd.h>
#include <signal.h>
#include <memory>

#define USE_SQLITE

static CurlWrapper w;

static bool exit_asked = false;

void sig_handler(int signum)
{
    if (signum == SIGINT)
        exit_asked = true;
}

#ifdef USE_SQLITE
#   define SQL_NOW "datetime('now')"
#   define SQL_TRUE "1"
#   define SQL_FALSE "0"
#   define SQL_AUTOINCREMENT ""
#   define SQL_UTF8SUPPORT ""
#   define LEAST "min"
#   define MINUTES_DIFF(y) "round((julianday( 'now' ) - julianday( " #y " )) * 86400/ 60)"
#else
#   define SQL_NOW "now()"
#   define SQL_TRUE "TRUE"
#   define SQL_FALSE "FALSE"
#   define SQL_AUTOINCREMENT "auto_increment"
#   define SQL_UTF8SUPPORT "character set utf8 COLLATE utf8_general_ci"
#   define LEAST "least"
#   define MINUTES_DIFF(y) "timestampdiff(MINUTE, now(), " #y ")"
#endif

#define ORDER_STATUS_CHECKING "-1"
#define ORDER_STATUS_ACTIVE "0"
#define ORDER_STATUS_DONE "1"
#define ORDER_STATUS_CANCEL "2"
#define ORDER_STATUS_PARTIAL "3"

#define CLOSED_ORDER "1"
#define ACTIVE_ORDER "0"

struct SqlVault
{
    SqlVault(QSqlDatabase& db)
        :db(db)
    {
        try
        {
            create_tables();
            prepare();
        }
        catch (const QSqlQuery& e)
        {
            std::cerr << "Fail to perform " << e.lastQuery() << " : " << e.lastError().text() << std::endl;
            throw e;
        }
    }

    bool prepare();
    bool create_tables();

    std::unique_ptr<QSqlQuery> insertOrder;
    std::unique_ptr<QSqlQuery> updateActiveOrder;
    std::unique_ptr<QSqlQuery> updateSetCanceled;
    std::unique_ptr<QSqlQuery> cancelPrevRoundActiveOrders;
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
    QSqlDatabase& db;
};

bool SqlVault::prepare()
{
    auto prepareSql = [this](const QString& str, std::unique_ptr<QSqlQuery>& sql)
    {
        sql.reset(new QSqlQuery(db));
        if (!sql->prepare(str))
            throw *sql;
    };

    prepareSql("UPDATE orders set status=" ORDER_STATUS_CHECKING
               " where status=" ORDER_STATUS_ACTIVE, updateOrdersCheckToActive);

    prepareSql("INSERT INTO orders (order_id, status, type, amount, start_amount, rate, round_id) "
                          " values (:order_id, :status, :type, :amount, :start_amount, :rate, :round_id)", insertOrder);

    prepareSql("UPDATE orders set status=" ORDER_STATUS_ACTIVE ", amount=:amount, rate=:rate "
               " where order_id=:order_id", updateActiveOrder);

    prepareSql("UPDATE orders set status=:status, amount=:amount, start_amount=:start_amount, rate=:rate "
               " where order_id=:order_id", updateSetCanceled);

    prepareSql("UPDATE orders set status=" ORDER_STATUS_CANCEL " where round_id=:round_id and status=" ORDER_STATUS_ACTIVE, cancelPrevRoundActiveOrders);

    prepareSql("SELECT order_id, start_amount, rate from orders o "
               " where o.status < " ORDER_STATUS_DONE " and o.round_id=:round_id and o.type='sell'", selectSellOrder);

    prepareSql("SELECT id, comission, first_step, martingale, dep, coverage, count, currency, goods, secret_id, dep_inc from settings"
               " where enabled=" SQL_TRUE, selectSettings);

    prepareSql("SELECT count(*) from orders o "
               " where o.status= " ORDER_STATUS_ACTIVE" and o.round_id=:round_id", selectCurrentRoundActiveOrdersCount);

    prepareSql("select r.settings_id, sum(o.start_amount - o.amount)*(1-s.comission) as amount, sum((o.start_amount - o.amount) * o.rate) as payment, sum((o.start_amount - o.amount) * o.rate) / sum(o.start_amount - o.amount)/(1-s.comission)/(1-s.comission)*(1+s.profit) as sell_rate, s.profit as profit from orders o left join rounds r on r.round_id=o.round_id left join settings s  on r.settings_id = s.id "
               " where o.type='buy' and r.round_id=:round_id", selectCurrentRoundGain);

    //  / (1-s.first_step)
    prepareSql("select max(o.rate) * (1+s.first_step * 2) from orders o left join rounds r on r.round_id = o.round_id left join settings s on s.id = r.settings_id "
               " where o.status= " ORDER_STATUS_ACTIVE" and o.type='buy' and o.round_id=:round_id", checkMaxBuyRate);

    prepareSql("select sum(start_amount-amount) from orders o "
               " where o.round_id=:round_id and o.type='sell' and o.status > " ORDER_STATUS_DONE, currentRoundPayback);

    prepareSql("SELECT order_id from orders o "
               " where o.status=" ORDER_STATUS_CHECKING " and o.round_id=:round_id order by o.type desc", selectOrdersWithChangedStatus);

    prepareSql("SELECT order_id from orders o "
               " where o.round_id=:round_id and o.status < " ORDER_STATUS_DONE, selectOrdersFromPrevRound);

    prepareSql("select " LEAST "(:last_price, o.rate + (:last_price - o.rate) / 10 * " LEAST "(" MINUTES_DIFF(r.end_time) ", 10)) from rounds r left join orders o on r.round_id=o.round_id "
               " where r.settings_id=:settings_id and o.type='sell' and o.status=" ORDER_STATUS_DONE " group by r.round_id order by r.end_time desc limit 1", selectPrevRoundSellRate);

    prepareSql("select count(id) from transactions t "
               " where t.secret_id=:secret_id", selectMaxTransHistoryId);

    prepareSql("insert into transactions values (:id, :type, :amount, :currency, :description, :status, :secret_id, :timestamp, :order_id)", insertTransaction);

    prepareSql("insert into rounds (settings_id, start_time, income) values (:settings_id, " SQL_NOW ", 0)", insertRound);

    prepareSql("update rounds set income=:income, c_in=:c_in, c_out=:c_out, g_in=:g_in, g_out=:g_out "
               " where round_id=:round_id", updateRound);

    prepareSql("update rounds set end_time=" SQL_NOW ", reason='sell' "
               " where round_id=:round_id", closeRound);

    prepareSql("select round_id from rounds r "
               " where r.settings_id=:settings_id and r.end_time is null", getRoundId);

    prepareSql("select sum(start_amount - amount) * (1-comission) as goods_in, sum((start_amount-amount)*rate)  as currency_out from orders o left join rounds r on r.round_id=o.round_id left join settings s on s.id=r.settings_id "
               " where o.type='buy' and o.round_id=:round_id", roundBuyStat);

    prepareSql("select sum(start_amount-amount) as goods_out, sum((start_amount-amount)*rate)*(1-comission) as currency_in from orders o left join rounds r on r.round_id=o.round_id left join settings s on s.id=r.settings_id "
               " where o.type='sell' and o.round_id=:round_id", roundSellStat);

    prepareSql("update settings set dep = dep+:dep_inc "
               " where id=:settings_id", depositIncrease);

    prepareSql("update orders set round_id=:round_id "
               " where order_id=:order_id", orderTransition);

    prepareSql("update rounds set dep_usage=:usage "
               " where round_id=:round_id", setRoundsDepUsage);

    prepareSql("insert into rates values (:time, :currency, :goods, :buy, :sell, :last, :currency_volume, :goods_volume)", insertRate);

    prepareSql("insert into dep values (:time, :name, :secret, :dep, :orders)", insertDep);

    return true;
}

class TableField
{
public:
    enum Types {Integer, Decimal, Char, Boolean, BigInt, Double, Varchar, Datetime};

    TableField(const QString& name, Types type = Integer, int size=0, qint16 prec=-1)
        :_name(name), _type(type), _size(size), _prec(prec),
          _notNull(false), _primaryKey(false), _autoIncrement(false)
    {}

    TableField& notNull()
    {
        _notNull = true;
        return *this;
    }

    TableField& defaultValue(const QString& value)
    {
        _default = value;
        return *this;
    }

    TableField& defaultValue(float value)
    {
        _default = QString::number(value, 'f');
        return *this;
    }

    TableField& references(const QString& tableName, const QStringList& fields = QStringList())
    {
        _refTable = tableName;
        _refFields = fields;
        return *this;
    }

    TableField& primaryKey(bool isAutoInc = true)
    {
        _primaryKey = true;
        _autoIncrement = isAutoInc;
        return *this;
    }

    TableField& check(const QString& check)
    {
        _check = check;
        return *this;
    }

    operator QString() const
    {
        QString t;
        switch(_type)
        {
            case Decimal: t = "DECIMAL"; break;
            case Integer:  t = "INTEGER"; break;
            case Char: t = "CHAR"; break;
            case Boolean: t = "BOOLEAN"; break;
            case BigInt: t = "BIGINT"; break;
            case Double: t = "DOUBLE"; break;
            case Varchar: t = "VARCHAR"; break;
            case Datetime: t = "DATETIME"; break;
        }

        QString ret = QString("%1 %2").arg(_name).arg(t);
        if (_size > 0)
        {
            if (_prec == -1)
                ret += QString(" (%1)").arg(_size);
            else
                ret += QString(" (%1, %2)").arg(_size).arg(_prec);
        }
        if (_primaryKey)
            ret +=  " PRIMARY KEY ";
        if (_autoIncrement)
            ret += " " SQL_AUTOINCREMENT " ";
        if (_notNull)
            ret += " NOT NULL";
        if (!_default.isEmpty())
            ret += QString(" DEFAULT '%1'").arg(_default);
        if (!_check.isEmpty())
        {
            ret += " CHECK (" + _check + ")";
        }
        if (!_refTable.isEmpty())
        {
            ret += QString (" REFERENCES %1").arg(_refTable);
            if (!_refFields.isEmpty())
                ret += QString(" (%1)").arg(_refFields.join(','));
        }

        return ret;
    }
private:
    QString _name;
    Types _type;
    int _size;
    int _prec;
    bool _notNull;
    bool _primaryKey;
    bool _autoIncrement;
    QString _default;
    QString _refTable;
    QString _check;
    QStringList _refFields;
};

bool SqlVault::create_tables()
{
    QMap<QString, QStringList> createSqls;
    createSqls["settings"]
             << TableField("id", TableField::BigInt).primaryKey(true)
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
             << TableField("enabled", TableField::Boolean).notNull().defaultValue(SQL_TRUE)
             << TableField("secret_id").notNull().references("secrets", {"id"})
             << " FOREIGN KEY(secret_id) REFERENCES secrets(id) ON UPDATE CASCADE ON DELETE RESTRICT"
             ;
    createSqls["orders"]
             <<  TableField("order_id", TableField::BigInt).primaryKey(false)
             <<  TableField("status", TableField::Integer, 11).notNull().defaultValue(0)
             <<  TableField("type", TableField::Char, 4).notNull().defaultValue("buy")
             <<  TableField("amount", TableField::Decimal, 11, 6).notNull().defaultValue(0)
             <<  TableField("rate", TableField::Decimal, 11, 6).notNull().defaultValue(0)
             <<  TableField("start_amount", TableField::Decimal, 11, 6).notNull().defaultValue(0)
             <<  TableField("round_id", TableField::Integer).notNull().references("rounds", {"round_id"})
             << " FOREIGN KEY(round_id) REFERENCES rounds(round_id) ON UPDATE CASCADE ON DELETE RESTRICT"
             ;

    createSqls["secrets"]
            << TableField("id", TableField::Integer).primaryKey(true)
            << TableField("apikey", TableField::Char, 255).notNull()
            << TableField("secret", TableField::Char, 255).notNull()
            << TableField("is_crypted", TableField::Boolean).notNull().defaultValue(SQL_FALSE)
            ;

    createSqls["transactions"]
            << TableField("id", TableField::BigInt).primaryKey(false)
            << TableField("type").notNull().check("type < 6 and type > 0")
            << TableField("amount", TableField::Double).notNull().check("amount>=0")
            << TableField("currency", TableField::Char, 3).notNull()
            << TableField("description", TableField::Varchar, 255)
            << TableField("status").check("status>0 and status<5")
            << TableField("timestamp", TableField::Datetime).notNull()
            << TableField("secret_id").references("secrets", {"id"})
            << TableField("order_id", TableField::BigInt).notNull().defaultValue(0).references("orders", {"order_id"})
            << " FOREIGN KEY(secret_id) REFERENCES secrets(id) ON UPDATE CASCADE ON DELETE RESTRICT"
            << " FOREIGN KEY(order_id) REFERENCES orders(order_id) ON UPDATE CASCADE ON DELETE RESTRICT"
            ;

    createSqls["rounds"]
            << TableField("round_id", TableField::Integer).primaryKey(true)
            << TableField("start_time", TableField::Datetime).notNull()
            << "end_time DATETIME"
            << "income DECIMAL(14,6) default 0"
            << "reason char(16) not null default 'active'"
            << "g_in decimal(14,6) not null default 0"
            << "g_out decimal(14,6) not null default 0"
            << "c_in decimal(14,6) not null default 0"
            << "c_out decimal(14,6) not null default 0"
            << "dep_usage decimal(14,6) not null default 0"
            << TableField("settings_id", TableField::BigInt).notNull().references("settings", {"id"})
            << " FOREIGN KEY(settings_id) REFERENCES settings(id) ON UPDATE CASCADE ON DELETE RESTRICT"
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
            << " FOREIGN KEY(secret_id) REFERENCES secrets(id) ON UPDATE CASCADE ON DELETE RESTRICT"
            << "CONSTRAINT uniq_rate UNIQUE (time, name, secret_id)"
            ;

    createSqls["order_status"]
            << TableField("status_id", TableField::Integer).primaryKey(false)
            << TableField("status", TableField::Char, 16)
            ;

    createSqls["order_type"]
            << TableField("type_id", TableField::Integer).primaryKey(false)
            << TableField("type", TableField::Char, 8)
            ;

    QSqlQuery sql(db);
    for (const QString& tableName : createSqls.keys())
    {
        QStringList& fields = createSqls[tableName];
        QString createSql = QString("CREATE TABLE IF NOT EXISTS %1 (%2) " SQL_UTF8SUPPORT)
                            .arg(tableName)
                            .arg(fields.join(','))
                            ;
        QString caption = QString("create %1 table").arg(tableName);

        performSql(caption, sql, createSql);
    }

    return true;
}

int main(int argc, char *argv[])
{

    signal(SIGINT, sig_handler);

    (void) argc;
    (void) argv;

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

    SqlVault vault(db);

    std::clog << "Initialize key storage ... ";
    SqlKeyStorage storage(db, "secrets");
    storage.setPassword("g00dd1e#wer4");
    std::clog << "ok" << std::endl;

    QMap<int, BtcObjects::Funds> allFunds;
    BtcPublicApi::Info pinfo;
    BtcPublicApi::Ticker pticker;
    BtcPublicApi::Depth pdepth(1000);

    std::clog << "get currencies info ...";
    if (!pinfo.performQuery())
        std::clog << "fail" << std::endl;
    else
        std::clog << "ok" << std::endl;

    int settings_id = 1;
    int round_id = 0;

    double dep = 3.00;
    double first_step = 0.01;
    double martingale = 0.05;
    double coverage = 0.15;
    int n = 8;
    int secret_id = 0;
    QString currency = "usd";
    QString goods = "ppc";
    double dep_inc = 0;

    QElapsedTimer timer;
    timer.start();

    std::clog << "start main cycle" << std::endl;
    while (!exit_asked)
    {
        QDateTime ratesUpdateTime = QDateTime::currentDateTime();
        try
        {
            timer.restart();
            std::clog << std::endl;
            std::clog << ratesUpdateTime.toString() << std::endl;
            std::clog << "update currencies rate info ...";
            if (!pticker.performQuery())
                std::clog << "fail" << std::endl;
            else
            {
                std::clog << "ok" << std::endl;

                QVariantMap params;
                try
                {
                    db.transaction();
                    for (const BtcObjects::Pair& pair: BtcObjects::Pairs::ref().values())
                    {
                        params[":time"] = ratesUpdateTime; //pair.ticker.updated;
                        params[":currency"] = pair.currency();
                        params[":goods"] = pair.goods();
                        params[":buy"] = pair.ticker.buy;
                        params[":sell"] = pair.ticker.sell;
                        params[":last"] = pair.ticker.last;
                        params[":goods_volume"] = pair.ticker.vol;
                        params[":currency_volume"] = pair.ticker.vol_cur;
                        performSql("Add rate", *vault.insertRate, params);
                    }
                    db.commit();
                }
                catch(const QSqlQuery& e)
                {
                    db.rollback();
                }
            }

            std::clog << "update orders depth ...";
            if (!pdepth.performQuery())
                std::clog << "fail" << std::endl;
            else
                std::clog << "ok" << std::endl;

            performSql("Mark all active orders as unknown status", *vault.updateOrdersCheckToActive);

            BtcObjects::Funds onOrders;
            for(int id: storage.allKeys())
            {
                std::clog << "for keypair "  << id << " mark active orders status: " ORDER_STATUS_CHECKING " --> " ORDER_STATUS_ACTIVE  << std::endl;
                storage.setCurrent(id);

                BtcTradeApi::Info info(storage, allFunds[id]);
                performTradeRequest(QString("get funds info for keypair %1").arg(id), info);

                BtcTradeApi::ActiveOrders activeOrders(storage);
                try
                {
                    if (performTradeRequest(QString("get active orders for keypair %1").arg(id), activeOrders))
                    {
                        db.transaction();
                        for (BtcObjects::Order& order: activeOrders.orders)
                        {
                            QVariantMap params;
                            params[":order_id"] = order.order_id;
                            params[":amount"]  = order.amount;
                            params[":rate"] = order.rate;
                            performSql(QString("Order %1 still active, mark it").arg(order.order_id), *vault.updateActiveOrder, params);

                            if (order.type == BtcObjects::Order::Buy)
                                onOrders[order.currency()] += order.amount * order.rate;
                            else
                                onOrders[order.goods()] += order.amount;
                        }
                        db.commit();
                    }
                }
                catch (std::runtime_error& e)
                {
                    if (std::string(e.what()) == "no orders")
                    {
                        // no orders yet - just ignore
                    }
                    else
                        throw e;
                }

                BtcObjects::Funds& funds = allFunds[id];
                QVariantMap params;
                params[":secret"] = id;
                try
                {
                    db.transaction();
                    for (const QString& name: funds.keys())
                    {
                        if (name == key_field)
                            continue;

                        double value = funds[name];
                        params[":name"] = name;
                        params[":dep"] = value;
                        params[":time"] = ratesUpdateTime;
                        params[":orders"] = onOrders[name];
                        performSql("Add dep", *vault.insertDep, params);
                    }
                    db.commit();
                }
                catch(const QSqlQuery& e)
                {
                    db.rollback();
                }

                db.transaction();
                BtcTradeApi::TransHistory hist(storage);
                QVariantMap hist_param;
                hist_param[":secret_id"] = id;
                performSql("get max transaction id", *vault.selectMaxTransHistoryId, hist_param);
                if (vault.selectMaxTransHistoryId->next())
                    hist.setFrom(vault.selectMaxTransHistoryId->value(0).toInt());
                hist.setCount(100).setOrder(false);
                try {
                    performTradeRequest("get history", hist);
                    QRegExp order_rx (":order:([0-9]*):");
                    for(BtcObjects::Transaction transaction: hist.trans)
                    {
                        QVariantMap ins_params;
                        BtcObjects::Order::Id order_id = 0;
                        if (order_rx.indexIn(transaction.desc) > -1)
                            order_id = order_rx.cap(1).toULongLong();
                        ins_params[":id"] = transaction.id;
                        ins_params[":type"] = transaction.type;
                        ins_params[":amount"] = transaction.amount;
                        ins_params[":currency"] = transaction.currency;
                        ins_params[":description"] = transaction.desc;
                        ins_params[":status"] = transaction.status;
                        ins_params[":timestamp"] = transaction.timestamp;
                        ins_params[":secret_id"] = id;
                        ins_params[":order_id"] = order_id;
                        performSql("insert transaction info", *vault.insertTransaction, ins_params);
                    }
                }
                catch (std::runtime_error& e)
                {

                }

                db.commit();
            }

            if (!performSql("get settings list", *vault.selectSettings))
            {
                std::clog << "Sleep for 10 seconds" << std::endl;
                usleep(1000 * 1000 * 10);
                continue;
            }

            while (vault.selectSettings->next())
            {
                settings_id = vault.selectSettings->value(0).toInt();
//				comission = vault.selectSettings->value(1).toDouble();
                first_step = vault.selectSettings->value(2).toDouble();
                martingale = vault.selectSettings->value(3).toDouble();
                dep = vault.selectSettings->value(4).toDouble();
                coverage = vault.selectSettings->value(5).toDouble();
                n = vault.selectSettings->value(6).toInt();
                currency = vault.selectSettings->value(7).toString();
                goods = vault.selectSettings->value(8).toString();
                secret_id = vault.selectSettings->value(9).toInt();
                dep_inc = vault.selectSettings->value(10).toDouble();

                QString pairName = QString("%1_%2").arg(goods, currency);

                if (!BtcObjects::Pairs::ref().contains(pairName))
                {
                    std::cerr << "no pair" << pairName << "available" <<std::endl;
                    continue;
                }

                storage.setCurrent(secret_id);
                BtcObjects::Pair& pair = BtcObjects::Pairs::ref(pairName);
                BtcObjects::Funds& funds = allFunds[secret_id];

                bool round_in_progress = false;

                QVariantMap param;
                QVariantMap insertOrderParam;

                param[":settings_id"] = settings_id;

                performSql("get current round id", *vault.getRoundId, param);
                if (vault.getRoundId->next())
                    round_id = vault.getRoundId->value(0).toInt();
                else
                    round_id = 0;
                param[":round_id"] = round_id;

                std::clog << QString("\n -------     Processing settings_id %1. Pair: %2   [%3]   --------------- ").arg(settings_id).arg(pairName).arg(round_id) << std::endl;
                std::clog << QString("Available: %1 %2, %3 %4") .arg(funds[pair.currency()])
                                                                .arg(pair.currency())
                                                                .arg(funds[pair.goods()])
                                                                .arg(pair.goods())
                         << std::endl;

                std::clog << QString("last buy rate: %1. last sell rate: %2").arg(pair.ticker.buy).arg(pair.ticker.sell) << std::endl;

                if (performSql("get active orders count", *vault.selectCurrentRoundActiveOrdersCount, param))
                    if (vault.selectCurrentRoundActiveOrdersCount->next())
                    {
                        int count = vault.selectCurrentRoundActiveOrdersCount->value(0).toInt();
                        round_in_progress = count > 0;
                        std::clog << QString("active orders count: %1").arg(count)  << std::endl;
                    }

                QVector<BtcObjects::Order::Id> orders_for_round_transition;

                bool sell_order_executed = false;
                db.transaction();
                if (performSql("check if any orders have status changed", *vault.selectOrdersWithChangedStatus, param))
                {
                    while (vault.selectOrdersWithChangedStatus->next())
                    {
                        BtcObjects::Order::Id order_id = vault.selectOrdersWithChangedStatus->value(0).toInt();
                        BtcTradeApi::OrderInfo info(storage, order_id);
                        performTradeRequest(QString("get info for order %1").arg(order_id), info);

                        std::clog << QString("order %1 changed status to %2").arg(order_id).arg(info.order.status) << std::endl;
                        QVariantMap upd_param;
                        upd_param[":order_id"] = order_id;
                        upd_param[":status"] = info.order.status;
                        upd_param[":amount"] = info.order.amount;
                        upd_param[":start_amount"] = info.order.start_amount;
                        upd_param[":rate"] = info.order.rate;

                        performSql(QString("update order %1").arg(order_id), *vault.updateSetCanceled, upd_param);

                        // orders are sorted by type field, so sell orders come first
                        // if  buy order is changed and sell orders have changed also -- we translate buy order to next round
                        if (info.order.type == BtcObjects::Order::Sell)
                        {
                            std::clog << QString("sell order changed status to %1").arg(info.order.status) << std::endl;
                            sell_order_executed = true;

                            double currency_out = 0;
                            double currency_in = 0;
                            double goods_out = 0;
                            double goods_in = 0;
                            QVariantMap round_upd;
                            round_upd[":round_id"] = round_id;

                            performSql("get round buy stats", *vault.roundBuyStat, round_upd);
                            if (vault.roundBuyStat->next())
                            {
                                goods_in = vault.roundBuyStat->value(0).toDouble();
                                currency_out = vault.roundBuyStat->value(1).toDouble();
                            }
                            performSql("get round sell stats", *vault.roundSellStat, round_upd);
                            if (vault.roundSellStat->next())
                            {
                                goods_out = vault.roundSellStat->value(0).toDouble();
                                currency_in = vault.roundSellStat->value(1).toDouble();
                            }

                            double income = currency_in - currency_out;
                            round_upd[":income"] = income;
                            round_upd[":c_in"] = currency_in;
                            round_upd[":c_out"] = currency_out;
                            round_upd[":g_in"] = goods_in;
                            round_upd[":g_out"] = goods_out;
                            performSql("update round", *vault.updateRound, round_upd);

                            QVariantMap round_close;
                            round_close[":round_id"] = round_id;
                            performSql("close round", *vault.closeRound, round_close);

                            QVariantMap dep_upd = param;
                            dep += income * dep_inc;
                            dep_upd[":settings_id"] = settings_id;
                            dep_upd[":dep_inc"] = income * dep_inc;
                            performSql("increase deposit", *vault.depositIncrease, dep_upd);

                            round_id = 0;
                            round_in_progress = false;
                        }
                        else
                        {
                            // this is buy order
                            if (sell_order_executed)
                            {
                                // and round has finished (thus -- sell order exists)
                                orders_for_round_transition << order_id;
                            }
                        }
                    }
                }
                db.commit();

                double amount_gain = 0;
                double sell_rate = 0;
                performSql("Get current round amoumt gain", *vault.selectCurrentRoundGain, param);
                if(vault.selectCurrentRoundGain->next())
                {
                    amount_gain = vault.selectCurrentRoundGain->value(1).toDouble();
                    double payment = vault.selectCurrentRoundGain->value(2).toDouble();
                    sell_rate = vault.selectCurrentRoundGain->value(3).toDouble();
                    double profit = vault.selectCurrentRoundGain->value(4).toDouble();

                    std::clog << QString("in current round we got %1 %2 and payed %3 %4 for it. To get %6% profit we need to sell it back with rate %5")
                                 .arg(amount_gain).arg(pair.goods()).arg(payment).arg(pair.currency()).arg(sell_rate).arg(profit * 100)
                              << std::endl;

                    // adjust sell rate -- increase it till lowest price that is greater then sell_rate - 0.001
                    // for example we decide to sell @ 122.340, but currently depth looks like 123.000, 122.890, 122.615, 122.112
                    // so there is no point to create 122.340 order, we can create 122.614 order

                    double calculated_sell_rate = sell_rate;
                    double adjusted_sell_rate =  sell_rate + 1;
                    double decimal_fix = 0;
//					decimal_fix = qPow(10, -pair.decimal_places);
                    for (BtcObjects::Depth::Position& pos: pair.depth.asks)
                    {
                        if (pos.rate > calculated_sell_rate && pos.rate < adjusted_sell_rate)
                            adjusted_sell_rate = pos.rate;
                    }
                    sell_rate = qMax(adjusted_sell_rate - decimal_fix, calculated_sell_rate);

                    std::clog << QString("After depth lookup, we adjusted sell rate to %1").arg(sell_rate) << std::endl;
                }

                if (qFuzzyIsNull(amount_gain))
                {
                    std::clog << "nothing bought yet in this round. check -- may be we should increase buy rates" << std::endl;
                    performSql("Get maximum buy rate", *vault.checkMaxBuyRate, param);
                    if (vault.checkMaxBuyRate->next())
                    {
                        double rate = vault.checkMaxBuyRate->value(0).toDouble();
                        std::clog << QString("max buy rate is %1, last rate is %2").arg(rate).arg(pair.ticker.last) << std::endl;
                        if (rate > 0 && pair.ticker.last > rate)
                        {
                            std::clog << QString("rate for %1 too high (%2)").arg(pair.name).arg(rate) << std::endl;
                            round_in_progress = false;
                        }
                    }
                }

                if (!round_in_progress)
                {
                    performSql("check for orders left from previous round", *vault.selectOrdersFromPrevRound, param);
                    while(vault.selectOrdersFromPrevRound->next())
                    {
                        BtcObjects::Order::Id order_id = vault.selectOrdersFromPrevRound->value(0).toInt();
                        BtcTradeApi::CancelOrder cancel(storage, funds, order_id);
                        performTradeRequest(QString("cancel order %1").arg(order_id), cancel);

                    }
                    performSql("cancel active orders left from previous round", *vault.cancelPrevRoundActiveOrders, param);

                    std::clog << "New round start. Calculate new buy orders parameters" << std::endl;

                    if (round_id == 0)
                    {
                        performSql("create new round record", *vault.insertRound, param, false);
                        round_id = vault.insertRound->lastInsertId().toInt();
                    }

                    double sum =0;

                    for (int j=0; j<n; j++)
                        sum += qPow(1+martingale, j) * ( 1 - first_step - (coverage - first_step) * j/(n-1));

                    double execute_rate = pair.ticker.last;
                    param[":last_price"] = execute_rate;
                    if (performSql("get prev round sell price", *vault.selectPrevRoundSellRate, param))
                    {
                        if (vault.selectPrevRoundSellRate->next())
                        {
                            execute_rate = vault.selectPrevRoundSellRate->value(0).toDouble();
                        }
                    }
                    double u = dep / execute_rate / sum;
                    //u = qMax(qMin(funds[currency], dep) / execute_rate / sum, pair.min_amount / (1-comission));
                    double total_currency_spent = 0;
                    QVariantMap usage_params;
                    usage_params[":round_id"] = round_id;
                    for(int j=0; j<n; j++)
                    {
                        double amount = u * qPow(1+martingale, j);
                        double rate = execute_rate * ( 1 - first_step - (coverage - first_step) * j/(n-1));

                        if (amount * rate > funds[currency] ||
                            total_currency_spent + amount*rate > dep+0.00001)
                        {
                            std::clog << QString("Not enought %1 for full bids").arg(pair.currency()) << std::endl;
                            break;
                        }

                        int auto_executed_counter = 0;
                        if (amount < pair.min_amount)
                        {
                            std::clog << "proposed trade amount " << amount << " too low (min: " << pair.min_amount << ") -- skip order" << std::endl;
                        }
                        else
                        {
                            BtcTradeApi::Trade trade(storage, funds, pair.name, BtcObjects::Order::Buy, rate, amount);
                            if (performTradeRequest(QString("create %1 order %2 @ %3").arg("buy").arg(amount).arg(rate), trade))
                            {
                                insertOrderParam[":order_id"] = (trade.order_id==0)?-(round_id * 100 + auto_executed_counter++):trade.order_id;
                                insertOrderParam[":status"] = (trade.order_id==0)?1:0;
                                insertOrderParam[":type"] = "buy";
                                insertOrderParam[":amount"] = trade.remains;
                                insertOrderParam[":start_amount"] = trade.received + trade.remains;
                                insertOrderParam[":rate"] = QString::number(rate, 'f', pair.decimal_places);
                                insertOrderParam[":round_id"] = round_id;

                                performSql("insert buy order record", *vault.insertOrder, insertOrderParam);

                                total_currency_spent += amount * rate;
                                std::clog << QString("%1 bid: %2@%3").arg(j+1).arg(amount).arg(rate) << std::endl;
                            }
                        }
                    }
                    usage_params[":usage"] = total_currency_spent;
                    performSql("set dep_usage", *vault.setRoundsDepUsage, usage_params);

                    round_in_progress = true;
                    std::clog << QString("total bid: %1 %2").arg(total_currency_spent).arg(pair.currency())
                              << std::endl;

                    if (!orders_for_round_transition.isEmpty())
                    {
                        std::clog << "some orders from previous round got transition to this round";
                        QVariantMap trans_param;
                        trans_param[":round_id"] = round_id;
                        for(BtcObjects::Order::Id order_id: orders_for_round_transition)
                        {
                            trans_param[":order_id"] = order_id;
                            performSql("transit order", *vault.orderTransition, trans_param);
                        }
                    }
                }
                else
                {
                //	std::cout << qPrintable(QString("round for %1(%2) already in progress").arg(settings_id).arg(pair.name)) << std::endl;
                }

                if (round_in_progress)
                {
                    if (pair.min_amount > amount_gain)
                    {
                        std::clog << "An amount we have ("<< amount_gain <<") is less then minimal trade amount ("<< pair.min_amount <<"). skip creating sell order" << std::endl;
                        continue;
                    }

                    bool need_recreate_sell = !sell_order_executed;
                    BtcObjects::Order::Id sell_order_id = 0;
                    performSql("get sell order id and amount", *vault.selectSellOrder, param);
                    if (vault.selectSellOrder->next())
                    {
                        sell_order_id = vault.selectSellOrder->value(0).toInt();
                        double sell_order_amount = vault.selectSellOrder->value(1).toDouble();
                        double sell_order_rate = vault.selectSellOrder->value(2).toDouble();
                        double closed_sells_sold_amount = 0;
                        performSql("get canelled sell orders sold amount ", *vault.currentRoundPayback, param);
                        if (vault.currentRoundPayback->next())
                        {
                            closed_sells_sold_amount = vault.currentRoundPayback->value(0).toDouble();
                            std::clog << QString("cancelled sells sold %1 %2").arg(closed_sells_sold_amount).arg(pair.goods()) << std::endl;
                        }

                        need_recreate_sell = (amount_gain - sell_order_amount - closed_sells_sold_amount > pair.min_amount && funds[pair.goods()] > 0)
                                || qAbs(sell_rate - sell_order_rate) > qPow(10, -pair.decimal_places);

                        std::clog << QString("found sell order %1 for %2 amount, %4 rate. Need recreate sell order: %3")
                                     .arg(sell_order_id).arg(sell_order_amount).arg(need_recreate_sell?"yes":"no").arg(sell_order_rate)
                                  << std::endl;

                    }

                    if (need_recreate_sell)
                    {
                        if (sell_order_id > 0)
                        {
                            BtcTradeApi::CancelOrder cancel(storage, funds, sell_order_id);
                            performTradeRequest("cancel order", cancel);
                            BtcTradeApi::OrderInfo info(storage, sell_order_id);
                            performTradeRequest("get canceled sell order info", info);

                            QVariantMap upd_param;
                            upd_param[":order_id"] = sell_order_id;
                            upd_param[":status"] = info.order.status;
                            upd_param[":amount"] = info.order.amount;
                            upd_param[":start_amount"] = info.order.start_amount;
                            upd_param[":rate"] = info.order.rate;

                            performSql(QString("update order %1").arg(sell_order_id), *vault.updateSetCanceled, upd_param);

                            if (!qFuzzyCompare(info.order.amount, info.order.start_amount))
                                amount_gain -= (info.order.start_amount - info.order.amount);
                        }

                        amount_gain = qMin(amount_gain, funds[pair.goods()]);

                        if (funds[pair.goods()] - amount_gain < pair.min_amount)
                        {
                            amount_gain = funds[pair.goods()];
                        }

                        if (amount_gain > pair.min_amount)
                        {
                            BtcTradeApi::Trade sell(storage, funds, pair.name, BtcObjects::Order::Sell, sell_rate, amount_gain);
                            if (performTradeRequest(QString("create %1 order %2 @ %3").arg("sell").arg(amount_gain).arg(sell_rate), sell))
                            {
                                insertOrderParam[":order_id"] = (sell.order_id==0)?-(round_id*100+99):sell.order_id;
                                insertOrderParam[":status"] = (sell.order_id==0)?1:0;
                                insertOrderParam[":type"] = "sell";
                                insertOrderParam[":amount"] = sell.remains;
                                insertOrderParam[":start_amount"] = sell.received + sell.remains;
                                insertOrderParam[":rate"] = sell_rate;
                                insertOrderParam[":settings_id"] = settings_id;
                                insertOrderParam[":round_id"] = round_id;

                                performSql("insert sell order record", *vault.insertOrder, insertOrderParam);
                            }
                        }
                    }
                }
            }

            quint64 t = timer.elapsed();
            std::clog << QString("iteration done in %1 ms").arg(t) << std::endl << std::endl << std::endl;
            quint64 ms_sleep = 10 * 1000;
            if (t < ms_sleep)
                usleep(1000 * (ms_sleep-t));
        }
        catch(const QSqlQuery& e)
        {
            std::cerr << "Fail sql query:" << e.executedQuery() << e.lastError().text() << std::endl;
            usleep(1000 * 1000 * 30);
        }
        catch (const BtcTradeApi::Api& e)
        {
            std::cerr << "Fail http query: " << e.error() << std::endl;
            usleep(1000 * 1000 * 60);
        }
        catch (const HttpError& e)
        {
            std::cerr << "Http error: " << e.what() << std::endl;
            usleep(1000 * 1000 * 60);
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Runtime error: " << e.what() << std::endl;
            usleep(1000 * 1000 * 60);
        }
    }
    return 0;
}
