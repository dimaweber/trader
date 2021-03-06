#include "btce.h"

#include <QTimeZone>

double read_double(const QVariantMap& map, const QString& name);
QString read_string(const QVariantMap& map, const QString& name);
qint64 read_long(const QVariantMap& map, const QString& name);
quint64 read_ulong(const QVariantMap& map, const QString& name);
QVariantMap read_map(const QVariantMap& map, const QString& name);
QVariantList read_list(const QVariantMap& map, const QString& name);
QDateTime read_timestamp(const QVariantMap& map, const QString& name);

namespace BtcObjects
{
bool Funds::parse(const QVariantMap& fundsMap)
{
    clear();
    for (QString currencyName: fundsMap.keys())
    {
        double v = fundsMap[currencyName].toDouble();
        insert(currencyName, v);
    }

    return true;
}

void Funds::display() const
{
    for (QString& pairName: keys())
        std::cout << pairName << ' ' << value(pairName) << std::endl;
}

bool Depth::Position::operator <(const Depth::Position& other) const
{
    return rate < other.rate || (rate == other.rate && amount < other.amount);
}

void Depth::display() const {throw 1;}

Pair::~Pair() {}

void Pair::display() const
{
    std::cout	<< QString("%1    : %2").arg("name").arg(name)  << std::endl
                << QString("   %1 : %2").arg("decimal_places").arg(decimal_places) << std::endl
                << QString("   %1 : %2").arg("min_price").arg(min_price) << std::endl
                << QString("   %1 : %2").arg("max_price").arg(max_price) << std::endl
                << QString("   %1 : %2").arg("min_amount").arg(min_amount) << std::endl
                << QString("   %1 : %2").arg("hidden").arg(hidden) << std::endl
                << QString("   %1 : %2").arg("fee").arg(fee) << std::endl;
    ticker.display();
}

QString Pair::currency() const { return name.right(3);}

QString Pair::goods() const { return name.left(3);}

bool Pair::parse(const QVariantMap& map)
{
    decimal_places = read_long(map, "decimal_places");
    min_price = read_double(map, "min_price");
    max_price = read_double(map, "max_price");
    min_amount = read_double(map, "min_amount");
    fee = read_double(map, "fee");
    hidden = read_long(map, "hidden") == 1;
    name = read_string(map, key_field);

    return true;
}

Pairs&Pairs::ref() { static Pairs* pInstance = nullptr; if (!pInstance) pInstance = new Pairs; return *pInstance;}

Pair&Pairs::ref(const QString& pairName){ return ref()[pairName];}

bool Pairs::parse(const QVariantMap& map)
{
    clear();
    for (QString pairName: map.keys())
    {
        if (pairName == key_field)
            continue;

        Pair pair;
        pair.parse(read_map(map, pairName));
        insert(pair.name, pair);
    }

    return true;
}

void Pairs::display() const
{
    for(Pair& pair: values())
        pair.display();
}

void Order::display() const
{
    QString sStatus;
    switch (status)
    {
        case Active: sStatus = "active"; break;
        case Done: sStatus = "done"; break;
        case Canceled: sStatus = "canceled"; break;
        case CanceledPartiallyDone: sStatus = "canceled, patrially done"; break;
        case Unknonwn: sStatus = "invalid"; break;
    }

    std::cout << order_id
              << QString("   status : %1\n").arg(sStatus)
              << QString("   pair   : %1\n").arg(pair)
              << QString("   type   : %1\n").arg((type==Order::Type::Sell)?"sell":"buy")
              << QString("   amount : %1 (%2)").arg(amount).arg(start_amount)
              << QString("   rate   : %1").arg(rate)
              << QString("   created: %1").arg(timestamp_created.toString(Qt::ISODate));
//	qDebug() << QString("      %1 : %2 / %3").arg(mon).arg(gain()).arg(comission());
}

bool Order::parse(const QVariantMap& map)
{
    pair = read_string(map, "pair");
    QString sType = read_string(map, "type");
    if (sType == "sell")
        type = Order::Type::Sell;
    else if (sType == "buy")
        type = Order::Type::Buy;
    else
        throw BadFieldValue("type", sType);

    amount = read_double(map, "amount");
    rate = read_double(map, "rate");
    timestamp_created = read_timestamp(map, "timestamp_created");
    if (map.contains("start_amount"))
        start_amount = read_double(map, "start_amount");
    else
        start_amount = amount;

    if (map.contains("status"))
    {
        long s = read_long(map, "status");
        switch (s)
        {
            case 0: status = Order::Active; break;
            case 1: status = Order::Done; break;
            case 2: status = Order::Canceled; break;
            case 3: status = Order::CanceledPartiallyDone; break;
        }
    }
    else
        status = Order::Active;

    if (map.contains("order_id"))
        order_id = read_ulong(map, "order_id");
    else if (map.contains(key_field))
        order_id = read_ulong(map, key_field);

    return true;
}

bool Ticker::parse(const QVariantMap& map)
{
    name = read_string(map, key_field);
    high = read_double(map, "high");
    low = read_double(map, "low");
    avg = read_double(map, "avg");
    vol = read_double(map, "vol");
    vol_cur = read_double(map, "vol_cur");
    last = read_double(map, "last");
    buy = read_double(map, "buy");
    sell = read_double(map, "sell");
    updated = read_timestamp(map, "updated");

    return true;
}

void Ticker::display() const
{
    std::cout << QString("%1 : %2").arg("high").arg(high)
              << QString("%1 : %2").arg("low").arg(low)
              << qPrintable(QString("%1 : %2").arg("avg").arg(avg))
    << qPrintable(QString("%1 : %2").arg("vol").arg(vol))
    << qPrintable( QString("%1 : %2").arg("vol_cur").arg(vol_cur))
    << qPrintable(QString("%1 : %2").arg("last").arg(last))
    << qPrintable(QString("%1 : %2").arg("buy").arg(buy))
    << qPrintable(QString("%1 : %2").arg("sell").arg(sell))
    << qPrintable(QString("%1 : %2").arg("update").arg(updated.toString()));
}

bool Depth::parse(const QVariantMap& map)
{
    QVariantList lst = read_list(map, "asks");
    QVariantList pos;
    Position p;

    asks.clear();
    bids.clear();

    for(QVariant position: lst)
    {
        pos = position.toList();
        p.amount = pos[1].toDouble();
        p.rate = pos[0].toDouble();
        asks.append(p);
    }
    std::sort(asks.begin(), asks.end());

    lst = read_list(map, "bids");
    for(QVariant position: lst)
    {
        pos = position.toList();
        p.amount = pos[1].toDouble();
        p.rate = pos[0].toDouble();
        bids.append(p);
    }
    std::sort(bids.begin(), bids.end());

    return true;
}

QString Order::goods() const { return pair.left(3);}

QString Order::currency() const {return pair.right(3);}

bool Transaction::parse(const QVariantMap& map)
{
    type = static_cast<int>(read_long(map, "type"));
    amount = read_double(map, "amount");
    currency = read_string(map, "currency");
    desc = read_string(map, "desc");
    status = static_cast<int>(read_long(map, "status"));
    timestamp = read_timestamp(map, "timestamp");

    if (map.contains("transaction_id"))
        id = read_long(map, "transaction_id");
    else if (map.contains(key_field))
        id = read_long(map, key_field);

    return true;
}

bool Trade::parse(const QVariantMap& map)
{
    QString sType = read_string(map, "type");
    if (sType == "ask")
        type = Trade::Type::Ask;
    else if (Q_LIKELY(sType == "bid"))
        type = Trade::Type::Bid;
    else
        type = Trade::Type::Invalid;

    price = read_double(map, "price");
    amount = read_double(map, "amount");
    id = read_ulong(map, "tid");
    timestamp = read_timestamp(map, "timestamp");

    return true;
}

void Trade::display() const
{

}

}

