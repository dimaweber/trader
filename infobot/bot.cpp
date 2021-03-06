#include "utils.h"
#include "sql_database.h"

#include <stdio.h>
#include <tgbot/tgbot.h>

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QRegExp>
#include <QSettings>

#include <iostream>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QString iniFilePath = QCoreApplication::applicationDirPath() + "/../data/trader.ini";
    if (!QFileInfo(iniFilePath).exists())
    {
        throw std::runtime_error("*** No INI file!");
    }
    QSettings settings(iniFilePath, QSettings::IniFormat);

    Database database(settings);
    if (!database.init())
    {
        throw std::runtime_error("failt to connect to db");
        return 1;
    }

    QSqlQuery enumeratePairs = database.getQuery();

    enumeratePairs.prepare("SELECT distinct concat(id, ': ', goods, '/' , currency) from settings");

    QSqlQuery getPairById = database.getQuery();
    getPairById.prepare("SELECT id, profit, comission, first_step, martingale,"
                        "dep, coverage, count, currency, goods, enabled "
                        "from settings where id=:id");

    QSqlQuery getPairByName = database.getQuery();
    getPairByName.prepare("SELECT * from settings where goods=:goods and currency=:currency");

    QMap<QString, std::shared_ptr<QSqlQuery>> setQueriesId;
    QMap<QString, std::shared_ptr<QSqlQuery>> setQueriesName;
    QStringList verbs;
    verbs << "dep" << "profit" << "coverage" << "martingale" << "first_step" << "count" << "enabled";

    for (const QString& verb: verbs)
    {
        setQueriesId[verb].reset(new QSqlQuery(database.getQuery()));
        setQueriesId[verb]->prepare(QString("UPDATE settings set %1=:value where id=:id").arg(verb));
        setQueriesName[verb].reset(new QSqlQuery(database.getQuery()));
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
        QString idRxStr ("[0-9]+|([a-z]{3})/([a-z]{3})");
        QString actionRxStr = QString("(%1)\\s*(?::|=)\\s*([0-9]+(?:\\.[0-9]+)?%?)").arg(verbs.join('|'));
        QString setRxStr = QString("set\\s+(%1)((?:\\s+%2)+)").arg(idRxStr).arg(actionRxStr);

        std::cout << setRxStr << std::endl;

        QString text = QString::fromStdString(message->text);
        QRegExp  rx(setRxStr);
        QRegExp idRx(idRxStr);
        QRegExp actionRx(actionRxStr);

        QString answer;
        QVariantMap params;

        QString verb;
        QString goods;
        QString currency;
        QString value;

        if (rx.indexIn(text) > -1)
        {
            QString idStr = rx.cap(1);
            QString actionStr = rx.cap(4);

            std::cout << "id: " << idStr << std::endl
                      << "actions: " << actionStr << std::endl;

            QSqlQuery* pQ;
            QString pairName;
            bool isId = false;

            if (idRx.indexIn(idStr) > -1)
            {
                pairName = idRx.cap(0);
                int id = pairName.toInt(&isId);
                goods = idRx.cap(1);
                currency = idRx.cap(2);

                if (isId)
                {
                    params[":id"] = id;
                }
                else
                {
                    params[":goods"] = goods;
                    params[":currency"] = currency;
                }


                std::cout << "id: " << id
                          << ", goods: " << goods
                          << ", currency: " << currency
                          << std::endl;
            }

            int pos = 0;
            while (actionRx.indexIn(actionStr, pos) > -1)
            {
                verb = actionRx.cap(1);
                value = actionRx.cap(2);

                if (isId)
                    pQ = setQueriesId[verb].get();
                else
                    pQ = setQueriesName[verb].get();

                if (value.endsWith('%'))
                {
                    value.chop(1);
                    value = QString::number( value.toDouble() / 100);
                }

                std::cout << "action: " << verb << ", value: " << value << std::endl;

                pos += actionRx.matchedLength();

                params[":value"] = value;

                if (performSql("set deposit", *pQ, params, true))
                    answer += QString("%3 for pair %1 set to %2\n")
                             .arg(pairName)
                             .arg(value)
                            .arg(verb);
                else
                    answer = QString("fail to set %1 for pair %2\n").arg(verb).arg(pairName);
            }
        }
        else
            answer = "unknown action";
        bot.getApi().sendMessage(message->chat->id,answer.toStdString());
    });

    bot.getEvents().onCommand("test", [&bot](TgBot::Message::Ptr message)
    {
        bot.getApi().sendMessage(message->chat->id, "`|1|2|\n|-|-|\n|1|2|`",
                                 false, message->messageId, TgBot::GenericReply::Ptr(), "Markdown");
    });

    bot.getEvents().onCommand("pair", [&bot, &getPairById, &getPairByName](TgBot::Message::Ptr message)
    {
        auto pairStringFromSqlRow  = [](const QSqlQuery& sql) -> QString
        {
          return   QString("id: %1\n\tprofit: %2%\n\tcomission: %3%\n\tfirst step: %4%\n\tmartingale: %5%"
                           "\n\tdep: %6\n\tcoverage: %7%\n\tcount: %8\n\tcurrency: %9\n\tgoods: %10\n\tenabled: %11")
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
                  .arg(sql.value(10).toBool())
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
