#include <QByteArray>
#include <QString>
#include <QDebug>
#include <QUrlQuery>
#include <QDateTime>
#include <QJsonDocument>
#include <QFile>
#include <QDataStream>
#include <QtMath>

#include <iostream>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>

#include <curl/curl.h>

#include <readline/readline.h>

typedef const EVP_MD* (*HashFunction)();

QByteArray hmac_sha512(const QByteArray& message, const QByteArray& key, HashFunction func = EVP_sha512)
{
    unsigned char* digest = nullptr;
    unsigned int digest_size = 0;

    digest = HMAC(func(), reinterpret_cast<const unsigned char*>(key.constData()), key.length(),
                          reinterpret_cast<const unsigned char*>(message.constData()), message.length(),
                          digest, &digest_size);

    return QByteArray(reinterpret_cast<char*>(digest), digest_size);
}

double read_double(const QVariantMap& map, const QString& name);
QString read_string(const QVariantMap& map, const QString& name);
long read_long(const QVariantMap& map, const QString& name);
QVariantMap read_map(const QVariantMap& map, const QString& name);
QDateTime read_timestamp(const QVariantMap& map, const QString& name);

class Funds : public QMap<QString, double>
{
public:
    bool parse(const QVariantMap& fundsMap)
    {
        clear();
        for (QString currencyName: fundsMap.keys())
        {
            double v = fundsMap[currencyName].toDouble();
            insert(currencyName, v);
        }

        return true;
    }
    void display() const
    {
        for (QString& pairName: keys())
            qDebug() << pairName << value(pairName);
    }
};

class HttpQuery
{
protected:
    bool valid;

    static size_t writeFunc(char *ptr, size_t size, size_t nmemb, void *userdata);
    QByteArray jsonData;

    virtual  bool performQuery() =0;
    virtual QString path() const = 0;
    virtual bool parseSuccess(const QVariantMap& returnMap) =0;
    virtual bool parse(const QByteArray& serverAnswer) =0;
public:
    HttpQuery():valid(false){}
    bool isValid() const { return valid;}
};

struct Ticker
{
public:
    QString name;
    double high, low, avg, vol, vol_cur, last, buy, sell;
    QDateTime updated;

    Ticker() : high(0), low(0), avg(0), vol(0), vol_cur(0), last(0), buy(0), sell(0){}
    virtual bool parse(const QVariantMap& map);
    virtual void display() const;
};

struct Pair
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

    virtual ~Pair() {}

    virtual void display() const
    {
        qDebug() << QString("%1    : %2").arg("name").arg(name);
        qDebug() << QString("   %1 : %2").arg("decimal_places").arg(decimal_places);
        qDebug() << QString("   %1 : %2").arg("min_price").arg(min_price);
        qDebug() << QString("   %1 : %2").arg("max_price").arg(max_price);
        qDebug() << QString("   %1 : %2").arg("min_amount").arg(min_amount);
        qDebug() << QString("   %1 : %2").arg("hidden").arg(hidden);
        qDebug() << QString("   %1 : %2").arg("fee").arg(fee);
        ticker.display();
    }
    virtual bool parse(const QVariantMap& map);
};

class Pairs: public QMap<QString, Pair>
{
public:
    QDateTime server_time;
    static Pairs& ref() { static Pairs* pInstance = nullptr; if (!pInstance) pInstance = new Pairs; return *pInstance;}
    static Pair& ref(const QString& pairName){ return ref()[pairName];}

    bool parse(const QVariantMap& map)
    {
        clear();
        for (QString pairName: map.keys())
        {
            if (pairName == "__key")
                continue;

            Pair pair;
            pair.parse(read_map(map, pairName));
            insert(pair.name, pair);
        }

        return true;
    }

    virtual void display() const
    {
        for(Pair& pair: values())
            pair.display();
    }
};

class BtcPublicApi : public HttpQuery
{
protected:
    virtual QString path() const override;
    virtual bool parse(const QByteArray& serverAnswer) final override;
public:
    virtual bool performQuery() override final;
};

class BtcPublicTicker : public BtcPublicApi
{
public:
    virtual QString path() const override
    {
        QString p;
        for(const QString& pairName : Pairs::ref().keys())
            p += pairName + "-";
        p.chop(1);
        return BtcPublicApi::path() + "ticker/" + p;
    }
    virtual bool parseSuccess(const QVariantMap& returnMap) override
    {
        for (const QString& pairName: returnMap.keys())
        {
            Pairs::ref(pairName).ticker.parse(read_map(returnMap, pairName));
        }
        return true;
    }
};

