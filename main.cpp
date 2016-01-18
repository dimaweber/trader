#include <QByteArray>
#include <QString>
#include <QDebug>
#include <QUrlQuery>
#include <QDateTime>
#include <QJsonDocument>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <curl/curl.h>

typedef const EVP_MD* (*HashFunction)();

double get_comission_pct(const QString& pair)
{
	return 0.002;
}

QByteArray hmac_sha512(QByteArray& message, QByteArray& key, HashFunction func = EVP_sha512)
{
	unsigned char* digest = nullptr;
	unsigned int digest_size = 0;

	digest = HMAC(func(), reinterpret_cast<unsigned char*>(key.data()), key.length(),
						  reinterpret_cast<unsigned char*>(message.data()), message.length(),
						  digest, &digest_size);

	return QByteArray(reinterpret_cast<char*>(digest), digest_size);
}

typedef QMap<QString, double> Funds;
Funds parseFunds(QVariantMap fundsMap)
{
	Funds funds;
	for (QString currencyName: fundsMap.keys())
	{
		double v = fundsMap[currencyName].toDouble();
		funds[currencyName] = v;
	}

	return funds;
}

double read_double(const QVariantMap& map, const QString& name);
QString read_string(const QVariantMap& map, const QString& name);
long read_long(const QVariantMap& map, const QString& name);
QVariantMap read_map(const QVariantMap& map, const QString& name);
QDateTime read_timestamp(const QVariantMap& map, const QString& name);

class MissingField : public std::runtime_error
{public : MissingField(const QString& msg): std::runtime_error(msg.toUtf8()){}};

class BrokenJson : public std::runtime_error
{public : BrokenJson(const QString& msg): std::runtime_error(msg.toUtf8()){}};

class HttpError : public std::runtime_error
{public : HttpError(const QString& msg): std::runtime_error(msg.toUtf8()){}};

class BtcTradeQuery
{
	static size_t writeFunc(char *ptr, size_t size, size_t nmemb, void *userdata);
	QByteArray jsonData;

	static quint32 _nonce;
	static QString nonce() { return QString::number(++_nonce);}
protected:

	bool valid;

	bool success;
	QString errorMsg;

	QUrlQuery query;
	QByteArray queryParams();

	bool parse(QByteArray serverAnswer);

	virtual QMap<QString, QString> extraQueryParams();
	virtual void showSuccess() const = 0;
	virtual bool parseSuccess(QVariantMap returnMap) =0;
	virtual QString methodName() const = 0;

public:
	BtcTradeQuery() : valid(false), success(false), errorMsg("Not executed")
	{}

	bool performQuery(QByteArray& apiKey, QByteArray& secret);
	void display() const;
	bool isValid() const { return valid;}
	bool isSuccess() const {return success;}
};

quint32 BtcTradeQuery::_nonce = QDateTime::currentDateTime().toTime_t();

class Info : public BtcTradeQuery
{
	QMap<QString, double> funds;
	quint64 transaction_count;
	quint64 open_orders_count;
	QDateTime server_time;

	virtual bool parseSuccess(QVariantMap returnMap) override;
	virtual QString methodName() const  override {return "getInfo";}
public:
	void showSuccess() const override;
};


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
	bool parse(const QVariant& map);

	double total() const { return amount * rate;}
	double gain() const {return ((type==Sell)?total():amount) * (1 - get_comission_pct(pair));}
	double comission() const { return ((type==Sell)?total():amount) * get_comission_pct(pair);}
};

class Trade : public BtcTradeQuery
{
	QString pair;
	Order::OrderType type;
	double rate;
	double amount;


	double received;
	double remains;
	Order::Id order_id;
	Funds funds;
protected:
	virtual QString methodName() const override;
	virtual bool parseSuccess(QVariantMap returnMap) override;
	virtual QMap<QString, QString> extraQueryParams() override;
	virtual void showSuccess() const override;

public:
	Trade(const QString& pair, Order::OrderType type, double rate, double amount)
		:BtcTradeQuery(), pair(pair), type(type), rate(rate), amount(amount)
	{}
};

class CancelOrder : public BtcTradeQuery
{
	Order::Id order_id;

public:
	CancelOrder(Order::Id order_id):BtcTradeQuery(), order_id(order_id)
	{}

	Funds funds;
protected:
	virtual QString methodName() const override;
	virtual bool parseSuccess(QVariantMap returnMap) override;
	virtual QMap<QString, QString> extraQueryParams() override;
	virtual void showSuccess() const override;
};

class ActiveOrders : public BtcTradeQuery
{
	virtual bool parseSuccess(QVariantMap returnMap) override;
	virtual QString methodName() const  override {return "ActiveOrders";}
public:
	QMap<Order::Id, Order> orders;

	void showSuccess() const override;
};

class OrderInfo : public BtcTradeQuery
{
	Order::Id order_id;
	virtual bool parseSuccess(QVariantMap returnMap) override;
	virtual QString methodName() const  override {return "OrderInfo";}
	virtual QMap<QString, QString> extraQueryParams() override;
public:
	Order order;

	OrderInfo(Order::Id id) :BtcTradeQuery(), order_id(id) {}
	void showSuccess() const override;
};

