#ifndef SPAMMER_H
#define SPAMMER_H

#include <QObject>
class QTimer;
class Mailer;
class Spammer : public QObject
{
    Q_OBJECT
    int emailsSent;
    int emailsToSend;
    QTimer* pTimer;
    Mailer* pMailer;

    QString getRandomSubject();
    QString getRandomMessage();
    QStringList getRandomAttach();
public:
    explicit Spammer(QObject *parent = 0);
    
signals:
    void done();
public slots:
    void spam(int count = 10, int delay=5000);
private slots:
    void onTimer();
    void onSend(const QString& res);
};

#endif // SPAMMER_H
