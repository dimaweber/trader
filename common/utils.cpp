#include "utils.h"
#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlError>

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

qint64 read_long(const QVariantMap& map, const QString& name)
{
    bool ok;
    long ret = read_string(map, name).toLongLong(&ok);

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
    ret[key_field] = name;

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

bool performSql(const QString& message, QSqlQuery& query, const QString& sql, bool silent)
{
    bool ok;
    if (!silent)
        std::clog << QString("[sql] %1:").arg(message) << std::endl;
    if (sql.isEmpty())
        ok = query.exec();
    else
        ok = query.exec(sql);
    if (!silent)
        std::clog << query.lastQuery() << std::endl;
    if (ok)
    {
        if(!silent)
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

            if (!silent)
                std::clog << QString("(return %1 records)").arg(count);
        }
        else
            if (!silent)
                std::clog << QString("(affected %1 records)").arg(query.numRowsAffected());
    }
    else
    {
        if (!silent)
            std::clog << "Fail.";
        std::cerr << "SQL: " << query.lastQuery() << "."
                  << "Reason: " << query.lastError().text();
    }
    if(!silent)
        std::clog << std::endl;
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
