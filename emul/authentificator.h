#ifndef AUTHENTIFICATOR_H
#define AUTHENTIFICATOR_H

#include "sqlclient.h"

#include <QtCore/qglobal.h>
#include <QCache>
#include <QMutex>
#include <QSqlDatabase>
#include <QSqlQuery>

#include <memory>

class Authentificator
{
    std::shared_ptr<AbstractDataAccessor> dataAccessor;
public:
    Authentificator(std::shared_ptr<AbstractDataAccessor>& dataAccessor);
    bool authOk(const ApiKey& key, const QByteArray& sign, const QString& nonce, const QByteArray rawPostData, QString& message);
    bool hasInfo(const ApiKey& key);
    bool hasTrade(const ApiKey& key);
    bool hasWithdraw(const ApiKey& key);
private:
    ApikeyInfo::Ptr validateKey(const ApiKey& key);
    bool validateSign(const ApikeyInfo::Ptr& pkey, const QByteArray& sign, const QByteArray& data);
    quint32 nonceOnKey(const QString& key);
    bool checkNonce(const QString& key, quint32 nonce);

    QByteArray getSecret(const QString& key);
    bool updateNonce(const QString& key, quint32 nonce);
};

#endif // AUTHENTIFICATOR_H
