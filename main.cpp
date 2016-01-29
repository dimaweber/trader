#include <QByteArray>
#include <QString>
#include <QUrlQuery>
#include <QDateTime>
#include <QJsonDocument>
#include <QFile>
#include <QDataStream>
#include <QtMath>
#include <QElapsedTimer>
#include <QMap>
#include <QVariant>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlDriver>
#include <QSqlError>

#include <iostream>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>

#include <curl/curl.h>

#include <readline/readline.h>

#include <unistd.h>

typedef const EVP_MD* (*HashFunction)();

std::ostream& operator << (std::ostream& stream, const QString& str)
{
	return stream << qPrintable(str);
}


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
QVariantList read_list(const QVariantMap& map, const QString& name);
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
			std::cout << pairName << ' ' << value(pairName) << std::endl;
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

struct Depth
{
public:
	struct Position
	{
		double amount;
		double rate;

		bool operator < (const Position& other) const
		{
			return rate < other.rate || (rate == other.rate && amount < other.amount);
		}
	};

	QList<Position> bids;
	QList<Position> asks;

	virtual bool parse(const QVariantMap& map);
	virtual void display() const {throw 1;}
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
	Depth depth;

	virtual ~Pair() {}

	virtual void display() const
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
	QString currency() const { return name.right(3);}
	QString goods() const { return name.left(3);}
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

class KeyStorage
{
protected:
	struct StoragePair{
		QByteArray apikey;
		QByteArray secret;
	};

	QMap<int, StoragePair> vault;
	QByteArray _hashPwd;
	int currentPair;

	virtual QByteArray getPassword(bool needConfirmation = false);
	void read_input(const QString& prompt, QByteArray& ba) const;

	static void encrypt(QByteArray& data, const QByteArray& password, QByteArray& ivec);
	static void decrypt(QByteArray& data, const QByteArray& password, QByteArray& ivec);

	virtual void load()  =0;
	virtual void store() =0;
public:
	KeyStorage(){}

	virtual void setPassword(const QByteArray& pwd);
	bool setCurrent(int id);
	const QByteArray& apiKey() { if (vault.isEmpty()) load(); return vault[currentPair].apikey;}
	const QByteArray& secret() { if (vault.isEmpty()) load(); return vault[currentPair].secret;}

	void changePassword();

	QList<int> allKeys()  {if (vault.isEmpty()) load(); return vault.keys();}
};

class FileKeyStorage : public KeyStorage
{
	QString _fileName;

protected:
	QString fileName() const { return _fileName;}

