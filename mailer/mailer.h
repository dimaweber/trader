#ifndef MAILER_H
#define MAILER_H

#include <QObject>
#include <QList>
#include <QStringList>
#include "smptAuthData.h"
#include "smtp.h"

class Mailer : public QObject
{
    Q_OBJECT
    SmtpAuthData buildAuthFromConfig();

    QList<SmtpAuthData> mailServices;
    void buildMailServices();
    SmtpAuthData randomMailService();

public:
    int customerId;
    explicit Mailer( QObject *parent = 0);
    void send(int customerId,
              const QString& to,
              const QString& subject,
              const QString& body,
              const QStringList& attach = QStringList(),
              const int smtpServer = 0); // 0..n -- use n's server, -1 -- use random server, -2 -- reread conf and use it
    void addSmtpServer (const QString& adr, const QString& port, const bool useSSl, const QString& email, const QString& displayname, const QString& login, const QString& password);
private slots:
    void onMailSent(int code, QString extraInfo);
signals:
    void sentResult(const QString& msg);
};

#endif // MAILER_H