class BtcPublicInfo: public BtcPublicApi
{

public:
    virtual QString path() const override { return BtcPublicApi::path() + "info";}
    virtual bool parseSuccess(const QVariantMap& returnMap) override
    {
        Pairs::ref().server_time = read_timestamp(returnMap, "server_time");
        Pairs::ref().parse(read_map(returnMap, "pairs"));

        return true;
    }
};

double get_comission_pct(const QString& pair)
{
    return Pairs::ref(pair).fee;
}

struct Order {
    enum OrderType {Buy, Sell};
    enum OrderStatus {Active=0, Done=1, Canceled=2, CanceledPartiallyDone=3};
    typedef quint32 Id;

    QString pair;
    OrderType type;
    double amount;
    double start_amount;
    double rate;
    QDateTime timestamp_created;
    OrderStatus status;
    Id order_id;

    void display() const;
    bool parse(const QVariantMap& map);

    double total() const { return amount * rate;}
    double gain() const {return ((type==Sell)?total():amount) * (1 - get_comission_pct(pair));}
    double comission() const { return ((type==Sell)?total():amount) * get_comission_pct(pair);}
};

class MissingField : public std::runtime_error
{public : MissingField(const QString& msg): std::runtime_error(msg.toUtf8()){}};

class BrokenJson : public std::runtime_error
{public : BrokenJson(const QString& msg): std::runtime_error(msg.toUtf8()){}};

class HttpError : public std::runtime_error
{public : HttpError(const QString& msg): std::runtime_error(msg.toUtf8()){}};

class BtcTradeApi : public HttpQuery
{
    static quint32 _nonce;
    const QByteArray& apiKey;
    const QByteArray& secret;
    static QString nonce() { return QString::number(++_nonce);}
protected:
    bool success;
    QString errorMsg;

    QUrlQuery query;
    QByteArray queryParams();

    virtual bool parse(const QByteArray& serverAnswer) override;

    virtual QMap<QString, QString> extraQueryParams();
    virtual void showSuccess() const = 0;
    virtual QString methodName() const = 0;

public:
    BtcTradeApi(const QByteArray& apiKey, const QByteArray& secret)
        : HttpQuery(),
          apiKey(apiKey), secret(secret),
          success(false), errorMsg("Not executed")
    {}

    virtual QString path() const override { return "https://btc-e.com/tapi";}
    virtual bool performQuery() override;
    void display() const;
    bool isSuccess() const {return isValid() && success;}
};

quint32 BtcTradeApi::_nonce = QDateTime::currentDateTime().toTime_t();

class Info : public BtcTradeApi
{
    Funds& funds;
    quint64 transaction_count;
    quint64 open_orders_count;
    QDateTime server_time;

    virtual bool parseSuccess(const QVariantMap& returnMap) override;
    virtual QString methodName() const  override {return "getInfo";}
public:
    Info(const QByteArray& apiKey, const QByteArray& secret, Funds& funds):BtcTradeApi(apiKey,secret),funds(funds){}
    void showSuccess() const override;
};

class Trade : public BtcTradeApi
{
    QString pair;
    Order::OrderType type;
    double rate;
    double amount;
protected:
    virtual QString methodName() const override;
    virtual bool parseSuccess(const QVariantMap& returnMap) override;
    virtual QMap<QString, QString> extraQueryParams() override;
    virtual void showSuccess() const override;

public:
    double received;
    double remains;
    Order::Id order_id;
    Funds& funds;

    Trade(const QByteArray& apiKey, const QByteArray& secret, Funds& funds,
          const QString& pair, Order::OrderType type, double rate, double amount)
        :BtcTradeApi(apiKey, secret), pair(pair), type(type), rate(rate), amount(amount),
          funds(funds)
    {}
};

class CancelOrder : public BtcTradeApi
{
    Order::Id order_id;

public:
    CancelOrder(const QByteArray& apiKey, const QByteArray& secret, Funds& funds, Order::Id order_id)
        :BtcTradeApi(apiKey, secret), order_id(order_id), funds(funds)
    {}

