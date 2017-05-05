#ifndef BTCE_H
#define BTCE_H

#include <QMap>
#include <QUrlQuery>
#include <QFile>
#include <QTextStream>

#include <memory>

#include "utils.h"
#include "http_query.h"
#include "key_storage.h"
#include "curl_wrapper.h"

class CommonTest;

namespace BtcObjects {

struct ExchangeObject
{
public:
    virtual bool parse(const QVariantMap& objMap) =0;
    virtual void display() const =0;
};

struct Funds : public QMap<QString, double>, ExchangeObject
{
public:
    bool parse(const QVariantMap& fundsMap) override;
    void display() const override;
};

struct Ticker : public ExchangeObject
{
public:
    QString name;
    double high, low, avg, vol, vol_cur, last, buy, sell;
    QDateTime updated;

    Ticker() : high(0), low(0), avg(0), vol(0), vol_cur(0), last(0), buy(0), sell(0){}
    bool parse(const QVariantMap& map) override;
    void display() const override;
};

struct Depth : public ExchangeObject
{
public:
    struct Position
    {
        double amount;
        double rate;

        bool operator < (const Position& other) const;
    };

    QList<Position> bids;
    QList<Position> asks;

    bool parse(const QVariantMap& map) override;
    void display() const override;
};

struct Pair : public ExchangeObject
{
public:
    QString name;
    int decimal_places;
    double min_price;
    double max_price;
    double min_amount;
    bool hidden;
    double fee;

    Ticker ticker;
    Depth depth;

    virtual ~Pair();

    void display() const override;
    QString currency() const;
    QString goods() const;
    bool parse(const QVariantMap& map) override;
};

struct Pairs: public QMap<QString, Pair>, ExchangeObject
{
    QDateTime server_time;
    static Pairs& ref();
    static Pair& ref(const QString& pairName);

    bool parse(const QVariantMap& map) override;
    void display() const override;
};

struct Order  : public ExchangeObject
{
    enum Type {Buy, Sell, Invalid};
    enum Status {Active=0, Done=1, Canceled=2, CanceledPartiallyDone=3, Unknonwn};
    typedef quint64 Id;

    Order()
        :type(Type::Invalid), amount(0), start_amount(0), rate(0), status(Status::Unknonwn)
    {}
    virtual ~Order()
    {}

    QString pair;
    Type type;
    double amount;
    double start_amount;
    double rate;
    QDateTime timestamp_created;
    Status status;
    Id order_id;

    void display() const override;
    bool parse(const QVariantMap& map) override;

    QString goods() const;
    QString currency() const;
};

struct Transaction : public ExchangeObject
{
    typedef qint64 Id;

    virtual ~Transaction()
    {}

    Id id;
    int type;
    double amount;
    QString currency;
    QString desc;
    int status;
    QDateTime timestamp;

    bool parse(const QVariantMap& map) override;
    void display() const override {throw 1;}
};
}

namespace BtcPublicApi {

class Api : public HttpQuery
{
protected:
    virtual QString path() const override;
    virtual bool parse(const QByteArray& serverAnswer) final override;
};

class Ticker : public Api
{

    virtual QString path() const override;
    virtual bool parseSuccess(const QVariantMap& returnMap) override;
};

class Info: public Api
{

public:
    virtual QString path() const override;
    virtual bool parseSuccess(const QVariantMap& returnMap) override;
};

class Depth : public Api
{
    int _limit;
public:
    explicit Depth(int limit=100);

protected:
    virtual QString path() const override;
    virtual bool parseSuccess(const QVariantMap& returnMap) final override;
};

}

namespace BtcTradeApi
{
static std::unique_ptr<QFile> tradeLogFile;
static std::unique_ptr<QTextStream> tradeLogStream;

bool enableTradeLog(const QString& tradeLogFileName);
void disableTradeLog();

class Api : public HttpQuery
{
    IKeyStorage& storage;
    static QAtomicInteger<quint32> _nonce;
    static QString nonce() { return QString::number(++_nonce);}
    QByteArray postParams;
protected:
    bool success;
    QString errorMsg;

    QByteArray queryParams();

    virtual bool parse(const QByteArray& serverAnswer) override;

    virtual QVariantMap extraQueryParams();
    virtual void showSuccess() const = 0;

public:
    explicit Api(IKeyStorage& storage)
        : HttpQuery(), storage(storage),
          success(false), errorMsg("Not executed")
    {}

