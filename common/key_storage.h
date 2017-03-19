#ifndef KEY_STORAGE_H
#define KEY_STORAGE_H

#include <QMap>
#include <QByteArray>
#include <QFile>
#include <QDataStream>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>

class QSqlDatabase;

class IKeyStorage
{
public:
    virtual void setPassword(const QByteArray& pwd) =0;
    virtual bool setCurrent(int id) =0;
    virtual const QByteArray& apiKey() =0;
    virtual const QByteArray& secret() =0;

    virtual void changePassword() = 0;

    virtual QList<int> allKeys() = 0;
};

class KeyStorage : public IKeyStorage
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

    virtual void setPassword(const QByteArray& pwd) override;
    virtual bool setCurrent(int id) override final;
    virtual const QByteArray& apiKey() override final;
    virtual const QByteArray& secret() override final;
    virtual void changePassword() override final;
    virtual QList<int> allKeys() override final;
};

class FileKeyStorage : public KeyStorage
{
    QString _fileName;

protected:
    QString fileName() const { return _fileName;}

    virtual void load() override;
    virtual void store() override;
public:
    FileKeyStorage(const QString& fileName);
};


#endif // KEY_STORAGE_H
