#include "mailer.h"
#include "smtp.h"
#include "smptAuthData.h"
#include <QSettings>
#include <QFile>
#include <QDebug>

Mailer::Mailer(QObject *parent) :
    QObject(parent)
{
    buildMailServices();
}

SmtpAuthData Mailer::buildAuthFromConfig()
{
    SmtpAuthData smtpAuthData;
    SmtpAuthData::Data data;
    QSettings config;
    config.sync();
    data.host = config.value("host").toString();
    // your email address
    data.email = config.value("from_email").toString();
    // login for your email account
    data.login = config.value("login").toString();
    // name which will be shown in From: line(optional)
    data.name = config.value("display_name").toString();
    // password for auth on email account
    data.password = config.value("password").toString();
    // port for host 587 default
    data.port = config.value("port").toString();
    // ssl support
    data.enableSsl = config.value("use_ssl").toBool();
    smtpAuthData.rewriteData(data);

    return smtpAuthData;
}

void Mailer::buildMailServices()
{
    mailServices.append( buildAuthFromConfig() );
}

SmtpAuthData Mailer::randomMailService()
{
    int index = qrand() % mailServices.size();
    qDebug() << "use service " << index;
    return mailServices[index];
}

void Mailer::send(int customerId, const QString &to, const QString &subject, const QString &body, const QStringList &attach, const int smtpServer)
{
    Letter* letter;
    this->customerId = customerId;
    if (smtpServer == -1 || smtpServer >= mailServices.count())
    {
        letter = new Letter( randomMailService() );
    }
    else if ( smtpServer == -2)
    {
        letter = new Letter( buildAuthFromConfig() );
    }
    else
        letter = new Letter( mailServices[smtpServer] );
    connect (letter, SIGNAL(sendingProcessState(int,QString)), SLOT(onMailSent(int,QString)));

    QStringList verifiedAttaches;
    foreach(QString file, attach)
    {
        if (QFile(file).exists())
            verifiedAttaches.append(file);
        else
            qWarning() << QString(tr("File %1 not found -- won't be attached")).arg(file);
    }

    //QString blindCopyReceivers = "";
    // choice letter encoding
    letter->set_receivers(to);
    //letter.set_blindCopyReceivers(blindCopyReceivers);
    letter->set_subject(subject);
    if (!attach.isEmpty())
        letter->set_attachement(verifiedAttaches);
    letter->set_text(body.toUtf8());

    // send letter
    letter->send();
}

void Mailer::addSmtpServer(const QString &host, const QString &port, const bool useSSl, const QString &email, const QString &displayname, const QString &login, const QString &password)
{
    SmtpAuthData smtpAuthData;
    SmtpAuthData::Data data;
    data.host = host;
    data.email = email;
    data.login = login;
    data.name = displayname;
    data.password = password;
    data.port = port;
    data.enableSsl = useSSl;
    smtpAuthData.rewriteData(data);

    mailServices.append(smtpAuthData);
}

void Mailer::onMailSent(int code, QString msg)
{
    Letter* letter = (Letter*)sender();
    QString message;
    switch ((Letter::SendCodes)code)
    {
        case Letter::OK:
            message = "Letter sent succesfully";
            break;
        case Letter::RECIEP_LIST_NOT_VALID:
            message = "Recipients list is empty";
            break;
        case Letter::BCC_NOT_VALID:
            message = "Carbon copy list is invalid";
        case Letter::CANT_OPEN_ATTACH:
            message = QString("Cannot open attachment file %s").arg(msg);
            break;
        case Letter::SERVER_INIT_ERROR:
            message = "Server connection error";
            break;
        case Letter::SERVER_NOT_ANSWER:
            message = "Server do not respond";
            break;
        case Letter::AUTH_FAIL:
            message = "Authentification fails";
            break;
        case Letter::INVALID_LOGIN:
            message = "No such user";
            break;
        case Letter::PASSWORD_INVALID:
            message = "Password is incorrent";
            break;
        case Letter::RECIEP_NOT_EXISTS:
            message = "Recipient does not exist";
            break;
        case Letter::BCC_ADDR_INVALID:
            message = "BCC address is invalid";
            break;
        case Letter::SERVER_ABORT_DATA:
            message = "Server aborts DATA command";
            break;
        case Letter::SERVER_ABORT_SEND:
            message = "Server aborts SEND command";
            break;
        case Letter::SERVER_ABORT_FROM:
            message = "Server aborts FROM command";
            break;
        default:
        case Letter::ERROR:
            message = QString("Generic error: %1").arg(msg);
            break;
    }

    emit sentResult(QString("%1 [Sent using %2]").arg(message).arg(letter->host()));
    letter->deleteLater();
}
