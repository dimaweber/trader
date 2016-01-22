#include <QByteArray>
#include <QString>
#include <QDebug>
#include <QUrlQuery>
#include <QDateTime>
#include <QJsonDocument>
#include <QFile>
#include <QDataStream>
#include <QtMath>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <iostream>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>

#include <curl/curl.h>

#include <readline/readline.h>

#include <unistd.h>

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
			std::cout << qPrintable(pairName) << ' ' << value(pairName) << std::endl;
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
		std::cout	<< qPrintable(QString("%1    : %2").arg("name").arg(name))  << std::endl
					<< qPrintable(QString("   %1 : %2").arg("decimal_places").arg(decimal_places)) << std::endl
					<< qPrintable(QString("   %1 : %2").arg("min_price").arg(min_price)) << std::endl
					<< qPrintable(QString("   %1 : %2").arg("max_price").arg(max_price)) << std::endl
					<< qPrintable(QString("   %1 : %2").arg("min_amount").arg(min_amount)) << std::endl
					<< qPrintable(QString("   %1 : %2").arg("hidden").arg(hidden)) << std::endl
					<< qPrintable(QString("   %1 : %2").arg("fee").arg(fee)) << std::endl;
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

class KeyStorage
{
	QByteArray _apiKey;
	QByteArray _secret;
	QString _fileName;
	QByteArray _hashPwd;

protected:
	QString fileName() const { return _fileName;}
	QByteArray getPassword(bool needConfirmation = false);
	void read_input(const QString& prompt, QByteArray& ba) const;

	static void encrypt(QByteArray& data, const QByteArray& password, QByteArray& ivec);
	static void decrypt(QByteArray& data, const QByteArray& password, QByteArray& ivec);

	virtual void load();
	virtual void store();
public:
	KeyStorage(const QString& fileName) : _fileName(fileName){}

	const QByteArray& apiKey() { if (_apiKey.isEmpty()) load(); return _apiKey;}
	const QByteArray& secret() { if (_secret.isEmpty()) load(); return _secret;}

	void changePassword();
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

	double money_spent() const { return amount * rate;}
	double money_gain() const {return ((type==Sell)?(start_amount-amount)*rate:0) * (1 - get_comission_pct(pair));}
	double amount_gain() const {return ((type==Sell)?0:(start_amount - amount)) * (1 - get_comission_pct(pair));}
	double comission() const { return ((type==Sell)?money_spent():amount) * get_comission_pct(pair);}
};

class MissingField : public std::runtime_error
{public : MissingField(const QString& msg): std::runtime_error(msg.toUtf8()){}};

class BrokenJson : public std::runtime_error
{public : BrokenJson(const QString& msg): std::runtime_error(msg.toUtf8()){}};

class HttpError : public std::runtime_error
{public : HttpError(const QString& msg): std::runtime_error(msg.toUtf8()){}};

class KeyStorage;

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
	virtual QString methodName() const = 0;

public:
	BtcTradeApi(KeyStorage& storage)
		: HttpQuery(), storage(storage),
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
			std::cout << qPrintable(QString("%1 : %2").arg(currency).arg(QString::number(funds[currency], 'f')));
	std::cout << qPrintable(QString("transactins: %1").arg(transaction_count));
	std::cout << qPrintable(QString("open orders : %1").arg(open_orders_count));
	std::cout << qPrintable(QString("serverTime : %1").arg(server_time.toString()));
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

		QByteArray sign = hmac_sha512(params, storage.secret());
		jsonData.clear();

		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &jsonData);
		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, BtcTradeApi::writeFunc);

		QByteArray sUrl = path().toUtf8();
		curl_easy_setopt(curlHandle, CURLOPT_URL, sUrl.constData());
		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, params.constData());

		headers = curl_slist_append(headers, QString("Key: %1").arg(storage.apiKey().constData()).toUtf8().constData());
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
			  << qPrintable(QString("   status : %1").arg(sStatus))
			  << qPrintable(QString("   pair   : %1").arg(pair))
			  << qPrintable(QString("   type   : %1").arg((type==Sell)?"sell":"buy"))
			  << qPrintable(QString("   amount : %1 (%2)").arg(amount).arg(start_amount))
			  << qPrintable(QString("   rate   : %1").arg(rate))
			  << qPrintable(QString("   created: %1").arg(timestamp_created.toString()));
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
	std::cout << qPrintable(QString("%1 : %2").arg("high").arg(high))
			  << qPrintable(QString("%1 : %2").arg("low").arg(low))
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

		curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &jsonData);
		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, BtcTradeApi::writeFunc);

		QByteArray sUrl = path().toUtf8();
		curl_easy_setopt(curlHandle, CURLOPT_URL, sUrl.constData());

