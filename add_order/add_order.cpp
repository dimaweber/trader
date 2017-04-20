#include "sql_database.h"
#include "optionparser.h"
#include "mailer.h"

#include <QtCore/QCoreApplication>
#include <QtCore/qglobal.h>
#include <QFileInfo>
#include <QSettings>
#include <QSqlQuery>
#include <QDateTime>

#include <iostream>


/// TODO: check rate - prevent instant orders
/// TODO: interactive mode

struct Arg: public option::Arg
{
  static void printError(const char* msg1, const option::Option& opt, const char* msg2)
  {
    fprintf(stderr, "%s", msg1);
    fwrite(opt.name, opt.namelen, 1, stderr);
    fprintf(stderr, "%s", msg2);
  }

  static option::ArgStatus Unknown(const option::Option& option, bool msg)
  {
    if (msg) printError("Unknown option '", option, "'\n");
    return option::ARG_ILLEGAL;
  }

  static option::ArgStatus Required(const option::Option& option, bool msg)
  {
    if (option.arg != 0)
      return option::ARG_OK;

    if (msg) printError("Option '", option, "' requires an argument\n");
    return option::ARG_ILLEGAL;
  }

  static option::ArgStatus NonEmpty(const option::Option& option, bool msg)
  {
    if (option.arg != 0 && option.arg[0] != 0)
      return option::ARG_OK;

    if (msg) printError("Option '", option, "' requires a non-empty argument\n");
    return option::ARG_ILLEGAL;
  }

  static option::ArgStatus Numeric(const option::Option& option, bool msg)
  {
    char* endptr = 0;
    if (option.arg != 0 && strtol(option.arg, &endptr, 10)){};
    if (endptr != option.arg && *endptr == 0)
      return option::ARG_OK;

    if (msg) printError("Option '", option, "' requires a numeric argument\n");
    return option::ARG_ILLEGAL;
  }

  static option::ArgStatus Decimal(const option::Option& option, bool msg)
  {
    if (option.arg != 0)
    {
        bool ok;
        QString(option.arg).toDouble(&ok);
        if (ok)
            return option::ARG_OK;
    }

    if (msg) printError("Option '", option, "' requires a decimal argument\n");
    return option::ARG_ILLEGAL;
  }
};

enum optionIndex{Unknown, Help, SettingsId/*, RoundId, Pair*/, Amount, Rate /*, Value*/};

const option::Descriptor usage[]={
    {Unknown, 0, "", "", Arg::Unknown, "Usage: add_order {options}\n\nOptions:"},
    {Help, 0, "", "help", Arg::None, "\t--help \tPrint usage and exit."},
    {SettingsId, 0, "", "settings_id", Arg::Numeric, "--settings_id"},
    {Amount, 0,"","amount", Arg::Decimal, "--amount"},
    {Rate, 0,"","rate", Arg::Decimal, "--rate"},
    {0,0,0,0,0,0}
};

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QString iniFilePath = QCoreApplication::applicationDirPath() + "/../data/trader.ini";
    if (!QFileInfo(iniFilePath).exists())
    {
        throw std::runtime_error("*** No INI file!");
    }
    QSettings settings(iniFilePath, QSettings::IniFormat);

    Database database(settings);
    if (! database.init())
    {
        throw std::runtime_error("failt to connect to db");
        return 1;
    }

    argc -= (argc>0);
    argv += (argc>0);
    option::Stats stats(usage, argc, argv);
    option::Option* options = new option::Option[stats.options_max];
    option::Option* buffer = new option::Option[stats.buffer_max];
    option::Parser parser(usage, argc, argv, options, buffer);

    if (parser.error())
        return 1;

    if (options[Help] )
    {
        int columns = getenv("COLUMNS")?atoi(getenv("COLUMNS")) : 80;
        option::printUsage(fwrite, stdout, usage, columns);
        return 0;
    }

    quint32 settings_id = 0;
    double amount =0;
    double rate =0;

    bool silent_sql = settings.value("debug/silent_sql", "true").toBool();

    for (int i=0; i<parser.optionsCount(); i++)
    {
        option::Option& opt = buffer[i];
        QString arg = opt.arg;
        switch (opt.index())
        {
            case SettingsId:  settings_id = arg.toUInt(); break;
            case Amount: amount = arg.toDouble();break;
            case Rate: rate = arg.toDouble(); break;
        }
    }

    delete [] buffer;
    delete [] options;

    if (amount == 0 || rate == 0)
    {
        std::clog << "amount or rate not specified. Please set both values" << std::endl;
        return 2;
    }

    if (settings_id == 0)
    {
        std::clog << "settings id not set" << std::endl;
        return 6;
    }

    QVariantMap params;
    params[":settings_id"] = settings_id;
    params[":amount"] = amount;
    params[":rate"] = rate;

    performSql("insert order into queue", *database.insertIntoQueue, params, silent_sql);

    std::clog << "order created";

    Mailer mailer(nullptr);
    a.connect (&mailer, SIGNAL(sentResult(QString)), &a, SLOT(quit()));
    mailer.addSmtpServer("192.168.10.4", "25", false, "nas@dweber.lan", "NAS", "nas", "zooTh1ae");
    mailer.send(QDateTime::currentDateTime().toMSecsSinceEpoch(), "diepress@gmail.com", "Order created",
                QString("Your order for settings id %1 %2@%3 has been created").arg(settings_id).arg(amount).arg(rate), QStringList(), 1);

    return a.exec();
}