	virtual void load() override;
	virtual void store() override;
public:
	FileKeyStorage(const QString& fileName) : KeyStorage(), _fileName(fileName){}
};

class SqlKeyStorage : public KeyStorage
{
	QString _tableName;
	QSqlDatabase& db;

protected:
	virtual void load() override;
	virtual void store() override;

public:
	SqlKeyStorage(QSqlDatabase& db, const QString& tableName) : KeyStorage(), _tableName(tableName), db(db){}
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

class BtcPublicDepth : public BtcPublicApi
{
	int _limit;
public:
	BtcPublicDepth(int limit=100): _limit(limit){}

protected:
	virtual QString path() const override;
	virtual bool parseSuccess(const QVariantMap& returnMap) final override;
};

QString BtcPublicDepth::path() const
{
	QString p;
	for(const QString& pairName : Pairs::ref().keys())
		p += pairName + "-";
	p.chop(1);
	return QString("%1depth/%2?limit=%3").arg(BtcPublicApi::path()).arg(p).arg(_limit);
}

bool BtcPublicDepth::parseSuccess(const QVariantMap& returnMap)
{
	for (const QString& pairName: returnMap.keys())
	{
		Pairs::ref(pairName).depth.parse(read_map(returnMap, pairName));
	}

	return true;
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

double get_comission_pct(const QString& pair)
{
	return Pairs::ref(pair).fee / 100;
}

struct Order {
	enum OrderType {Buy, Sell};
	enum OrderStatus {Active=0, Done=1, Canceled=2, CanceledPartiallyDone=3};
	typedef qint32 Id;

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

	double money_spent() const { return amount * rate;}
	double money_gain() const {return ((type==Sell)?(start_amount-amount)*rate:0) * (1 - get_comission_pct(pair));}
	double amount_gain() const {return ((type==Sell)?0:(start_amount - amount)) * (1 - get_comission_pct(pair));}
	double comission() const { return ((type==Sell)?money_spent():amount) * get_comission_pct(pair);}
};

class MissingField : public std::runtime_error
{public : MissingField(const QString& msg): std::runtime_error(msg.toStdString()){}};

class BrokenJson : public std::runtime_error
{public : BrokenJson(const QString& msg): std::runtime_error(msg.toStdString()){}};

class HttpError : public std::runtime_error
{public : HttpError(const QString& msg): std::runtime_error(msg.toStdString()){}};

class FileKeyStorage;

class BtcTradeApi : public HttpQuery
{
	KeyStorage& storage;
	static quint32 _nonce;
	static QString nonce() { return QString::number(++_nonce);}
protected:
	bool success;
	QString errorMsg;

	QUrlQuery query;
	QByteArray queryParams();

	virtual bool parse(const QByteArray& serverAnswer) override;

	virtual QMap<QString, QString> extraQueryParams();
	virtual void showSuccess() const = 0;

public:
	BtcTradeApi(KeyStorage& storage)
		: HttpQuery(), storage(storage),
		  success(false), errorMsg("Not executed")
	{}

	virtual QString path() const override { return "https://btc-e.com/tapi";}
	virtual bool performQuery() override;
	void display() const;
	bool isSuccess() const {return isValid() && success;}
	QString error() const {return errorMsg;}
	virtual QString methodName() const = 0;
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
	Info(KeyStorage& storage, Funds& funds):BtcTradeApi(storage),funds(funds){}
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

	Trade(KeyStorage& storage, Funds& funds,
		  const QString& pair, Order::OrderType type, double rate, double amount)
		:BtcTradeApi(storage), pair(pair), type(type), rate(rate), amount(amount),
		  funds(funds)
	{}
};

class CancelOrder : public BtcTradeApi
{
	Order::Id order_id;

public:
	CancelOrder(KeyStorage& storage, Funds& funds, Order::Id order_id)
		:BtcTradeApi(storage), order_id(order_id), funds(funds)
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
	ActiveOrders(KeyStorage& storage):BtcTradeApi(storage){}
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

	OrderInfo(KeyStorage& storage, Order::Id id)
		:BtcTradeApi(storage), order_id(id) {}
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
			std::cout << QString("%1 : %2").arg(currency).arg(QString::number(funds[currency], 'f'));
	std::cout << QString("transactins: %1\n").arg(transaction_count);
	std::cout << QString("open orders : %1\n").arg(open_orders_count);
	std::cout << QString("serverTime : %1\n").arg(server_time.toString());
}


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

QVariantList read_list(const QVariantMap& map, const QString& name)
{
	if (!map.contains(name))
		throw MissingField(name);

	if (!map[name].canConvert<QVariantList>())
		throw BrokenJson(name);

	return map[name].toList();
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
		std::cerr << "broken json/missing field: " << e.what() << std::endl;
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

		QByteArray sign = hmac_sha512(params, storage.secret());
		jsonData.clear();

		curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 20L);

		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &jsonData);
		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, HttpQuery::writeFunc);

		QByteArray sUrl = path().toUtf8();
		curl_easy_setopt(curlHandle, CURLOPT_URL, sUrl.constData());
		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, params.constData());

		curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 20L);

		headers = curl_slist_append(headers, QString("Key: %1").arg(storage.apiKey().constData()).toUtf8().constData());
		headers = curl_slist_append(headers, QString("Sign: %1").arg(sign.toHex().constData()).toUtf8().constData());

		curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

//		qDebug() << "perform query" << sUrl.constData() << params;

		curlResult = curl_easy_perform(curlHandle);
		if (curlResult != CURLE_OK)
			throw HttpError(params);

		long http_code = 0;
		curl_easy_getinfo (curlHandle, CURLINFO_RESPONSE_CODE, &http_code);
		if (http_code != 200 )
		{
			std::cerr << "Bad HTTP reply code: " << http_code << std::endl;
			return false;
		}

		curl_slist_free_all(headers);
		curl_easy_cleanup(curlHandle);
	}
	catch(std::runtime_error& e)
	{
		std::cerr << QString("error while executing query %1 : %2").arg(e.what()).arg(curl_easy_strerror(curlResult)) << std::endl;
		return false;
	}

	return parse(jsonData);
}

