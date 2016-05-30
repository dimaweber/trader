#include "key_storage.h"

#include <readline/readline.h>

#include "utils.h"

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
	if (!cryptQuery.prepare("UPDATE secrets set apikey=:apikey, secret=:secret, is_crypted=1 where id=:id"))
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
				vault[id].apikey = QByteArray::fromHex(selectQ.value(0).toByteArray());
				vault[id].secret = QByteArray::fromHex(selectQ.value(1).toByteArray());
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
				cryptQuery.bindValue(":apikey", apikey.toHex());
				cryptQuery.bindValue(":secret", secret.toHex());
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

SqlKeyStorage::SqlKeyStorage(QSqlDatabase& db, const QString& tableName) : KeyStorage(), _tableName(tableName), db(db){}

FileKeyStorage::FileKeyStorage(const QString& fileName) : KeyStorage(), _fileName(fileName){}

QList<int> KeyStorage::allKeys()  {if (vault.isEmpty()) load(); return vault.keys();}

const QByteArray&KeyStorage::secret() { if (vault.isEmpty()) load(); return vault[currentPair].secret;}

const QByteArray&KeyStorage::apiKey() { if (vault.isEmpty()) load(); return vault[currentPair].apikey;}