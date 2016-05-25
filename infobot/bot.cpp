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

//	app.addLibraryPath("/opt/Qt/5.6/gcc_64/plugins/sqldrivers");

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

    QMap<QString, std::shared_ptr<QSqlQuery>> setQueriesId;
    QMap<QString, std::shared_ptr<QSqlQuery>> setQueriesName;
    QStringList verbs;
    verbs << "dep" << "profit" << "coverage" << "martingale" << "first_step" << "count";

    for (const QString& verb: verbs)
    {
        setQueriesId[verb].reset(new QSqlQuery(db));
        setQueriesId[verb]->prepare(QString("UPDATE settings set %1=:value where id=:id").arg(verb));
        setQueriesName[verb].reset(new QSqlQuery(db));
        setQueriesName[verb]->prepare(QString("UPDATE settings set %1=:value where goods=:goods and currency=:currency").arg(verb));
    }

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

    bot.getEvents().onCommand("set", [&bot, &setQueriesId, &setQueriesName, verbs](TgBot::Message::Ptr message)
    {
        QString text = QString::fromStdString(message->text);
        QString rxStr = QString("set (%1) ([0-9]+|([a-z]+)/([a-z]+)) ([0-9]+(\\.[0-9]+)?)").arg(verbs.join('|'));
        QRegExp  rx(rxStr);
        QString answer = "unknown action";
        QVariantMap params;

        QString verb;
        int id = -1;
        QString goods;
        QString currency;
        QString value;

        if (rx.indexIn(text) > -1)
        {
            if (rx.cap(3).isEmpty() && rx.cap(4).isEmpty())
                id = rx.cap(2).toInt();
            else
            {
                goods = rx.cap(3);
                currency = rx.cap(4);
            }
            verb = rx.cap(1);
            value = rx.cap(5);

            params[":value"] = value;

            QSqlQuery* pQ;
            QString pairName;

            if (id != -1)
            {
                params[":id"] = id;
                pairName = QString::number(id);
                pQ = setQueriesId[verb].get();
            }
            else
            {
                pairName = QString("%1/%2").arg(goods).arg(currency);
                params[":goods"] = goods;
                params[":currency"] = currency;
                pQ = setQueriesName[verb].get();
            }


            if (performSql("set deposit", *pQ, params))
                answer = QString("ok, %3 for pair %1 set to %2")
                         .arg(pairName)
                         .arg(value)
                        .arg(verb);
            else
                answer = "fail to set dep";
        }
        bot.getApi().sendMessage(message->chat->id,answer.toStdString());
    });

    bot.getEvents().onCommand("test", [&bot](TgBot::Message::Ptr message)
    {
        bot.getApi().sendMessage(message->chat->id, "|1|2|\n|---|---|\n|1|2|",
                                 false, message->messageId, TgBot::GenericReply::Ptr(), "Markdown");
    });

    bot.getEvents().onCommand("pair", [&bot, &getPairById, &getPairByName](TgBot::Message::Ptr message)
    {
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
