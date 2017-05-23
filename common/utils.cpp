#include "utils.h"
#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlError>
#include <QElapsedTimer>

#include <openssl/sha.h>
#include <openssl/hmac.h>

std::ostream& operator << (std::ostream& stream, const QString& str)
{
    return stream << qPrintable(str);
}


QByteArray hmac_sha512(const QByteArray& message, const QByteArray& key)
{
    unsigned char* digest = nullptr;
    unsigned int digest_size = 0;

    digest = HMAC(EVP_sha512(), reinterpret_cast<const unsigned char*>(key.constData()), key.length(),
                          reinterpret_cast<const unsigned char*>(message.constData()), message.length(),
                          digest, &digest_size);

    return QByteArray(reinterpret_cast<char*>(digest), digest_size);
}



QByteArray hmac_sha384(const QByteArray& message, const QByteArray& key)
{
    unsigned char* digest = nullptr;
    unsigned int digest_size = 0;

    digest = HMAC(EVP_sha384(), reinterpret_cast<const unsigned char*>(key.constData()), key.length(),
                          reinterpret_cast<const unsigned char*>(message.constData()), message.length(),
                          digest, &digest_size);

    return QByteArray(reinterpret_cast<char*>(digest), digest_size);
}


bool prepareSql(QSqlQuery& query, const QString& sql)
{
    if (!query.prepare(sql))
    {
        std::cerr << query.lastError().text() << std::endl;
        return false;
    }
    return true;
}

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

bool performSql(QString message, QSqlQuery& query, const QVariantMap& binds, bool silent)
{
    for(const QString& param: binds.keys())
    {
        query.bindValue(param, binds[param]);
        if (!silent)
            std::clog << "\tbind: " << param << " = " << binds[param].toString() << std::endl;
        message.replace(param, binds[param].toString());
    }
    return performSql(message, query, QString(), silent);
}