namespace BtcPublicApi
{
QString Ticker::path() const
{
    QString p;
    for(const QString& pairName : BtcObjects::Pairs::ref().keys())
        p += pairName + "-";
    p.chop(1);
    return Api::path() + "ticker/" + p;
}

bool Ticker::parseSuccess(const QVariantMap& returnMap)
{
    for (const QString& pairName: returnMap.keys())
    {
        BtcObjects::Pairs::ref(pairName).ticker.parse(read_map(returnMap, pairName));
    }
    return true;
}

QString Info::path() const
{
    return Api::path() + "info";
}

bool Info::parseSuccess(const QVariantMap& returnMap)
{
    BtcObjects::Pairs::ref().server_time = read_timestamp(returnMap, "server_time");
    BtcObjects::Pairs::ref().parse(read_map(returnMap, "pairs"));

    return true;
}

Depth::Depth(int limit): _limit(limit){}

QString Depth::path() const
{
    QString p;
    for(const QString& pairName : BtcObjects::Pairs::ref().keys())
        p += pairName + "-";
    p.chop(1);
    return QString("%1depth/%2?limit=%3").arg(Api::path()).arg(p).arg(_limit);
}

bool Depth::parseSuccess(const QVariantMap& returnMap)
{
    for (const QString& pairName: returnMap.keys())
    {
        BtcObjects::Pairs::ref(pairName).depth.parse(read_map(returnMap, pairName));
    }

    return true;
}

QString Api::server = "http://example.com";
void Api::setServer(const QString& server)
{
    Api::server = server;
}

QString Api::path() const
{
    return QString("%1/api/3/").arg(BtcPublicApi::Api::server);
}

bool Api::parse(const QByteArray& serverAnswer)
{
    return parseSuccess(HttpQuery::convertReplyToMap(serverAnswer));
}

Trades::Trades(int limit):_limit(limit){}

QString Trades::path() const
{
    QString p;
    for(const QString& pairName : BtcObjects::Pairs::ref().keys())
        p += pairName + "-";
    p.chop(1);
    return QString("%1trades/%2?limit=%3").arg(Api::path()).arg(p).arg(_limit);
}

bool Trades::parseSuccess(const QVariantMap& returnMap)
{
    for (const QString& pairName: returnMap.keys())
    {
        QVariantList lst = read_list(returnMap, pairName);
        BtcObjects::Trade trade;
        for (const QVariant& t: lst)
        {
            trade.parse(t.toMap());
            BtcObjects::Pairs::ref(pairName).trades.append(trade);
        }
    }
    return true;
}

}