    Funds& funds;
protected:
    virtual QString methodName() const override;
    virtual bool parseSuccess(const QVariantMap& returnMap) override;
    virtual QMap<QString, QString> extraQueryParams() override;
    virtual void showSuccess() const override;
};

class ActiveOrders : public BtcTradeApi
{
    virtual bool parseSuccess(const QVariantMap& returnMap) override;
    virtual QString methodName() const  override {return "ActiveOrders";}
public:
    ActiveOrders(const QByteArray& apiKey, const QByteArray& secret):BtcTradeApi(apiKey, secret){}
    QMap<Order::Id, Order> orders;

    void showSuccess() const override;
};

class OrderInfo : public BtcTradeApi
{
    Order::Id order_id;
    virtual bool parseSuccess(const QVariantMap& returnMap) override;
    virtual QString methodName() const  override {return "OrderInfo";}
    virtual QMap<QString, QString> extraQueryParams() override;
public:
    Order order;

    OrderInfo(const QByteArray& apiKey, const QByteArray& secret, Order::Id id)
        :BtcTradeApi(apiKey, secret), order_id(id) {}
    void showSuccess() const override;
};

bool Info::parseSuccess(const QVariantMap& returnMap)
{
    funds.parse(read_map(returnMap, "funds"));
    transaction_count = read_long(returnMap, "transaction_count");
    open_orders_count = read_long(returnMap, "open_orders");
    server_time = read_timestamp(returnMap, "server_time");
    return true;
}

void Info::showSuccess() const
{
    for(QString currency: funds.keys())
        if (funds[currency] > 0)
            qDebug() << QString("%1 : %2").arg(currency).arg(QString::number(funds[currency], 'f'));
    qDebug() << QString("transactins: %1").arg(transaction_count);
    qDebug() << QString("open orders : %1").arg(open_orders_count);
    qDebug() << QString("serverTime : %1").arg(server_time.toString());
}


class KeyStorage
{
    QByteArray _apiKey;
    QByteArray _secret;
    QString _fileName;

protected:
    QString fileName() const { return _fileName;}
    QByteArray getPassword(bool needConfirmation = false) const;
    void read_input(const QString& prompt, QByteArray& ba) const;

    void encrypt(QByteArray& data, const QByteArray& password, QByteArray& ivec);
    void decrypt(QByteArray& data, const QByteArray& password, QByteArray& ivec);

    void load();
    void store();
public:
    KeyStorage(const QString& fileName) : _fileName(fileName){}

    const QByteArray& apiKey() { if (_apiKey.isEmpty()) load(); return _apiKey;}
    const QByteArray& secret() { if (_secret.isEmpty()) load(); return _secret;}

    void changePassword();
};

QByteArray BtcTradeApi::queryParams()
{
    QMap<QString, QString> extraParams = extraQueryParams();
    for(QString& param: extraParams.keys())
    {
        query.addQueryItem(param, extraParams[param]);
    }
    return query.query().toUtf8();
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
    double ret = read_string(map, name).toDouble(&ok);

    if (!ok)
        throw BrokenJson(name);

    return ret;
}

long read_long(const QVariantMap& map, const QString& name)
{
    bool ok;
    long ret = read_string(map, name).toLong(&ok);

    if (!ok)
        throw BrokenJson(name);

    return ret;
}

QVariantMap read_map(const QVariantMap& map, const QString& name)
{
    if (!map.contains(name))
        throw MissingField(name);

    if (!map[name].canConvert<QVariantMap>())
        throw BrokenJson(name);

    QVariantMap ret = map[name].toMap();
    ret["__key"] = name;

    return ret;
}

QDateTime read_timestamp(const QVariantMap &map, const QString &name)
{
    return QDateTime::fromTime_t(read_long(map, name));
}

bool BtcTradeApi::parse(const QByteArray& serverAnswer)
{
    valid = false;

    try {
        QJsonDocument jsonResponce;
        jsonResponce = QJsonDocument::fromJson(serverAnswer);
        QVariant json = jsonResponce.toVariant();

        if (!json.canConvert<QVariantMap>())
            throw BrokenJson("");


        QVariantMap jsonMap = json.toMap();
        if (read_long(jsonMap, "success"))
        {
            success = true;

            parseSuccess(read_map(jsonMap, "return"));
        }
        else
        {
            success = false;
            errorMsg = read_string(jsonMap, "error");
        }

        valid = true;
    }
    catch(std::runtime_error& e)
    {
        valid = false;
        qWarning() << "broken json/missing field: " << e.what();
    }

    return valid;

}