void BtcTradeApi::display() const
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
	std::cout << "active orders:";
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

	std::cout << order_id
			  << QString("   status : %1\n").arg(sStatus)
			  << QString("   pair   : %1\n").arg(pair)
			  << QString("   type   : %1\n").arg((type==Sell)?"sell":"buy")
			  << QString("   amount : %1 (%2)").arg(amount).arg(start_amount)
			  << QString("   rate   : %1").arg(rate)
			  << QString("   created: %1").arg(timestamp_created.toString());
//	qDebug() << QString("      %1 : %2 / %3").arg(mon).arg(gain()).arg(comission());
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
	params["amount"] = QString::number(amount, 'f', 8);
	params["rate"] = QString::number(rate, 'f', Pairs::ref(pair).decimal_places);

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

bool BtcPublicApi::performQuery()
{
	CURL* curlHandle = nullptr;
	CURLcode curlResult = CURLE_OK;

	jsonData.clear();
	try {
		curlHandle = curl_easy_init();

		curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 20L);

		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &jsonData);
		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, HttpQuery::writeFunc);

		QByteArray sUrl = path().toUtf8();
		curl_easy_setopt(curlHandle, CURLOPT_URL, sUrl.constData());

		curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 20L);
//		qDebug() << "perform query" << sUrl.constData();

		curlResult = curl_easy_perform(curlHandle);
		if (curlResult != CURLE_OK)
			throw HttpError(sUrl);

		long http_code = 0;
		curl_easy_getinfo (curlHandle, CURLINFO_RESPONSE_CODE, &http_code);
		if (http_code != 200 )
		{
			std::cerr << "Bad HTTP reply code: " << http_code << std::endl;
			return false;
		}

		curl_easy_cleanup(curlHandle);
	}
	catch(std::runtime_error& e)
	{
		std::cerr << "error while executing query " << e.what() << " : " << curl_easy_strerror(curlResult) << std::endl;
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

QByteArray KeyStorage::getPassword(bool needConfirmation)
{
	if (_hashPwd.isEmpty())
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

		setPassword(pass);
	}
	return _hashPwd;
}