namespace BtcTradeApi
{

bool enableTradeLog(const QString &tradeLogFileName)
{
    QFile* file = new QFile(tradeLogFileName);
    if (!file->open(QFile::Append | QFile::WriteOnly))
        return false;

    tradeLogFile.reset(file);
    tradeLogStream.reset(new QTextStream(file));

    return true;
}

void disableTradeLog()
{
    tradeLogStream.reset();
    tradeLogFile.reset();
}

QAtomicInteger<quint32> Api::_nonce = QDateTime::currentDateTime().toTime_t();

bool TransHistory::parseSuccess(const QVariantMap& returnMap)
{
    trans.clear();
    for (QString sId: returnMap.keys())
    {
        if (sId == key_field)
            continue;

        BtcObjects::Transaction transaction;

        transaction.parse(read_map(returnMap, sId));

        trans[transaction.id] = transaction;
    }

    return true;

}

QVariantMap TransHistory::extraQueryParams()
{
    QVariantMap map = Api::extraQueryParams();
    if (_from > -1)
        map["from"] = _from;
    if (_count > -1)
        map["count"] = _count;
    if (_from_id > -1)
        map["from_id"] = _from_id;
    if (_end_id > -1)
        map["end_id"] = _end_id;
    map["order"] = _order?"DESC":"ASC";
    if (_since > -1)
        map["since"] = _since;
    if (_end > -1)
        map["end"] = _end;
    return map;
}

bool Info::parseSuccess(const QVariantMap& returnMap)
{
    funds.parse(read_map(returnMap, "funds"));
    transaction_count = read_ulong(returnMap, "transaction_count");
    open_orders_count = read_ulong(returnMap, "open_orders");
    server_time = read_timestamp(returnMap, "server_time");
    return true;
}

void Info::showSuccess() const
{
    for(QString currency: funds.keys())
        if (funds[currency] > 0)
            std::cout << QString("%1 : %2").arg(currency).arg(QString::number(funds[currency], 'f'));
    std::cout << QString("transactins: %1\n").arg(transaction_count);
    std::cout << QString("open orders : %1\n").arg(open_orders_count);
    std::cout << QString("serverTime : %1\n").arg(server_time.toString());
}

QByteArray Api::queryParams()
{
    QUrlQuery query;
    QVariantMap extraParams = extraQueryParams();
    for(const QString& param: extraParams.keys())
    {
        query.addQueryItem(param, extraParams[param].toString());
    }
    return query.query().toUtf8();
}

bool Api::parse(const QByteArray& serverAnswer)
{
    valid = false;

    try {
        QVariantMap map = HttpQuery::convertReplyToMap(serverAnswer);
        if (read_long(map, "success"))
        {
            success = true;

            parseSuccess(read_map(map, "return"));
        }
        else
        {
            success = false;
            errorMsg = read_string(map, "error");
        }

        valid = true;
    }
    catch(std::runtime_error& e)
    {
        valid = false;
        std::cerr << "broken json/missing field: " << e.what() << std::endl;
    }

    return valid;

}

QVariantMap Api::extraQueryParams()
{
    QVariantMap params;
    params["method"] = methodName();
    params["nonce"] = Api::nonce();
    return params;
}

QString BtcTradeApi::Api::server;
void Api::setServer(const QString& server)
{
    Api::server = server;
}

QString Api::path() const
{
    return QString("%1/tapi").arg(BtcTradeApi::Api::server);
}

void Api::setHeaders(CurlListWrapper& headers)
{
    postParams = queryParams();

    QByteArray sign = hmac_sha512(postParams, storage.secret());
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, postParams.constData());
    headers .append(QString("Key: %1").arg(storage.apiKey().constData()).toUtf8())
            .append(QString("Sign: %1").arg(sign.toHex().constData()).toUtf8());