QMap<QString, QString> BtcTradeApi::extraQueryParams()
{
    QMap<QString, QString> params;
    params["method"] = methodName();
    params["nonce"] = BtcTradeApi::nonce();
    return params;
}

size_t HttpQuery::writeFunc(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    QByteArray* array = reinterpret_cast<QByteArray*>(userdata);
    int lenBeforeAppend = array->length();
    array->append(ptr, size * nmemb);
    return array->length() - lenBeforeAppend;
}

bool BtcTradeApi::performQuery()
{
    CURL* curlHandle = nullptr;
    CURLcode curlResult = CURLE_OK;

    valid = false;

    try {
        curlHandle = curl_easy_init();

        struct curl_slist* headers = nullptr;

        query.clear();
        QByteArray params = queryParams();

        QByteArray sign = hmac_sha512(params, secret);
        jsonData.clear();

        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &jsonData);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, BtcTradeApi::writeFunc);

        QByteArray sUrl = path().toUtf8();
        curl_easy_setopt(curlHandle, CURLOPT_URL, sUrl.constData());
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, params.constData());

        headers = curl_slist_append(headers, QString("Key: %1").arg(apiKey.constData()).toUtf8().constData());
        headers = curl_slist_append(headers, QString("Sign: %1").arg(sign.toHex().constData()).toUtf8().constData());

        curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

        qDebug() << "perform query" << sUrl.constData() << params;

        curlResult = curl_easy_perform(curlHandle);
        if (curlResult != CURLE_OK)
            throw HttpError(params);

        long http_code = 0;
        curl_easy_getinfo (curlHandle, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 200 )
        {
                 qWarning() << "Bad HTTP reply code: " << http_code;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curlHandle);
    }
    catch(std::runtime_error& e)
    {
        qWarning() << "error while executing query " << e.what() << " : " << curl_easy_strerror(curlResult);
        return false;
    }

    return parse(jsonData);
}

void BtcTradeApi::display() const
{
    if (!isValid())
        qWarning() << "failed query";
    else if (!isSuccess())
        qWarning() << "non success query: " << errorMsg;
    else
        showSuccess();
}

bool ActiveOrders::parseSuccess(const QVariantMap& returnMap)
{
    orders.clear();
    for (QString sOrderId: returnMap.keys())
    {
        if (sOrderId == "__key")
            continue;

        Order order;

        //order.order_id = sOrderId.toUInt();

        order.parse(read_map(returnMap, sOrderId));

        orders[order.order_id] = order;
    }

    return true;
}

void ActiveOrders::showSuccess() const
{
    qDebug() << "active orders:";
    for(const Order& order : orders)
    {
        order.display();
    }
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
    }

    qDebug() << order_id;
    qDebug() << QString("   status : %1").arg(sStatus);
    qDebug() << QString("   pair   : %1").arg(pair);
    qDebug() << QString("   type   : %1").arg((type==Sell)?"sell":"buy");
    qDebug() << QString("   amount : %1 (%2)").arg(amount).arg(start_amount);
    qDebug() << QString("   rate   : %1").arg(rate);
    qDebug() << QString("   created: %1").arg(timestamp_created.toString());
    qDebug() << QString("      %1 : %2 / %3").arg(total()).arg(gain()).arg(comission());
}

