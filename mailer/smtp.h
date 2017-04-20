#ifndef SMTP_H
#define SMTP_H

#include <QStringList>
#include <QPair>
#include <stdexcept>
#ifndef QT_NO_SSL
# include <QSslSocket>
#else
# include <QTcpSocket>
#endif
#include <QTextStream>
#include "smptAuthData.h"

typedef QList <QPair <QString, QString> > ReceiversList;

class Letter : public QObject, private SmtpAuthData
{
    QString header;
    QString text;
    QString subject;
    ReceiversList receiversList;
    ReceiversList blindReceiversList;
    QStringList attachementFilesList;
    QString smtpItself;

    SmtpAuthData smtpData;

    // socket for ssl encrypted connection
#ifndef QT_NO_SSL
    QSslSocket sslSocket;
#else
    QTcpSocket sslSocket;
#endif
    // stream that operate of protocol
    QTextStream stream;
    quint16 blockSize;

    Q_OBJECT

    void toStream(const QString& str);

public:
    enum SendCodes {OK=0, ERROR, RECIEP_LIST_NOT_VALID, BCC_NOT_VALID, CANT_OPEN_ATTACH, SERVER_INIT_ERROR, SERVER_NOT_ANSWER, AUTH_FAIL, INVALID_LOGIN, PASSWORD_INVALID, RECIEP_NOT_EXISTS, BCC_ADDR_INVALID, SERVER_ABORT_DATA, SERVER_ABORT_SEND, SERVER_ABORT_FROM};
    Letter(const SmtpAuthData &smtpAuthData);
    ~Letter();
    void setSmtpAuth(const SmtpAuthData &smtpAuthData);
    // enable or disable ssl encrypting
    void set_sslEncrypting(bool enable);
    // set letter text
    void set_text(QString text);
    void set_subject(QString subj);
    // add attachement to file
    void set_attachement(const QStringList &list = QStringList());
    // get letter text
    QString get_text() { return text; }
    // set receivers
    void set_receivers(QString receivers);
    // set blind copy receivers
    void set_blindCopyReceivers(QString bl_receivers = QString());
    void setAuthData(const SmtpAuthData& data);

    // send Letter
    void send();

    QString host()
    {
        return smtpData.get_host();
    }

private:

    const QString FILE_SEPARATOR;

    enum RequestState { RequestState_Init, RequestState_AUTH, RequestState_AutorizeLogin,
                        RequestState_AutorizePass,
                    RequestState_From, RequestState_To, RequestState_BlindCopy,
                    RequestState_Data,
                RequestState_Mail, RequestState_Quit, RequestState_AfterEnd} Request;

    QString encodeToBase64(QString line);

    void on_connect();
    void send_request(QString line);
    void set_header();
    QByteArray& chunk_split(QByteArray &fileContent, int chunklen = 76);
    QString extractFileName(const QString &fullName);
    void addMoreRecipients(const QString &serverSays);
    void establishConnectionToSocket(int port);
    bool isEnabledSsl() const;

    QByteArray *p_FilesContent;
    int filesCount;
    bool recipientsExist;


private slots:
    void error_happens(QAbstractSocket::SocketError socketError);
#ifndef QT_NO_SSL
    void sslError_happens(const QList<QSslError> &sslErrors);
#endif
    void on_read();
    void ready();

signals:
    void sendingProcessState(int code, QString extraInfo);

};


#endif // SMTP_H