    virtual QString path() const override { return "https://btc-e.com/tapi";}
    virtual void setHeaders(CurlListWrapper& headers) override final;
    void display() const;
    bool isSuccess() const {return isValid() && success;}
    QString error() const {return errorMsg;}
    virtual QString methodName() const = 0;

    friend class ::CommonTest;
};

class Info : public Api
{
    BtcObjects::Funds& funds;
    quint64 transaction_count;
    quint64 open_orders_count;
    QDateTime server_time;

    virtual bool parseSuccess(const QVariantMap& returnMap) override;
    virtual QString methodName() const  override {return "getInfo";}
public:
    Info(IKeyStorage& storage, BtcObjects::Funds& funds):Api(storage),funds(funds){}
    void showSuccess() const override;
protected:
    int runme() {return 1;}

    friend class ::CommonTest;
};

class TransHistory : public Api
{
    int _from;
    int _count;
    BtcObjects::Transaction::Id _from_id;
    BtcObjects::Transaction::Id _end_id;
    bool _order;
    int _since;
    int _end;

    virtual bool parseSuccess(const QVariantMap &returnMap) override;
    virtual QString methodName() const  override {return "TransHistory";}

    virtual QVariantMap extraQueryParams() override;
public:
    QMap<BtcObjects::Transaction::Id, BtcObjects::Transaction> trans;

    explicit TransHistory(IKeyStorage& storage)
        :Api(storage), _from(-1), _count(-1), _from_id(-1), _end_id(-1),
          _order(true), _since(-1), _end(-1)
    {}
    TransHistory& setFrom(int from =-1) {_from = from;return *this;}
    TransHistory& setCount(int count =-1) {_count = count;return *this;}
    TransHistory& setFromId(int from_id =-1) {_from_id = from_id;return *this;}
    TransHistory& setEndId(int end_id =-1) {_end_id = end_id;return *this;}
    TransHistory& setOrder(bool desc=true) {_order = desc; return *this;}
    TransHistory& setSince(const QDateTime& d = QDateTime()) {_since = d.isValid()?d.toTime_t():-1;return *this;}
    TransHistory& setEnd(const QDateTime& d = QDateTime()) {_end= d.isValid()?d.toTime_t():-1;return *this;}

    void showSuccess() const override { throw 1;}
};

class Trade : public Api
{
    QString pair;
    BtcObjects::Order::Type type;
    double rate;
    double amount;
protected:
    virtual QString methodName() const override;
    virtual bool parseSuccess(const QVariantMap& returnMap) override;
    virtual QVariantMap extraQueryParams() override;
    virtual void showSuccess() const override;

public:
    double received;
    double remains;
    BtcObjects::Order::Id order_id;
    BtcObjects::Funds& funds;

    Trade(IKeyStorage& storage, BtcObjects::Funds& funds,
          const QString& pair, BtcObjects::Order::Type type, double rate, double amount)
        :Api(storage), pair(pair), type(type), rate(rate), amount(amount),
          received(0), remains(0), funds(funds)
    {}
};

class CancelOrder : public Api
{
    BtcObjects::Order::Id order_id;

public:
    CancelOrder(IKeyStorage& storage, BtcObjects::Funds& funds, BtcObjects::Order::Id order_id)
        :Api(storage), order_id(order_id), funds(funds)
    {}

    BtcObjects::Funds& funds;
protected:
    virtual QString methodName() const override;
    virtual bool parseSuccess(const QVariantMap& returnMap) override;
    virtual QVariantMap extraQueryParams() override;
    virtual void showSuccess() const override;
};

class ActiveOrders : public Api
{
    virtual bool parseSuccess(const QVariantMap& returnMap) override;
    virtual QString methodName() const  override {return "ActiveOrders";}
public:
    explicit ActiveOrders(IKeyStorage& storage):Api(storage){}
    QMap<BtcObjects::Order::Id, BtcObjects::Order> orders;

    void showSuccess() const override;
};

class OrderInfo : public Api
{
    BtcObjects::Order::Id order_id;
    virtual bool parseSuccess(const QVariantMap& returnMap) override;
    virtual QString methodName() const  override {return "OrderInfo";}
    virtual QVariantMap extraQueryParams() override;
public:
    BtcObjects::Order order;

    OrderInfo(IKeyStorage& storage, BtcObjects::Order::Id id)
        :Api(storage), order_id(id) {}
    void showSuccess() const override;
};

}

bool performTradeRequest(const QString& message, BtcTradeApi::Api& req, bool silent=false);

#endif // BTCE_H