bool Order::parse(const QVariantMap& map)
{
    pair = read_string(map, "pair");
    QString sType = read_string(map, "type");
    if (sType == "sell")
        type = Order::Sell;
    else if (sType == "buy")
        type = Order::Buy;
    else
        throw BrokenJson("type");

    amount = read_double(map, "amount");
    rate = read_double(map, "rate");
    timestamp_created = read_timestamp(map, "timestamp_created");
    if (map.contains("start_amount"))
        start_amount = read_double(map, "start_amount");
    else
        start_amount = amount;

    if (map.contains("status"))
    {
        int s = read_long(map, "status");
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
        order_id = read_long(map, "order_id");
    else if (map.contains("__key"))
        order_id = read_long(map, "__key");

    return true;
}

bool OrderInfo::parseSuccess(const QVariantMap& returnMap)
{
    //order.order_id = order_id;
    valid = order.parse(read_map(returnMap, QString::number(order_id)));

    return true;
}

QMap<QString, QString> OrderInfo::extraQueryParams()
{
    QMap<QString, QString> params = BtcTradeApi::extraQueryParams();
    params["order_id"] = QString::number(order_id);
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
    order_id = read_long(returnMap, "order_id");
    funds.parse(read_map(returnMap, "funds"));

    return true;
}

QMap<QString, QString> Trade::extraQueryParams()
{
    QMap<QString, QString> params = BtcTradeApi::extraQueryParams();
    params["pair"] = pair;
    params["type"] = (type==Order::Sell)?"sell":"buy";
    params["amount"] = QString::number(amount);
    params["rate"] = QString::number(rate);

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
    if (read_long(returnMap, "order_id") != order_id)
        throw BrokenJson("order_id");

    funds.parse(read_map(returnMap, "funds"));

    return true;
}

QMap<QString, QString> CancelOrder::extraQueryParams()
{
    QMap<QString, QString> params = BtcTradeApi::extraQueryParams();
    params["order_id"] = QString::number(order_id);
    return params;
}

void CancelOrder::showSuccess() const
{

}

bool Pair::parse(const QVariantMap& map)
{
    decimal_places = read_long(map, "decimal_places");
    min_price = read_double(map, "min_price");
    max_price = read_double(map, "max_price");
    min_amount = read_double(map, "min_amount");
    fee = read_double(map, "fee");
    hidden = read_long(map, "hidden") == 1;
    name = read_string(map, "__key");

    return true;
}

bool Ticker::parse(const QVariantMap& map)
{
    name = read_string(map, "__key");
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
    qDebug() << QString("%1 : %2").arg("high").arg(high);
    qDebug() << QString("%1 : %2").arg("low").arg(low);
    qDebug() << QString("%1 : %2").arg("avg").arg(avg);
    qDebug() << QString("%1 : %2").arg("vol").arg(vol);
    qDebug() << QString("%1 : %2").arg("vol_cur").arg(vol_cur);
    qDebug() << QString("%1 : %2").arg("last").arg(last);
    qDebug() << QString("%1 : %2").arg("buy").arg(buy);
    qDebug() << QString("%1 : %2").arg("sell").arg(sell);
    qDebug() << QString("%1 : %2").arg("update").arg(updated.toString());
}

bool BtcPublicApi::performQuery()
{
    CURL* curlHandle = nullptr;
    CURLcode curlResult = CURLE_OK;

    jsonData.clear();
    try {
        curlHandle = curl_easy_init();

        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &jsonData);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, BtcTradeApi::writeFunc);

        QByteArray sUrl = path().toUtf8();
        curl_easy_setopt(curlHandle, CURLOPT_URL, sUrl.constData());

        qDebug() << "perform query" << sUrl.constData();

        curlResult = curl_easy_perform(curlHandle);
        if (curlResult != CURLE_OK)
            throw HttpError(sUrl);

        long http_code = 0;
        curl_easy_getinfo (curlHandle, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 200 )
        {
                 qWarning() << "Bad HTTP reply code: " << http_code;
        }

        curl_easy_cleanup(curlHandle);
    }
    catch(std::runtime_error& e)
    {
        qWarning() << "error while executing query " << e.what() << " : " << curl_easy_strerror(curlResult);
        return false;
    }

    return parse(jsonData);
}

QString BtcPublicApi::path() const
{
    return "https://btc-e.com/api/3/";
}

bool BtcPublicApi::parse(const QByteArray& serverAnswer)
{
    QJsonDocument jsonResponce;
    jsonResponce = QJsonDocument::fromJson(serverAnswer);
    QVariant json = jsonResponce.toVariant();

    if (!json.canConvert<QVariantMap>())
        throw BrokenJson("");

    QVariantMap jsonMap = json.toMap();
    return parseSuccess(jsonMap);
}

QByteArray KeyStorage::getPassword(bool needConfirmation) const
{
    QByteArray pass;
    read_input("Password", pass);

    if (needConfirmation)
    {
        QByteArray confirm;
        read_input("Confirm password", confirm);

        if (pass != confirm)
            throw std::runtime_error("password mismatch");
    }

    QByteArray hash(SHA_DIGEST_LENGTH, Qt::Uninitialized);
    SHA1(reinterpret_cast<const unsigned char *>(pass.constData()),
         pass.length(),reinterpret_cast<unsigned char*>(hash.data()));

    return hash;
}