    HttpQuery::setHeaders(headers);
}

void Api::display() const
{
    if (!isValid())
        std::clog << "failed query" << std::endl;
    else if (!isSuccess())
        std::clog << QString("non success query: %1").arg(errorMsg) << std::endl;
    else
        showSuccess();
}

bool ActiveOrders::parseSuccess(const QVariantMap& returnMap)
{
    orders.clear();
    for (QString sOrderId: returnMap.keys())
    {
        if (sOrderId == key_field)
            continue;

        BtcObjects::Order order;

        //order.order_id = sOrderId.toUInt();

        order.parse(read_map(returnMap, sOrderId));

        orders[order.order_id] = order;
    }

    return true;
}

void ActiveOrders::showSuccess() const
{
    std::cout << "active orders:";
    for(const BtcObjects::Order& order : orders)
    {
        order.display();
    }
}

bool OrderInfo::parseSuccess(const QVariantMap& returnMap)
{
    //order.order_id = order_id;
    valid = order.parse(read_map(returnMap, QString::number(order_id)));

    if (tradeLogStream && order.status != BtcObjects::Order::Active )
    {
        QString status="done";
        switch (order.status)
        {
            case BtcObjects::Order::Active: status = "ACTIVE"; break;
            case BtcObjects::Order::Done:   status = "DONE"; break;
            case BtcObjects::Order::Canceled: status="CANCEL"; break;
            case BtcObjects::Order::CanceledPartiallyDone: status="P-DONE"; break;
            case BtcObjects::Order::Unknonwn: status="UNKNWN"; break;
        }

        *tradeLogStream << QString("[%1] ").arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                        << QString("[?]   id: %3   type: %4   pair: %5   status: %6   rate: %7   amount: %8   start_amount: %9 (diff: %1)")
                           .arg(order.start_amount - order.amount)
                           .arg(QString::number(order_id).leftJustified(10))
                           .arg(QString((order.type==BtcObjects::Order::Type::Sell)?"SELL":"BUY").leftJustified(4))
                           .arg(order.pair.toUpper())
                           .arg(status.leftJustified(6))
                           .arg(QString::number(order.rate, 'f', BtcObjects::Pairs::ref(order.pair).decimal_places).leftJustified(10))
                           .arg(QString::number(order.amount, 'f', 8).leftJustified(10))
                           .arg(QString::number(order.start_amount, 'f', 8).leftJustified(10))
                         << endl;
    }

    return true;
}

QVariantMap OrderInfo::extraQueryParams()
{
    QVariantMap params = Api::extraQueryParams();
    params["order_id"] = order_id;
    return params;
}

void OrderInfo::showSuccess() const
{
    order.display();
}

QString Trade::methodName() const
{
    return "Trade";
}

bool Trade::parseSuccess(const QVariantMap& returnMap)
{
    received = read_double(returnMap, "received");
    remains = read_double(returnMap, "remains");
    order_id = read_ulong(returnMap, "order_id");
    funds.parse(read_map(returnMap, "funds"));

    if (tradeLogStream)
    {
        *tradeLogStream << QString("[%1] [+]   id: %3   type: %4   pair: %5   status: %2   rate: %6   amount: %7   recieved: %8   remain: %9")
                           .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                           .arg(QString(order_id?"CREATE":"I-DONE").leftJustified(6))
                           .arg(QString::number(order_id).leftJustified(10))
                           .arg(QString((type==BtcObjects::Order::Type::Sell)?"SELL":"BUY").leftJustified(4))
                           .arg(pair.toUpper())
                           .arg(QString::number(rate, 'f', BtcObjects::Pairs::ref(pair).decimal_places).leftJustified(10))
                           .arg(QString::number(amount, 'f', 8).leftJustified(10))
                           .arg(QString::number(received, 'f', 8).leftJustified(10))
                           .arg(QString::number(remains, 'f', 8).leftJustified(10))
                         << endl;
    }

    return true;
}

