#include "utils.h"

std::ostream& operator << (std::ostream& stream, const QString& str)
{
	return stream << qPrintable(str);
}


QByteArray hmac_sha512(const QByteArray& message, const QByteArray& key, HashFunction func)
{
	unsigned char* digest = nullptr;
	unsigned int digest_size = 0;

	digest = HMAC(func(), reinterpret_cast<const unsigned char*>(key.constData()), key.length(),
						  reinterpret_cast<const unsigned char*>(message.constData()), message.length(),
						  digest, &digest_size);

	return QByteArray(reinterpret_cast<char*>(digest), digest_size);
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