void KeyStorage::read_input(const QString& prompt, QByteArray& ba) const
{
    char* line;
    line = readline((prompt + ": ").toUtf8());

    ba.clear();
    ba.append(line, strlen(line));
    free(line);
}

void KeyStorage::encrypt(QByteArray& data, const QByteArray& password, QByteArray& ivec)
{
    const unsigned char* inbuf;
    unsigned char outbuf[AES_BLOCK_SIZE];

    AES_KEY key;
    AES_set_encrypt_key(reinterpret_cast<const unsigned char*>(password.constData()), 128, &key);

    int ptr = 0;

    while (ptr < data.length())
    {
        int num =0;
        inbuf = reinterpret_cast<const unsigned char*>(data.constData()) + ptr;
        int size = qMin(AES_BLOCK_SIZE, data.length() - ptr);
        AES_cfb128_encrypt(inbuf, outbuf, size, &key, reinterpret_cast<unsigned char*>(ivec.data()), &num, AES_ENCRYPT);
        memcpy(data.data()+ptr, outbuf, size);
        ptr += size;
    }
}

void KeyStorage::decrypt(QByteArray& data, const QByteArray& password, QByteArray& ivec)
{
    const unsigned char* inbuf;
    unsigned char outbuf[AES_BLOCK_SIZE];

    AES_KEY key;
    AES_set_encrypt_key(reinterpret_cast<const unsigned char*>(password.constData()), 128, &key);

    int ptr = 0;

    while (ptr < data.length())
    {
        int num =0;
        inbuf = reinterpret_cast<const unsigned char*>(data.constData()) + ptr;
        int size = qMin(AES_BLOCK_SIZE, data.length() - ptr);
        AES_cfb128_encrypt(inbuf, outbuf, size, &key, reinterpret_cast<unsigned char*>(ivec.data()), &num, AES_DECRYPT);
        memcpy(data.data()+ptr, outbuf, size);
        ptr += size;
    }
}

void KeyStorage::load()
{
    _apiKey.clear();
    _secret.clear();

    QFile file(fileName());
    if (!file.exists())
    {
        qDebug() << "Noi key store found -- creating new one";
        store();
    }

    if (file.open(QFile::ReadOnly))
    {
        QDataStream stream(&file);
        QByteArray encKey;
        QByteArray encSec;
        QByteArray check;
        QByteArray password = getPassword();
        QByteArray ivec = "thiswillbechanged";

        stream >> encKey >> encSec >> check;

        decrypt(encKey, password, ivec);
        decrypt(encSec, password, ivec);
        decrypt(check, password, ivec);

        QByteArray checkSum = hmac_sha512(encKey, encSec);

        if (check != checkSum)
            throw std::runtime_error("bad password");

        _apiKey = encKey;
        _secret = encSec;

        file.close();
    }
}

void KeyStorage::store()
{
    QFile file(fileName());
    if (file.open(QFile::WriteOnly | QFile::Truncate))
    {

        if (_apiKey.isEmpty())
            read_input("Enter apiKey", _apiKey);

        if (_secret.isEmpty())
            read_input("Enter secret", _secret);

        QByteArray password = getPassword(true);
        QByteArray ivec = "thiswillbechanged";

        QDataStream stream(&file);

        QByteArray check = hmac_sha512(_apiKey, _secret);
        encrypt(_apiKey, password, ivec);
        encrypt(_secret, password, ivec);
        encrypt(check, password, ivec);

        stream << _apiKey << _secret << check;

        file.close();
    }
}

void KeyStorage::changePassword()
{
    load();
    store();
}

enum Currency {BTC, USD, EUR, LTC};

const QString& currency_to_string(Currency c)
{
    static const QString usd = "usd";
    static const QString btc = "btc";
    static const QString eur = "eur";
    static const QString ltc = "ltc";
    static const QString none;

    switch (c)
    {
        case BTC: return btc;
        case USD: return usd;
        case EUR: return eur;
        case LTC: return ltc;
        default: return none;
    }
}

template<Currency G, Currency C>
struct Bid
{
    double amount;
    double rate;

    uint64_t order_id;

    double gain() { return amount * rate * (get_comission_pct(pair()));}
    double bid() { return amount * rate;}

