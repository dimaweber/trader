#ifndef KEY_STORAGE_H
#define KEY_STORAGE_H

#include <QMap>
#include <QByteArray>
#include <QFile>
#include <QDataStream>
#include <QSqlQuery>
#include <QSqlError>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/aes.h>

class QSqlDatabase;

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
	const QByteArray& apiKey();
	const QByteArray& secret();

	void changePassword();

	QList<int> allKeys();
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

class SqlKeyStorage : public KeyStorage
{
	QString _tableName;
	QSqlDatabase& db;

protected:
	virtual void load() override;
	virtual void store() override;

public:
	SqlKeyStorage(QSqlDatabase& db, const QString& tableName);
};

#endif // KEY_STORAGE_H