bool Info::parseSuccess(QVariantMap returnMap)
{
	funds = parseFunds(read_map(returnMap, "funds"));
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


int main(int argc, char *argv[])
{
	QByteArray apiKey = "G8K10M6S-C6T2X0FZ-E9H4RFF4-YR8TEML4-IK2UX9PV";
	QByteArray secret = "75137bd6768b7cefa199c679ef4f2d182721404de2a7ec75231f64f9f65f5b35";

	CURLcode curlResult;
	curlResult = curl_global_init(CURL_GLOBAL_ALL);
	if (curlResult != CURLE_OK)
	{
		qWarning() << curl_easy_strerror(curlResult);
	}

	Info info;
	info.performQuery(apiKey, secret);
	info.display();

	ActiveOrders orders;
	orders.performQuery(apiKey, secret);
	orders.display();

	for (Order order: orders.orders)
	{
		if (   order.status == Order::Active && order.pair == "btc_usd"
			&& order.type == Order::Sell && order.rate > 499)
		{
			CancelOrder cancel(order.order_id);
			cancel.performQuery(apiKey, secret);
		}
	}

	OrderInfo order_info(958249085);
	order_info.performQuery(apiKey, secret);
	order_info.display();

	if (order_info.order.pair == "btc_eur" && order_info.order.status == Order::Active)
	{
		CancelOrder cancel(958249085);
		cancel.performQuery(apiKey, secret);
		cancel.display();
	}

	Trade trade_sell("btc_usd", Order::Sell, 500, 0.01);
	trade_sell.performQuery(apiKey, secret);
	trade_sell.display();
	qDebug() << trade_sell.isSuccess();

	Trade trade_buy("btc_usd", Order::Buy, 100, 0.01);
	trade_buy.performQuery(apiKey, secret);
	trade_buy.display();
	qDebug() << trade_buy.isSuccess();

	curl_global_cleanup();
	return 0;
}

QByteArray BtcTradeQuery::queryParams()
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

	return map[name].toMap();
}

QDateTime read_timestamp(const QVariantMap &map, const QString &name)
{
	return QDateTime::fromTime_t(read_long(map, name));
}

bool BtcTradeQuery::parse(QByteArray serverAnswer)
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

QMap<QString, QString> BtcTradeQuery::extraQueryParams()
{
	QMap<QString, QString> params;
	params["method"] = methodName();
	params["nonce"] = BtcTradeQuery::nonce();
	return params;
}

size_t BtcTradeQuery::writeFunc(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	QByteArray* array = reinterpret_cast<QByteArray*>(userdata);
	int lenBeforeAppend = array->length();
	array->append(ptr, size * nmemb);
	return array->length() - lenBeforeAppend;
}

bool BtcTradeQuery::performQuery(QByteArray& apiKey, QByteArray& secret)
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
		curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, BtcTradeQuery::writeFunc);

		curl_easy_setopt(curlHandle, CURLOPT_URL, "https://btc-e.com/tapi");
		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, params.constData());

		headers = curl_slist_append(headers, QString("Key: %1").arg(apiKey.constData()).toUtf8().constData());
		headers = curl_slist_append(headers, QString("Sign: %1").arg(sign.toHex().constData()).toUtf8().constData());

		curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

		qDebug() << "perform query" << params;

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

void BtcTradeQuery::display() const
{
	if (!isValid())
		qWarning() << "failed query";
	else if (!isSuccess())
		qWarning() << "non success query: " << errorMsg;
	else
		showSuccess();
}

bool ActiveOrders::parseSuccess(QVariantMap returnMap)
{
	orders.clear();
	for (QString sOrderId: returnMap.keys())
	{
		Order order;

		order.order_id = sOrderId.toUInt();

		order.parse(returnMap[sOrderId]);

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

bool Order::parse(const QVariant& v)
{
	if (!v.canConvert<QVariantMap>())
		throw BrokenJson("");

	QVariantMap map = v.toMap();


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

	return true;
}

bool OrderInfo::parseSuccess(QVariantMap returnMap)
{
	order.order_id = order_id;
	valid = order.parse(read_map(returnMap, QString::number(order_id)));

	return true;
}

QMap<QString, QString> OrderInfo::extraQueryParams()
{
	QMap<QString, QString> params = BtcTradeQuery::extraQueryParams();
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

bool Trade::parseSuccess(QVariantMap returnMap)
{
	received = read_double(returnMap, "received");
	remains = read_double(returnMap, "remains");
	order_id = read_long(returnMap, "order_id");
	funds = parseFunds(read_map(returnMap, "funds"));

	return true;
}

QMap<QString, QString> Trade::extraQueryParams()
{
	QMap<QString, QString> params = BtcTradeQuery::extraQueryParams();
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

bool CancelOrder::parseSuccess(QVariantMap returnMap)
{
	if (read_long(returnMap, "order_id") != order_id)
		throw BrokenJson("order_id");

	funds = parseFunds(read_map(returnMap, "funds"));

	return true;
}

QMap<QString, QString> CancelOrder::extraQueryParams()
{
	QMap<QString, QString> params = BtcTradeQuery::extraQueryParams();
	params["order_id"] = QString::number(order_id);
	return params;
}

void CancelOrder::showSuccess() const
{

}