void KeyStorage::setPassword(const QByteArray& pwd)
{
	QByteArray hash(SHA_DIGEST_LENGTH, Qt::Uninitialized);
	SHA1(reinterpret_cast<const unsigned char *>(pwd.constData()),
		 pwd.length(),reinterpret_cast<unsigned char*>(hash.data()));

	_hashPwd = hash;
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

bool KeyStorage::setCurrent(int id)
{
	currentPair = id;
	return vault.contains(id);
}

void FileKeyStorage::load()
{
	vault.clear();

	QFile file(fileName());
	if (!file.exists())
	{
		std::clog << "No key store found -- creating new one" << std::endl;
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

		vault[0].apikey = encKey;
		vault[0].secret = encSec;

		file.close();
	}
}

void FileKeyStorage::store()
{
	QFile file(fileName());
	if (file.open(QFile::WriteOnly | QFile::Truncate))
	{

		if (vault[0].apikey.isEmpty())
			read_input("Enter apiKey", vault[0].apikey);

		if (vault[0].secret.isEmpty())
			read_input("Enter secret", vault[0].secret);

		QByteArray password = getPassword(true);
		QByteArray ivec = "thiswillbechanged";

		QDataStream stream(&file);

		QByteArray check = hmac_sha512(vault[0].apikey, vault[0].secret);
		encrypt(vault[0].apikey, password, ivec);
		encrypt(vault[0].secret, password, ivec);
		encrypt(check, password, ivec);

		stream << vault[0].apikey << vault[0].secret << check;

		file.close();
	}
}

void KeyStorage::changePassword()
{
	load();
	store();
}

void SqlKeyStorage::load()
{
	QSqlQuery selectQ(db);
	QString sql = QString("SELECT apiKey, secret, id, is_crypted from %1").arg(_tableName);
	QSqlQuery cryptQuery(db);
	if (!cryptQuery.prepare("UPDATE secrets set apikey=:apikey, secret=:secret, is_crypted='true' where id=:id"))
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
				cryptQuery.bindValue(":apikey", apikey);
				cryptQuery.bindValue(":secret", secret);
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

class CurlWrapper
{
	public:
	CurlWrapper()
	{
		CURLcode curlResult;
		curlResult = curl_global_init(CURL_GLOBAL_ALL);
		if (curlResult != CURLE_OK)
		{
			std::cerr << curl_easy_strerror(curlResult) << std::endl;
		}
	}

	~CurlWrapper()
	{
		curl_global_cleanup();
	}
};

CurlWrapper w;


bool performSql(const QString& message, QSqlQuery& query, const QString& sql)
{
	bool ok;
	std::clog << QString("[sql] %1 ... ").arg(message);
	if (sql.isEmpty())
		ok = query.exec();
	else
		ok = query.exec(sql);

	if (ok)
	{
		std::clog << "ok";
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

			std::clog << QString("(return %1 records)").arg(count);
		}
		else
			std::clog << QString("(affected %1 records)").arg(query.numRowsAffected());
	}
	else
	{
		std::clog << "Fail.";
		std::cerr << "SQL: " << query.lastQuery() << "."
				  << "Reason: " << query.lastError().text();
	}
	std::clog << std::endl;
	if (!ok)
		throw 1;
	return ok;
}

bool performSql(const QString& message, QSqlQuery& query, const QVariantMap& binds = QVariantMap())
{
	for(QString param: binds.keys())
	{
		query.bindValue(param, binds[param]);
	}
	return performSql(message, query, QString());
}

bool performTradeRequest(const QString& message, BtcTradeApi& req)
{
	bool ok = true;
	std::clog << "[http]" << qPrintable(message) << " ... ";
	ok = req.performQuery();
	if (!ok)
	{
		std::clog << "Fail.";
		std::cerr << "Failed method: " << qPrintable(req.methodName());
	}
	else
	{
		ok = req.isSuccess();
		if (!ok)
		{
			std::clog << "Fail.";
			std::cerr << "Non success result:"  << qPrintable(req.error());
		}
	}

	if (ok)
		std::clog << "ok";

	std::clog << std::endl;

	if (!ok)
		throw 1;
	return ok;
}

int main(int argc, char *argv[])
{
	(void) argc;
	(void) argv;

	std::clog << "connecting to database ... ";
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "trader_db");
	db.setDatabaseName("trader.db");
	if (!db.open())
	{
		std::clog << " FAIL. " << qPrintable(db.lastError().text()) << std::endl;
	}
	else
		std::clog << " ok" << std::endl;

	std::clog << "Create tables ..." << std::endl;
	QSqlQuery sql(db);
	QString createSettingsSql = "CREATE TABLE IF NOT EXISTS `settings`("
			 "`id` INTEGER PRIMARY KEY, "
			 "`profit` decimal(6,4) NOT NULL DEFAULT '0.0100',"
			 "`comission` decimal(6,4) NOT NULL DEFAULT '0.0020', "
			 "`first_step` decimal(6,4) NOT NULL DEFAULT '0.0500',"
			 "`martingale` decimal(6,4) NOT NULL DEFAULT '0.0500',"
			 "`dep` decimal(10,4) NOT NULL DEFAULT '100.0000',"
			 "`coverage` decimal(6,4) NOT NULL DEFAULT '0.1500',"
			 "`count` int(11) NOT NULL DEFAULT '10', "
			"currency char(3) not null default 'usd',"
			"goods char(3) not null default 'btc',"
			"secret_id integer not null references secrets(id)"
		   ")";
	QString createOrdersSql = "CREATE TABLE IF NOT EXISTS `orders`  ("
				"`order_id` INTEGER PRIMARY KEY,"
				"`status` int(11) NOT NULL DEFAULT '0',"
				"`type` char(4) NOT NULL DEFAULT 'buy',"
				"`amount` decimal(11,6) DEFAULT NULL,"
				"`start_amount` decimal(11,6) DEFAULT NULL,"
				"`rate` decimal(11,6) DEFAULT NULL,"
				"`settings_id` int(11) DEFAULT NULL,"
				"backed_up INTEGER NOT NULL DEFAULT 0"
			  ") ";

	QString createSecretsSql = "CREATE TABLE IF NOT EXISTS secrets ("
			"apikey char(256) not null,"
			"secret char(256) not null,"
			"id integer primary key,"
			"is_crypted BOOLEAN not null default FALSE"
			")";

	bool all_tables_created = true;
	std::clog << "\tsettings table : ";
	all_tables_created &= sql.exec(createSettingsSql);
	if (!all_tables_created)
		std::clog << "Fail. " << qPrintable(sql.lastError().text()) << std::endl;
	else
		std::clog << "ok" << std::endl;

	std::clog << "\torders table : ";
	all_tables_created &= sql.exec(createOrdersSql);
	if (!all_tables_created)
		std::clog << "Fail. " << qPrintable(sql.lastError().text()) << std::endl;
	else
		std::clog << "ok" << std::endl;

	std::clog << "\tsecrets table : ";
	all_tables_created &= sql.exec(createSecretsSql);
	if (!all_tables_created)
		std::clog << "Fail. " << qPrintable(sql.lastError().text()) << std::endl;
	else
		std::clog << "ok" << std::endl;

	if (!all_tables_created)
		std::clog << "tables creation fail" << std::endl;
	else
		std::clog << "all tables created" << std::endl;

	QSqlQuery insertOrder(db);
	QSqlQuery updateActiveOrder(db);
	QSqlQuery updateSetCanceled(db);
	QSqlQuery finishRound(db);
	QSqlQuery finishBuyRound(db);
	QSqlQuery selectSellOrder(db);
	QSqlQuery deleteSellOrder(db);
	QSqlQuery selectSettings (db);
	QSqlQuery selectCurrentRoundActiveOrdersCount(db);
	QSqlQuery selectCurrentRoundGain(db);
	QSqlQuery checkMaxBuyRate(db);
	QSqlQuery selectOrdersWithChangedStatus(db);
	QSqlQuery selectOrdersFromPrevRound(db);

	std::clog << "prepare sql statements ... ";
	try
	{
		if (!insertOrder.prepare( "INSERT INTO orders (order_id, status, type, amount, start_amount, rate, settings_id) values (:order_id, :status, :type, :amount, :start_amount, :rate, :settings_id)"))
			throw insertOrder;

		if (!updateActiveOrder.prepare("UPDATE orders set status=0, amount=:amount, rate=:rate where order_id=:order_id"))
			throw updateActiveOrder;

		if (!updateSetCanceled.prepare("UPDATE orders set status=:status, amount=:amount, start_amount=:start_amount, rate=:rate where order_id=:order_id"))
			throw updateSetCanceled;

		if (!finishRound.prepare("update orders set backed_up=1 where settings_id=:settings_id"))
			throw finishRound;

		if (!finishBuyRound.prepare("update orders set backed_up=1 where type='buy' and settings_id=:settings_id"))
			throw finishBuyRound;

		if (!selectSellOrder.prepare("SELECT order_id, start_amount from orders where backed_up=0 and settings_id=:settings_id and type='sell'"))
			throw selectSellOrder;

		if (!deleteSellOrder.prepare("DELETE from orders where backed_up=0 and settings_id=:settings_id and type='sell'"))
			throw deleteSellOrder;

		if (!selectSettings.prepare("SELECT id, comission, first_step, martingale, dep, coverage, count, currency, goods, secret_id from settings"))
			throw selectSettings;

		if (!selectCurrentRoundActiveOrdersCount.prepare("SELECT count(*) from orders where status=0 and settings_id=:settings_id and backed_up=0"))
			throw selectCurrentRoundActiveOrdersCount;

		if (!selectCurrentRoundGain.prepare("select o.settings_id, sum(o.start_amount - o.amount)*(1-s.comission) as amount, sum((o.start_amount - o.amount) * o.rate) as payment, sum((o.start_amount - o.amount) * o.rate) / sum(o.start_amount - o.amount)/(1-s.comission)/(1-s.comission)*(1+s.profit) as sell_rate from orders o left join settings s  on s.id = o.settings_id where o.type='buy' and backed_up=0 and o.settings_id=:settings_id"))
			throw selectCurrentRoundGain;

		if (!checkMaxBuyRate.prepare("select max(o.rate) * (1+s.first_step) / (1-s.first_step) from orders o left join settings s on s.id = o.settings_id where o.backed_up=0 and o.status=0 and type='buy' and o.settings_id=:settings_id"))
			throw checkMaxBuyRate;

		if (!selectOrdersWithChangedStatus.prepare("SELECT order_id from orders where status=-1 and settings_id=:settings_id"))
			throw selectOrdersWithChangedStatus;

		if (!selectOrdersFromPrevRound.prepare("SELECT order_id from orders where backed_up=1 and status<1 and settings_id=:settings_id"))
			throw selectOrdersFromPrevRound;

		std::clog << "ok" << std::endl;
	}
	catch (QSqlQuery& e)
	{
		std::clog << " FAIL. query: " << e.lastQuery() << ". " << "Reason: " << e.lastError().text() << std::endl;
	}

	std::clog << "Initialize key storage ... ";
	SqlKeyStorage storage(db, "secrets");
	storage.setPassword("g00dd1e#wer4");
	std::clog << "ok" << std::endl;

	QMap<int, Funds> funds;
	BtcPublicInfo pinfo;
	BtcPublicTicker pticker;
	BtcPublicDepth pdepth(20);

	std::clog << "get currencies info ...";
	if (!pinfo.performQuery())
		std::clog << "fail" << std::endl;
	else
		std::clog << "ok" << std::endl;


	/// Settings
	int settings_id = 1;

	double dep = 3.00;
	double first_step = 0.01;
	double martingale = 0.05;
	double coverage = 0.15;
	double comission = 0.002;
	int n = 8;
	int secret_id = 0;
	QString currency = "usd";
	QString goods = "ppc";

	QElapsedTimer timer;
	timer.start();

	std::clog << "start main cycle" << std::endl;
	while (1)
	{
		timer.restart();
		std::clog << std::endl;

		std::clog << "update currencies rate info ...";
		if (!pticker.performQuery())
			std::clog << "fail" << std::endl;
		else
			std::clog << "ok" << std::endl;

		std::clog << "update orders depth ...";
		if (!pdepth.performQuery())
			std::clog << "fail" << std::endl;
		else
			std::clog << "ok" << std::endl;

		performSql("Mark all active orders as unknown status", sql, "UPDATE orders set status=-1 where status=0");

		for(int id: storage.allKeys())
		{
			std::clog << "for keypair "  << id << " mark active orders status: -1 --> 0" << std::endl;
			storage.setCurrent(id);

			Info info(storage, funds[id]);
			performTradeRequest(QString("get funds info for keypair %1").arg(id), info);

			ActiveOrders activeOrders(storage);
			if (performTradeRequest(QString("get active orders for keypair %1").arg(id),activeOrders))
			{
				for (Order& order: activeOrders.orders)
				{
					QVariantMap params;
					params[":order_id"] = order.order_id;
					params[":amount"]  = order.amount;
					params[":rate"] = order.rate;
					performSql(QString("Order %1 still active, mark it").arg(order.order_id), updateActiveOrder, params);
				}
			}

			QString pname;
			pname = "btc_usd";

			auto seller = [](const QString& pname, double& goods, double& currency)
			{
				Pair& p = Pairs::ref(pname);
				double gain = 0;
				double sold = 0;
//				qDebug() << QString("we have %1 %2").arg(goods).arg(p.goods());
				for(auto d: p.depth.bids)
				{
					if (goods < 0.0000001)
						break;
					double amount = d.amount;
					double rate = d.rate;
					double trade_amount = qMin(goods, amount);
					gain += rate * trade_amount * (1-p.fee/100);
					goods -= trade_amount;
					sold += trade_amount;
				}

				currency += gain;

//				qDebug() << QString("we can sell %1 %3, and get %2 %4").arg(sold).arg(gain).arg(p.goods()).arg(p.currency());
			};

			auto buyer = [](const QString& pname, double& goods, double& currency)
			{
				Pair& p = Pairs::ref(pname);
				double bought = 0;
				double spent = 0;
//				qDebug() << QString("we have %1 %2").arg(currency).arg(p.currency());
				for(auto d: p.depth.asks)
				{
					if (currency < 0.000001)
						break;
					double amount = d.amount;
					double rate = d.rate;
					double price = qMin(currency, amount*rate);
					bought += ((price / rate) * (1-p.fee/100));
					currency -= price;
					spent += price;
				}

				goods += bought;

//				qDebug() << QString("we can spend %1 %3, and get %2 %4").arg(spent).arg(bought).arg(p.currency()).arg(p.goods());
			};

			double btc = funds[id]["btc"] / 10;
			double start_btc = btc;
			double usd = 0;
			double eur = 0;
			double ltc = 0;

			seller("btc_usd", btc, usd);
			buyer("eur_usd", eur, usd);
			buyer("btc_eur", btc, eur);

			if (btc - start_btc > 0)
				std::cout << QString("btc -> usd -> eur -> btc : %1").arg(btc - start_btc) << std::endl;

			btc = start_btc;
			usd = 0;
			eur = 0;

			seller("btc_eur", btc, eur);
			seller("eur_usd", eur, usd);
			buyer("btc_usd", btc, usd);

			if (btc - start_btc > 0)
				std::cout << QString("btc -> eur -> usd -> btc : %1").arg(btc - start_btc) << std::endl;

			btc = start_btc;
			usd = 0;
			ltc = 0;

			buyer("ltc_btc", ltc, btc);
			seller("ltc_usd", ltc, usd);
			buyer("btc_usd", btc, usd);
			if (btc - start_btc > 0)
				std::cout << QString("btc -> ltc -> usd -> btc : %1").arg(btc - start_btc) << std::endl;

			btc = start_btc;
			usd = 0;
			ltc = 0;

			seller("btc_usd", btc, usd);
			buyer("ltc_usd", ltc, usd);
			seller("ltc_btc", ltc, btc);
			if (btc - start_btc > 0)
				std::cout << QString("btc -> usd -> ltc -> btc : %1").arg(btc - start_btc) << std::endl;
		}

		if (!performSql("get settings list", selectSettings))
		{
			std::clog << "Sleep for 10 seconds" << std::endl;
			usleep(1000 * 1000 * 10);
			continue;
		}

		while (selectSettings.next())
		{
			settings_id = selectSettings.value(0).toInt();
			comission = selectSettings.value(1).toDouble();
			first_step = selectSettings.value(2).toDouble();
			martingale = selectSettings.value(3).toDouble();
			dep = selectSettings.value(4).toDouble();
			coverage = selectSettings.value(5).toDouble();
			n = selectSettings.value(6).toInt();
			currency = selectSettings.value(7).toString();
			goods = selectSettings.value(8).toString();
			secret_id = selectSettings.value(9).toInt();

			QString pairName = QString("%1_%2").arg(goods, currency);

			std::clog << QString("Processing settings_id %1. Pair: %2").arg(settings_id).arg(pairName) << std::endl;
			if (!Pairs::ref().contains(pairName))
				std::cerr << "no pair" << pairName << "available" <<std::endl;

			storage.setCurrent(secret_id);
			Pair& pair = Pairs::ref(pairName);

			bool round_in_progress = false;


			QVariantMap param;
			QVariantMap insertOrderParam;

			param[":settings_id"] = settings_id;
			if (performSql("get active orders count", selectCurrentRoundActiveOrdersCount, param))
				if (selectCurrentRoundActiveOrdersCount.next())
				{
					int count = selectCurrentRoundActiveOrdersCount.value(0).toInt();
					round_in_progress = count > 0;
					std::clog << QString("active orders count: %1").arg(count)  << std::endl;
				}

			if (performSql("check if any orders have status changed", selectOrdersWithChangedStatus, param))
				while (selectOrdersWithChangedStatus.next())
				{
					Order::Id order_id = selectOrdersWithChangedStatus.value(0).toInt();
					OrderInfo info(storage, order_id);
					performTradeRequest(QString("get info for order %1").arg(order_id), info);

					std::clog << QString("order %1 changed status to %2").arg(order_id).arg(info.order.status) << std::endl;
					QVariantMap upd_param;
					upd_param[":order_id"] = order_id;
					upd_param[":status"] = info.order.status;
					upd_param[":amount"] = info.order.amount;
					upd_param[":start_amount"] = info.order.start_amount;
					upd_param[":rate"] = info.order.rate;

					performSql(QString("update order %1").arg(order_id), updateSetCanceled, param);

					if (info.order.type == Order::Sell)
					{
						std::clog << QString("sell order changed status to %1").arg(info.order.status) << std::endl;
						performSql(QString("Finish round"), finishRound, param);
						round_in_progress = false;
					}
				}

			double amount_gain = 0;
			double sell_rate = 0;
			performSql("Get current round amoumt gain", selectCurrentRoundGain, param);
			if(selectCurrentRoundGain.next())
			{
				//int settings_id = selectCurrentRoundGain.value(0).toInt();
				amount_gain = selectCurrentRoundGain.value(1).toDouble();
				sell_rate = selectCurrentRoundGain.value(3).toDouble();

				std::clog << QString("in current round we got %1 %2.")
							 .arg(amount_gain).arg(pair.goods())
						  << std::endl;
			}

			performSql("Get maximum buy rate", checkMaxBuyRate, param);
			if (checkMaxBuyRate.next())
			{
				double rate = checkMaxBuyRate.value(0).toDouble();
				std::clog << QString("max buy rate is %1").arg(rate) << std::endl;
				if (rate > 0 && pair.ticker.last > rate && amount_gain == 0)
				{
					std::clog << QString("rate for %1 too high (%2)").arg(pair.name).arg(rate) << std::endl;
					performSql("Finish buy orders for round", finishBuyRound, param);
					round_in_progress = false;
				}
			}

			if (!round_in_progress)
			{
				performSql("check for orders left from previous round", selectOrdersFromPrevRound, param);
				while(selectOrdersFromPrevRound.next())
				{
					Order::Id order_id = selectOrdersFromPrevRound.value(0).toInt();
					CancelOrder cancel(storage, funds[secret_id], order_id);
					performTradeRequest(QString("cancel order %1").arg(order_id), cancel);
				}

				std::cout << "New round start. Calculate new buy orders parameters" << std::endl;
				double sum =0;
				for (int j=0; j<n; j++)
				{
					sum += qPow(1+martingale, j) * ( 1 - first_step - (coverage - first_step) * j/(n-1));
				}

				double execute_rate = pair.ticker.last;
				double u = qMax(qMin(funds[secret_id][currency], dep) / execute_rate / sum, pair.min_amount / (1-comission));
				double total_currency_spent = 0;

				for(int j=0; j<n; j++)
				{
					double amount = u * qPow(1+martingale, j);
					double rate = execute_rate * ( 1 - first_step - (coverage - first_step) * j/(n-1));

					if (amount * rate > funds[secret_id][currency] ||
						total_currency_spent + amount*rate > dep+0.00001)
					{
						std::clog << QString("Not enought %1 for full bids").arg(pair.currency()) << std::endl;
						break;
					}
					Trade trade(storage, funds[secret_id], pair.name, Order::Buy, rate, amount);
					if (performTradeRequest("create buy order", trade))
					{
						insertOrderParam[":order_id"] = trade.order_id;
						insertOrderParam[":status"] = 0;
						insertOrderParam[":type"] = "buy";
						insertOrderParam[":amount"] = trade.remains;
						insertOrderParam[":start_amount"] = trade.received + trade.remains;
						insertOrderParam[":rate"] = QString::number(rate, 'f', pair.decimal_places);
						insertOrderParam[":settings_id"] = settings_id;
						performSql("insert buy order record", insertOrder, insertOrderParam);

						total_currency_spent += amount * rate;
						std::clog << QString("%1 bid: %2@%3").arg(j+1).arg(amount).arg(rate) << std::endl;
					}
				}

				round_in_progress = true;
				std::clog << QString("total bid: %1 %2").arg(total_currency_spent).arg(pair.currency())
						  << std::endl;
			}
			else
			{
			//	std::cout << qPrintable(QString("round for %1(%2) already in progress").arg(settings_id).arg(pair.name)) << std::endl;
			}

			if (round_in_progress)
			{
				if (pair.min_amount > amount_gain)
				{
					std::clog << "An amount we have is less then minimal trade amount. skip creating sell order" << std::endl;
					continue;
				}

				bool need_recreate_sell = true;
				Order::Id sell_order_id = 0;
				performSql("get sell order id and amount", selectSellOrder, param);
				if (selectSellOrder.next())
				{
					double sell_order_amount = selectSellOrder.value(1).toDouble();
					sell_order_id = selectSellOrder.value(0).toInt();
					need_recreate_sell = qAbs(sell_order_amount - amount_gain) > 0.000001;
					std::clog << QString("found sell order %1 for %2 amount. Need recreate sell order: %3")
								 .arg(sell_order_id).arg(sell_order_amount).arg(need_recreate_sell?"yes":"no")
							  << std::endl;
				}


				if (need_recreate_sell)
				{
					if (sell_order_id > 0)
					{
						CancelOrder cancel(storage, funds[secret_id], selectSellOrder.value(0).toInt());
						if (performTradeRequest("cancel order", cancel))
						{
							performSql("delete sell order record", deleteSellOrder, param);
						}
					}

					Trade sell(storage, funds[secret_id], pair.name, Order::Sell, sell_rate, amount_gain);
					if (performTradeRequest("insert sell record into db", sell))
					{
						insertOrderParam[":order_id"] = sell.order_id;
						insertOrderParam[":status"] = 0;
						insertOrderParam[":type"] = "sell";
						insertOrderParam[":amount"] = sell.remains;
						insertOrderParam[":start_amount"] = sell.received + sell.remains;
						insertOrderParam[":rate"] = sell_rate;
						insertOrderParam[":settings_id"] = settings_id;
						performSql("insert sell order record", insertOrder, insertOrderParam);
					}
				}
			}
		}

		quint64 t = timer.elapsed();
		std::clog << QString("iteration done in %1 ms").arg(t) << std::endl;
		quint64 ms_sleep = 10 * 1000;
		if (t < ms_sleep)
			usleep(1000 * (ms_sleep-t));
	}
	return 0;
}