QVariantMap Trade::extraQueryParams()
{
    QVariantMap params = Api::extraQueryParams();
    params["pair"] = pair;
    params["type"] = (type==BtcObjects::Order::Type::Sell)?"sell":"buy";
    params["amount"] = QString::number(amount, 'f', 8);
    params["rate"] = QString::number(rate, 'f', BtcObjects::Pairs::ref(pair).decimal_places);

    return params;
}

void Trade::showSuccess() const
{

}

QString CancelOrder::methodName() const
{
    return "CancelOrder";
}

bool CancelOrder::parseSuccess(const QVariantMap& returnMap)
{
    if (read_ulong(returnMap, "order_id") != order_id)
        throw BadFieldValue("order_id", order_id);

    funds.parse(read_map(returnMap, "funds"));

    if (tradeLogStream)
    {
        *tradeLogStream << QString("[%1] [-]   id: %2")
                           .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                           .arg(QString::number(order_id).leftJustified(10))
                        << endl;
    }
    return true;
}

QVariantMap CancelOrder::extraQueryParams()
{
    QVariantMap params = Api::extraQueryParams();
    params["order_id"] = order_id;
    return params;
}

void CancelOrder::showSuccess() const
{

}
}

bool performTradeRequest(const QString& message, BtcTradeApi::Api& req, bool silent)
{
    bool ok = true;
    if (!silent)
        std::clog << QString("[http] %1 ... ").arg(message);
    ok = req.performQuery();
    if (!ok)
    {
        if (!silent)
            std::clog << "Fail.";
        std::cerr << QString("Failed method: %1").arg(req.methodName());
        throw std::runtime_error(req.methodName().toStdString());
    }
    else
    {
        ok = req.isSuccess();
        if (!ok)
        {
            if (!silent)
                std::clog << "Fail.";
            std::cerr << QString("Non success result: %1").arg(req.error());
            throw std::runtime_error(req.error().toStdString());
        }
    }

    if (ok && !silent)
        std::clog << "ok";

    if(!silent)
        std::clog << std::endl;

    return ok;
}

QString read_string(const QVariantMap& map, const QString& name)
{
    if (!map.contains(name))
        throw MissingField(name);

    return map[name].toString();
}

double read_double(const QVariantMap& map, const QString& name)
{
    bool ok;
    QString v = read_string(map, name);
    double ret = v.toDouble(&ok);

    if (!ok)
        throw BadFieldValue(name, v);

    return ret;
}

qint64 read_long(const QVariantMap& map, const QString& name)
{
    bool ok;
    QString v = read_string(map, name);
    long ret = v.toLongLong(&ok);

    if (!ok)
        throw BadFieldValue(name, v);

    return ret;
}

quint64 read_ulong(const QVariantMap& map, const QString& name)
{
    bool ok;
    QString v = read_string(map, name);
    quint64 ret = v.toULongLong(&ok);

    if (!ok)
        throw BadFieldValue(name, v);

    return ret;
}

QVariantMap read_map(const QVariantMap& map, const QString& name)
{
    if (!map.contains(name))
        throw MissingField(name);

    if (!map[name].canConvert<QVariantMap>())
        throw BrokenJson(QString("Field '%1' value could not be converted to map").arg(name));

    QVariantMap ret = map[name].toMap();
    ret[key_field] = name;

    return ret;
}

QVariantList read_list(const QVariantMap& map, const QString& name)
{
    if (!map.contains(name))
        throw MissingField(name);

    if (!map[name].canConvert<QVariantList>())
        throw BrokenJson(QString("Field '%1' value could not be converted to list").arg(name));

    return map[name].toList();
}

QDateTime read_timestamp(const QVariantMap &map, const QString &name)
{
    QDateTime t = QDateTime::fromTime_t(read_long(map, name));
    QTimeZone zone("Europe/Moscow");
    t.setTimeZone(zone);
    return t;
}
