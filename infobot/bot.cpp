#include <stdio.h>
#include <tgbot/tgbot.h>
#include "utils.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegExp>

#include <iostream>

#define USE_SQLITE

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);

	app.addLibraryPath("/opt/Qt/5.6/gcc_64/plugins/sqldrivers");

	QSqlDatabase db;
#ifdef USE_SQLITE
	std::clog << "use sqlite database" << std::endl;
	db = QSqlDatabase::addDatabase("QSQLITE", "trader_db");
	db.setDatabaseName("../data/trader.db");
#else
	std::clog << "use mysql database" << std::endl;
	db = QSqlDatabase::addDatabase("QMYSQL", "trader_db");
	db.setHostName("192.168.10.112");
	db.setUserName("trader");
	db.setPassword("traderread");
	db.setDatabaseName("trade");
	db.setConnectOptions("MYSQL_OPT_RECONNECT=true");
#endif

	while (!db.isOpen())
	{
		std::clog << "connecting to database ... ";
		if (!db.open())
		{
			std::clog << " FAIL. " << db.lastError().text() << std::endl;
			usleep(1000 * 1000 * 5);
		}
		else
			std::clog << " ok" << std::endl;
	}

	QSqlQuery enumeratePairs(db);
#ifdef USE_SQLITE
	enumeratePairs.prepare("SELECT distinct id || ': ' || goods || '/' ||  currency from settings");
#else
	enumeratePairs.prepare("SELECT distinct concat(id, ': ', goods, '/' , currency) from settings");
#endif

	QSqlQuery getPairById(db);
	getPairById.prepare("SELECT * from settings where id=:id");

	QSqlQuery getPairByName(db);
	getPairByName.prepare("SELECT * from settings where goods=:goods and currency=:currency");

	TgBot::Bot bot("220646763:AAFzK5J5pHlHPkMf8pOlbXwVJCkdC7rjegE");

	bot.getEvents().onCommand("pairs", [&bot, &enumeratePairs](TgBot::Message::Ptr message) {
		std::string answer;
		if (enumeratePairs.exec())
		{
			while (enumeratePairs.next()) {
				answer += enumeratePairs.value(0).toString().toStdString() + "\n";
			}
		}
		bot.getApi().sendMessage(message->chat->id, answer);
	});

	bot.getEvents().onCommand("pair", [&bot, &getPairById, &getPairByName](TgBot::Message::Ptr message) {

		auto pairStringFromSqlRow  = [](const QSqlQuery& sql) -> QString
		{
		  return   QString("id: %1\n\tprofit: %2%\n\tcomission: %3%\n\tfirst step: %4%\n\tmartingale: %5%"
						   "\n\tdep: %6\n\tcoverage: %7%\n\tcount: %8\n\tcurrency: %9\n\tgoods: %10")
				  .arg(sql.value(0).toInt())
				  .arg(sql.value(1).toDouble() * 100)
				  .arg(sql.value(2).toDouble() * 100)
				  .arg(sql.value(3).toDouble() * 100)
				  .arg(sql.value(4).toDouble() * 100)
				  .arg(sql.value(5).toDouble())
				  .arg(sql.value(6).toDouble() * 100)
				  .arg(sql.value(7).toInt())
				  .arg(sql.value(8).toString())
				  .arg(sql.value(9).toString())
				  ;
		};

		QString answer;
		QRegExp rxId ("pair ([0-9]+)");
		QRegExp rxName ("pair ([a-z]+)/([a-z]+)");

		QString text = QString::fromStdString(message->text);
		if (rxId.indexIn(text) > -1)
		{
			int id = rxId.cap(1).toInt();

			getPairById.bindValue(":id", id);
			if (getPairById.exec())
			{
				while (getPairById.next())
				{
					answer += pairStringFromSqlRow(getPairById) + "\n";
				}
				answer.chop(1); // remove last \n
			}
		}
		else if (rxName.indexIn(text) > -1)
		{
			QString goods = rxName.cap(1);
			QString currency = rxName.cap(2);

			getPairByName.bindValue(":goods", goods);
			getPairByName.bindValue(":currency", currency);

			if (getPairByName.exec())
			{
				while(getPairByName.next())
				{
					answer += pairStringFromSqlRow(getPairByName) + "\n";
				}
				answer.chop(1); // remove last \n
			}
		}
		bot.getApi().sendMessage(message->chat->id, answer.toStdString());
	});

	printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
	TgBot::TgLongPoll longPoll(bot);
	while (true) {
		printf("Long poll started\n");
		try {
			longPoll.start();
		} catch (TgBot::TgException& e) {
			printf("error: %s\n", e.what());
		}
	}
	return 0;
}