//		qDebug() << "perform query" << sUrl.constData();

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

		QByteArray hash(SHA_DIGEST_LENGTH, Qt::Uninitialized);
		SHA1(reinterpret_cast<const unsigned char *>(pass.constData()),
			 pass.length(),reinterpret_cast<unsigned char*>(hash.data()));

		_hashPwd = hash;
	}
	return _hashPwd;
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

class CurlWrapper
{
	public:
	CurlWrapper()
	{
		CURLcode curlResult;
		curlResult = curl_global_init(CURL_GLOBAL_ALL);
		if (curlResult != CURLE_OK)
		{
			qWarning() << curl_easy_strerror(curlResult);
		}
	}

	~CurlWrapper()
	{
		curl_global_cleanup();
	}
};

CurlWrapper w;

int main(int argc, char *argv[])
{
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "trader_db");
	db.setDatabaseName("trader.db");
	if (!db.open())
		qWarning() << "fail to open database connection:" << db.lastError().text();

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
			"goods char(3) not null default 'btc'"
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
	if (!sql.exec(createSettingsSql))
		qDebug() << "Fail to create settings table:" << sql.lastError().text();

	if (!sql.exec(createOrdersSql))
		qDebug() << "Fail to create orders table" << sql.lastError().text();

	QSqlQuery insertOrder(db);
	if (!insertOrder.prepare( "INSERT INTO orders (order_id, status, type, amount, start_amount, rate, settings_id) values (:order_id, :status, :type, :amount, :start_amount, :rate, :settings_id)"))
		qWarning() << "Fail to prepare orders insert statement: " << insertOrder.lastError().text();

	QSqlQuery updateActiveOrder(db);
	if (!updateActiveOrder.prepare("UPDATE orders set status=0, amount=:amount, rate=:rate where order_id=:order_id"))
		qWarning() << "Fail to prepare orders update active statement: " << updateActiveOrder.lastError().text();

	QSqlQuery updateSetCanceled(db);
	if (!updateSetCanceled.prepare("UPDATE orders set status=:status, amount=:amount, start_amount=:start_amount, rate=:rate where order_id=:order_id"))
		qWarning() << "Fail to prepare orders update statement: " << updateSetCanceled.lastError().text();

	QSqlQuery finishRound(db);
	if (!finishRound.prepare("update orders set backed_up=1 where settings_id=:settings_id"))
		qWarning() << "Fail to prepare finish round statement: " << finishRound.lastError().text();

	QSqlQuery selectSellOrder(db);
	if (!selectSellOrder.prepare("SELECT order_id, start_amount from orders where backed_up=0 and settings_id=:settings_id and type='sell'"))
		qWarning() << "Fail to prepare select sell order statement: " << selectSellOrder.lastError().text();

	QSqlQuery deleteSellOrder(db);
	if (!deleteSellOrder.prepare("DELETE from orders where backed_up=0 and settings_id=:settings_id and type='sell'"))
		qWarning() << "Fail to prepare delete sell order statement: " << deleteSellOrder.lastError().text();

	QSqlQuery selectSettings (db);
	if (!selectSettings.prepare("SELECT id, comission, first_step, martingale, dep, coverage, count, currency, goods from settings"))
		qWarning() << "Fail to prepare select settings statement:" << selectSettings.lastError().text();

	QSqlQuery selectCurrentRound(db);
	if (!selectCurrentRound.prepare("SELECT count(*) from orders where status=0 and settings_id=:settings_id and backed_up=0"))
		qWarning() << "Fail to prepare select current round statement:" << selectCurrentRound.lastError().text();

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

	Info info(storage, funds);
	info.performQuery();

	/// Settings
	int settings_id = 1;

	double dep = 300;
	double first_step = 0.01;
	double martingale = 0.05;
	double coverage = 0.15;
	double comission = 0.002;
	int n = 28;
	QString currency = "usd";
	QString goods = "ppc";

	while (1)
	{
		pticker.performQuery();

		ActiveOrders activeOrders(storage);
		activeOrders.performQuery();

		QString sqlQuery = QString("UPDATE orders set status=-1 where status=0");
		if (!sql.exec(sqlQuery))
			qWarning() << sql.lastQuery() << sql.lastError().text();

		for (Order& order: activeOrders.orders)
		{
			updateActiveOrder.bindValue(":order_id", order.order_id);
			updateActiveOrder.bindValue(":amount", order.amount);
			updateActiveOrder.bindValue(":rate", order.rate);
			if (!updateActiveOrder.exec())
				qWarning() << updateActiveOrder.lastQuery() << updateActiveOrder.lastError().text();
		}

		if (!selectSettings.exec())
		{
			qWarning() << "Fail to retrieve settings: " << selectSettings.lastError().text();
			usleep(1000 * 10);
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
			QString pairName = QString("%1_%2").arg(goods, currency);
			if (!Pairs::ref().contains(pairName))
				qWarning() << "no pair" << pairName << "available";

			Pair& pair = Pairs::ref(pairName);

			bool round_in_progress = false;

			selectCurrentRound.bindValue(":settings_id", settings_id);
			if (!selectCurrentRound.exec())
				qWarning() << selectCurrentRound.lastQuery() << selectCurrentRound.lastError().text();
			else
				if (selectCurrentRound.next())
				{
					round_in_progress = selectCurrentRound.value(0).toInt() > 0;
				}

			if (!round_in_progress)
			{
				std::cout << "new round start" << std::endl;
				double sum =0;
				for (int j=0; j<n; j++)
				{
					sum += qPow(1+martingale, j) * ( 1 - first_step - (coverage - first_step) * j/(n-1));
				}

				double execute_rate = pair.ticker.last;
				double u = qMax(qMin(funds[currency], dep) / execute_rate / sum, pair.min_amount / (1-comission));
				double total_currency_spent = 0;

				for(int j=0; j<n; j++)
				{
					double amount = u * qPow(1+martingale, j);
					double rate = execute_rate * ( 1 - first_step - (coverage - first_step) * j/(n-1));

					if (amount * rate > funds[currency])
					{
						qWarning() << "not enought currency for bids";
						break;
					}
					if (total_currency_spent + amount*rate > dep+0.00001)
					{
						std::cout << "not enought usd for full bids" << std::endl;
						break;
					}
					Trade trade(storage, funds, pair.name, Order::Buy, rate, amount);
					trade.performQuery();
					if (trade.isSuccess())
					{
						insertOrder.bindValue(":order_id", trade.order_id);
						insertOrder.bindValue(":status", 0);
						insertOrder.bindValue(":type", "buy");
						insertOrder.bindValue(":amount", trade.remains);
						insertOrder.bindValue(":start_amount", trade.received);
						insertOrder.bindValue(":rate", QString::number(rate, 'f', pair.decimal_places));
						insertOrder.bindValue(":settings_id", settings_id);
						if (!insertOrder.exec())
							qWarning() << "can't insert order record to records: " << insertOrder.lastError().text();

						total_currency_spent += amount * rate;
						std::cout << j+1 << " bid " <<  amount << '@'  << rate << std::endl;
					}
					else
					{
						trade.display();
						break;
					}
				}

				round_in_progress = true;
				std::cout << "total bid: " << total_currency_spent << ' ' << qPrintable(currency) << std::endl;
			}
			else
				std::cout << qPrintable(QString("round for %1(%2) already in progress").arg(settings_id).arg(pair.name)) << std::endl;

			sqlQuery = QString("SELECT order_id, settings_id from orders where status=-1");
			if (!sql.exec(sqlQuery))
				qWarning() << sql.lastQuery() << sql.lastError().text();
			else
				while (sql.next())
				{
					Order::Id order_id = sql.value(0).toInt();
					OrderInfo info(storage, order_id);
					if (!info.performQuery() && info.isSuccess())
					{
						qWarning() << "fail to retrieve info for order: " << order_id;
						info.display();
					}


					if (info.order.type == Order::Buy)
					{
						updateSetCanceled.bindValue(":order_id", order_id);
						updateSetCanceled.bindValue(":status", info.order.status);
						updateSetCanceled.bindValue(":amount", info.order.amount);
						updateSetCanceled.bindValue(":start_amount", info.order.start_amount);
						updateSetCanceled.bindValue(":rate", info.order.rate);

						if (!updateSetCanceled.exec())
							qWarning() << updateSetCanceled.lastError().text();
					}
					if (info.order.type == Order::Sell)
					{
						finishRound.bindValue(":settings_id", sql.value(1).toInt());
						finishRound.exec();
						round_in_progress = false;
						qDebug() << "round done";
					}
				}

			if (round_in_progress)
			{
				sqlQuery = QString("select o.settings_id, sum(o.start_amount - o.amount)*(1-s.comission) as amount, sum((o.start_amount - o.amount) * o.rate) as payment, sum((o.start_amount - o.amount) * o.rate) / sum(o.start_amount - o.amount)/(1-s.comission)/(1-s.comission)*(1+s.profit) as sell_rate from orders o left join settings s  on s.id = o.settings_id where o.type='buy'  group by o.settings_id");
				if (!sql.exec(sqlQuery))
					qWarning() << sql.lastQuery() << sql.lastError().text();
				else
				{
					while(sql.next())
					{
						int settings_id = sql.value(0).toInt();
						double amount_gain = sql.value(1).toDouble() - 0.000001;
						double sell_rate = sql.value(3).toDouble();

						if (pair.min_amount > amount_gain)
						{
							qDebug() << "not enought amount for trade. Wait for more";
							continue;
						}

						bool need_recreate_sell = true;
						Order::Id sell_order_id = 0;
						selectSellOrder.bindValue(":settings_id", settings_id);
						if (!selectSellOrder.exec())
							qWarning() << "can't select sell order for settings_id" << selectSellOrder.lastError().text();
						else
						{
							if (selectSellOrder.next())
							{
								double sell_order_amount = selectSellOrder.value(1).toDouble();
								sell_order_id = selectSellOrder.value(0).toInt();
								need_recreate_sell = qAbs(sell_order_amount - amount_gain) > 0.000001;
							}
						}

						if (need_recreate_sell)
						{
							if (sell_order_id > 0)
							{
								CancelOrder cancel(storage, funds, selectSellOrder.value(0).toInt());
								if (cancel.performQuery() && cancel.isSuccess())
								{
									deleteSellOrder.bindValue(":settings_id", settings_id);
									if (!deleteSellOrder.exec())
										qWarning() << "can't delete sell order: " << deleteSellOrder.lastError().text();
								}
							}

							Trade sell(storage, funds, pair.name, Order::Sell, sell_rate, amount_gain);
							if (sell.performQuery() && sell.isSuccess())
							{
								insertOrder.bindValue(":order_id", sell.order_id);
								insertOrder.bindValue(":status", 0);
								insertOrder.bindValue(":type", "sell");
								insertOrder.bindValue(":amount", sell.remains);
								insertOrder.bindValue(":start_amount", sell.received + sell.remains);
								insertOrder.bindValue(":rate", sell_rate);
								insertOrder.bindValue(":settings_id", settings_id);
								if (!insertOrder.exec())
									qWarning() << "can't insert sell order record: " << insertOrder.lastError().text();
							}
							else
								sell.display();
						}
					}
				}
			}
		}
		usleep(1000 * 3000);
	}
	return 0;
}