    const QString& currency() const { return currency_to_string(C); }
    const QString& goods() const {return currency_to_string(G);}

    QString pair() const { return goods() + "_" + currency(); }
};


int main(int argc, char *argv[])
{
    CURLcode curlResult;
    curlResult = curl_global_init(CURL_GLOBAL_ALL);
    if (curlResult != CURLE_OK)
    {
        qWarning() << curl_easy_strerror(curlResult);
    }

    KeyStorage storage("keystore.enc");
    if (argc>1 && QString(argv[1]) == "-changepassword")
    {
        qDebug() << "change password for keystore";
        storage.changePassword();
        return 0;
    }

    Funds funds;

    BtcPublicInfo pinfo;
    BtcPublicTicker pticker;
    pinfo.performQuery();
    pticker.performQuery();

    Pairs::ref().display();

    Info info(storage.apiKey(), storage.secret(), funds);
    info.performQuery();
    info.display();

    ActiveOrders orders(storage.apiKey(), storage.secret());
    orders.performQuery();
    orders.display();

    for (Order order: orders.orders)
    {
        if (   order.status == Order::Active && order.pair == "btc_usd"
            && order.type == Order::Sell && order.rate > 499)
        {
            CancelOrder cancel(storage.apiKey(), storage.secret(), funds, order.order_id);
            cancel.performQuery();
        }
    }

    OrderInfo order_info(storage.apiKey(), storage.secret(), 958249085);
    order_info.performQuery();
    order_info.display();

    if (order_info.order.pair == "btc_eur" && order_info.order.status == Order::Active)
    {
        CancelOrder cancel(storage.apiKey(), storage.secret(), funds, 958249085);
        cancel.performQuery();
        cancel.display();
    }

    Trade trade_sell(storage.apiKey(), storage.secret(), funds, "btc_usd", Order::Sell, 500, 0.01);
    trade_sell.performQuery();
    trade_sell.display();
    qDebug() << trade_sell.isSuccess();

    Trade trade_buy(storage.apiKey(), storage.secret(), funds, "btc_usd", Order::Buy, 100, 0.01);
    trade_buy.performQuery();
    trade_buy.display();
    qDebug() << trade_buy.isSuccess();

    CancelOrder cancel_sell(storage.apiKey(), storage.secret(), funds, trade_sell.order_id);
    qDebug() << "deleting sell order: " << cancel_sell.performQuery();
    cancel_sell.display();

    CancelOrder cancel_buy(storage.apiKey(), storage.secret(), funds, trade_buy.order_id);
    qDebug() << "deleting buy order: " << cancel_buy.performQuery();
    cancel_buy.display();

    OrderInfo order_info2(storage.apiKey(), storage.secret(), trade_buy.order_id);
    order_info2.performQuery();
    order_info2.display();

    funds.display();

    Bid<LTC, USD> bid;

    double dep = 450;
    double first_step = 0.05;
    double martingale = 0.05;
    double coverage = 0.15;
    double execute_rate = Pairs::ref(bid.pair()).ticker.last;

    int n = 28;

    double u;

    double sum =0;
    for (int j=0; j<n; j++)
    {
        sum += qPow(1+martingale, j) * ( 1 - first_step - (coverage - first_step) * j/(n-1));
    }

    u = qMax(dep / execute_rate / sum, Pairs::ref(bid.pair()).min_amount);

    double total_currency_spent = 0;
    std::vector<Bid<LTC, USD>> forward_bids;
    for(int j=0; j<n; j++)
    {
        bid.amount = u * qPow(1+martingale, j);
        bid.rate = execute_rate * ( 1 - first_step - (coverage - first_step) * j/(n-1));
        if (total_currency_spent + bid.bid() > dep+0.00001)
        {
            std::cout << "not enought usd for full bids" << std::endl;
            break;
        }
        Trade trade(storage.apiKey(), storage.secret(), funds, bid.pair(), Order::Buy, bid.rate, bid.amount);
        trade.performQuery();
        if (trade.isSuccess())
        {
            forward_bids.push_back(bid);
            total_currency_spent +=bid.bid();
            std::cout << j+1 << " bid " <<  bid.amount << '@'  << bid.rate << std::endl;
        }
    }
    std::cout << "total bid: " << total_currency_spent << ' ' << qPrintable(currency_to_string(USD)) << std::endl;

    curl_global_cleanup();
    return 0;
}
