#include "smtp.h"

#include <QRegExp>
#include <QTextCodec>
#include <QDebug>
#include <QObject>
#include <QDateTime>
#include <QFileInfo>
#include <QDataStream>

const QString mailLog = "mail.log";
QFile file (mailLog);

void log(const QString& str)
{
    file.open(QFile::Append);
    file.write(str.toUtf8());
    file.write("\n");
    file.close();
}

QByteArray toBaseAndChunk(const QByteArray &ba)
{
    const char alphabet[] = "ABCDEFGH" "IJKLMNOP" "QRSTUVWX" "YZabcdef"
                            "ghijklmn" "opqrstuv" "wxyz0123" "456789+/";

    const char padchar = '=';
    int padlen = 0;
    int baseSize = ba.size();
    int counter = 0;
    const int CRITICAL_SYMBOL = 76;
    const char *underlayindData = ba.data();
    const int INSERTED_SYMBOLS = ((baseSize * 4) / 3 + 3) / 38; // ( ((baseSize * 4) / 3 + 3) / 76 * 2)

    qDebug() << "Inserted symbols -> " << INSERTED_SYMBOLS;

    QByteArray tmp((baseSize * 4) / 3 + 3 + INSERTED_SYMBOLS + 3, Qt::Uninitialized);

    int i = 0;
    char *out = tmp.data();
    while (i < baseSize) {
        int chunk = 0;
        chunk |= int(uchar(underlayindData[i++])) << 16;
        if (i == baseSize) {
            padlen = 2;
        } else {
            chunk |= int(uchar(underlayindData[i++])) << 8;
            if (i == baseSize) padlen = 1;
            else chunk |= int(uchar(underlayindData[i++]));
        }

        int j = (chunk & 0x00fc0000) >> 18;
        int k = (chunk & 0x0003f000) >> 12;
        int l = (chunk & 0x00000fc0) >> 6;
        int m = (chunk & 0x0000003f);

        if (counter == CRITICAL_SYMBOL)
        {
                counter = 0;
                *out++ = '\r';
                *out++ = '\n';
        }
        *out++ = alphabet[j];
        ++counter;

        if (counter == CRITICAL_SYMBOL)
        {
                counter = 0;
                *out++ = '\r';
                *out++ = '\n';
        }
        *out++ = alphabet[k];
        ++counter;
        if (padlen > 1)
        {
                if (counter == CRITICAL_SYMBOL)
                {
                        counter = 0;
                        *out++ = '\r';
                        *out++ = '\n';
                }
                *out++ = padchar;
                ++counter;
        }
        else
        {
                if (counter == CRITICAL_SYMBOL)
                {
                        counter = 0;
                        *out++ = '\r';
                        *out++ = '\n';
                }
                *out++ = alphabet[l];
                ++counter;
        }
        if (padlen > 0)
        {
                if (counter == CRITICAL_SYMBOL)
                {
                        counter = 0;
                        *out++ = '\r';
                        *out++ = '\n';
                }
                *out++ = padchar;
                ++counter;
        }
        else
        {
                if (counter == CRITICAL_SYMBOL)
                {
                        counter = 0;
                        *out++ = '\r';
                        *out++ = '\n';
                }
                *out++ = alphabet[m];
                ++counter;
        }
    }

    tmp.truncate(out - tmp.data());
    return tmp;
}

Letter::Letter(const SmtpAuthData &smtpAuthData)
    : FILE_SEPARATOR("----------A4D921C2D10D666")
{
    //smtpData.rewriteData(smtpAuthData.get_SmtpAuth());
    setAuthData(smtpAuthData);

    blockSize = 0;
    filesCount = 0;
    recipientsExist = false;
#ifndef QT_NO_SSL
    sslSocket.setProtocol(QSsl::SslV3);
#endif
    // init text stream
    stream.setDevice(&sslSocket);

    connect(&sslSocket, SIGNAL(error(QAbstractSocket::SocketError )),
            this, SLOT(error_happens(QAbstractSocket::SocketError )));

#ifndef QT_NO_SSL
    connect(&sslSocket, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslError_happens(QList<QSslError>)));
#endif

    connect(&sslSocket, SIGNAL(readyRead()), this, SLOT(on_read()));


}

void Letter::setAuthData(const SmtpAuthData &data)
{
    smtpData.rewriteData(data.get_SmtpAuth());
}

void Letter::setSmtpAuth(const SmtpAuthData &smtpAuthData)
{
    smtpData.rewriteData(smtpAuthData.get_SmtpAuth());
}


void Letter::set_sslEncrypting(bool enable)
{
    const SmtpAuthData::Data &smtpAuth =
            smtpData.get_SmtpAuth();

    SmtpAuthData::Data newData = smtpAuth;
    newData.enableSsl = enable;

    smtpData.rewriteData(newData);

}

bool Letter::isEnabledSsl() const
{
    const SmtpAuthData::Data &smtpAuth =
            smtpData.get_SmtpAuth();

    return smtpAuth.enableSsl;
}

void Letter::set_subject(QString subj)
{
    subject = subj;
}
// FIX:: upgrade
void Letter::set_text(QString text)
{
    qDebug() << "Email at -> " << smtpData.get_SmtpAuth().email;
    // if no including files we send only text mail
    if (!filesCount)
        this->text = text;
    // else add files to mail
    else
    {
        this->text.clear();
        // prepend plain text
        this->text += QString("--%1\r\n"
                      "Content-Type: text/plain; charset=utf-8\r\n"
                      "Content-Transfer-Encoding: 8bit\r\n\r\n"
                      "%2\r\n\r\n"
                      "--%3").arg(FILE_SEPARATOR,text, FILE_SEPARATOR);

        // prepend encoded files
        for (int i = 0; i < filesCount; ++i)
        {

            QString fileName = extractFileName(attachementFilesList.at(i));
            qDebug() << "After extract";

            this->text += "\r\n";
            this->text += QString(
                    "Content-Type: application/octet-stream; name=\"%1\"\r\n"
                    "Content-transfer-encoding: base64\r\n"
                    "Content-Disposition: attachment; filename=\"%2\"\r\n\r\n"
                    ).arg(fileName, fileName);

            this->text += p_FilesContent[i] + "\r\n";
            this->text += QString("--%1").arg(FILE_SEPARATOR);

        }

        this->text += "--";

    }

    // set header for letter
    set_header();

}

void Letter::set_header()
{
    header.clear();

    const SmtpAuthData::Data &smtpAuth =
            smtpData.get_SmtpAuth();


    header = "Date: " + QDateTime::currentDateTime().toString("dd.MM.yyyy, hh:mm:ss \r\n");

    header += "From: =?utf-8?B?" + encodeToBase64(smtpAuth.name) +
              "?= <" + smtpAuth.email + ">\r\n";

    header += "X_Mailer: Mail Client 1.0\r\n";
    header += "Reply-To: =?utf-8?B?" +
              encodeToBase64(smtpAuth.name) + "?= <" + smtpAuth.email + ">\r\n";
    header += "X-Priority: 3 (Normal)\r\n";
    header += "Message-ID: <172562218." + QDateTime::currentDateTime()
              .toString("yyyyMMddhhmmss@%1>\r\n").arg(smtpItself.mid(5));

    header += "To: ";

    QStringList mailTo_List;
    for (int i = 0; i < receiversList.count(); ++i)
    {
        mailTo_List.append("=?utf-8?B?" + encodeToBase64(receiversList.at(i).first) +
                          "?= <" + receiversList.at(i).second + ">");
    }

    if (receiversList.count() > 1)
        header += mailTo_List.join(", ") + "\r\n";
    else
        header += mailTo_List.at(0) + "\r\n";

    if (!blindReceiversList.isEmpty())
    {
        header += "Bcc: ";

        QStringList mailBcc_List;
        for (int i = 0; i < blindReceiversList.count(); ++i)
        {
            mailBcc_List.append("=?utf-8?B?" + encodeToBase64(blindReceiversList.at(i).first) +
                              "?= <" + blindReceiversList.at(i).second + ">");
        }

        if (blindReceiversList.count() > 1)
            header += mailBcc_List.join(", ") + "\r\n";
        else
            header += mailBcc_List.at(0) + "\r\n";
    }

    header += "Subject: =?utf-8?B?" + encodeToBase64(subject) + "?=\r\n";
    header += "MIME-Version: 1.0\r\n";

    if (attachementFilesList.isEmpty())
    {
        header += "Content-Type: text/plain; charset=utf-8\r\n";
        header += "Content-Transfer-Encoding: 8bit\r\n";
    }
    else
        header += QString("Content-Type: multipart/mixed; boundary=\"%1\"\r\n")
                        .arg(FILE_SEPARATOR);

}

void Letter::set_receivers(QString receivers)
{
    receiversList.clear();

    QStringList emailGroupList(receivers.split(",", QString::SkipEmptyParts));

    if (emailGroupList.empty())
    {
        emit sendingProcessState((int)RECIEP_LIST_NOT_VALID, "");
        qDebug() << "Receiver list not valid";
        return;
    }


    for (int i = 0; i < emailGroupList.count(); ++i)
    {

        QStringList list;

        list << emailGroupList.at(i).split(QRegExp("<|>|\"| "), QString::SkipEmptyParts);

        if (list.count() > 1)
            receiversList.append(qMakePair(list.at(0), list.at(1)));
        else if (list.count() == 1)
            receiversList.append(qMakePair(QString(), list.at(0)));
        else
            qDebug() << "an error ocured empty list!"
                    << "in void Letter::set_receivers(QString receivers()";
    }

}

void Letter::set_blindCopyReceivers(QString bl_receivers)
{
    blindReceiversList.clear();

    QStringList emailGroupList(bl_receivers.split(",", QString::SkipEmptyParts));

    if (emailGroupList.empty())
    {
        emit sendingProcessState((int)BCC_NOT_VALID, "");
        return;
    }


    for (int i = 0; i < emailGroupList.count(); ++i)
    {

        QStringList list;

        list << emailGroupList.at(i).split(QRegExp("<|>|\"| "), QString::SkipEmptyParts);

        if (list.count() > 1)
            blindReceiversList.append(qMakePair(list.at(0), list.at(1)));
        else if (list.count() == 1)
            blindReceiversList.append(qMakePair(QString(), list.at(0)));
        else
            qDebug() << "an error ocured empty list!"
                    << "in void Letter::set_blindCopyReceivers(QString bl_receivers)";
    }


}

void Letter::set_attachement(const QStringList &list)
{
    //attachementFilesList = list;
    filesCount = list.count();
    p_FilesContent = new QByteArray [filesCount];
    attachementFilesList.clear();

    for (int i = 0; i < filesCount; ++i)
    {

        QString attachedFileName = list.at(i);

        QFile file(attachedFileName);

        if (!file.open(QIODevice::ReadOnly))
        {
            emit sendingProcessState((int)CANT_OPEN_ATTACH, extractFileName(attachedFileName));

            qDebug() << "file don't open -> " << attachedFileName;
            continue;

        }

        attachementFilesList.append(attachedFileName);
        p_FilesContent[i] = toBaseAndChunk(file.readAll());

    }

}

QString Letter::encodeToBase64(QString line)
{
    QByteArray encodedArray;

    encodedArray.append(line);

    return encodedArray.toBase64();
}

QByteArray& Letter::chunk_split(QByteArray &fileContent, int chunklen)
{
    int counter = chunklen;

    while (counter < fileContent.count())
    {
        fileContent.insert(counter, "\r\n");
        counter += chunklen;
    }

    return fileContent;
}

QString Letter::extractFileName(const QString &fullName)
{
    return QFileInfo(fullName).fileName();
}

void Letter::send()
{
    //QStringList list;

    //list = smtpData.get_email().split("@", QString::SkipEmptyParts);


    smtpItself = smtpData.get_host();

    bool ok;

    int port = smtpData.get_port().toInt(&ok);
    if (!ok)
        port = 587;

    // establish new connection
    establishConnectionToSocket(port);


}

void Letter::establishConnectionToSocket(int port)
{

    // abort previous sending
    sslSocket.abort();
#ifndef QT_NO_SSL
    if (isEnabledSsl())
    {
        qDebug() << "SSL enabled";
        sslSocket.connectToHostEncrypted(smtpItself, port);
        connect(&sslSocket, SIGNAL(encrypted()), this, SLOT(ready()));
    }
    else
#endif
        sslSocket.connectToHost(smtpItself, port);

    Request = RequestState_Init;


}

void Letter::ready()
{
    qDebug() << "Hand shake succeed!!!";
}

void Letter::send_request(QString line)
{
    blockSize = 0;
//    QDataStream out(&sslSocket);
//    out.setVersion(QDataStream::Qt_4_0);

    log( QString("S: %1").arg(line));
    switch (Request)
    {

        case RequestState_Init :
        {
            if (line.at(0) != '2')
            {
                qDebug() << "Servers answer not 220";
                emit sendingProcessState((int)SERVER_INIT_ERROR, "");

                sslSocket.close();
                break;

            }

            toStream("EHLO user\r\n");

            Request = RequestState_AUTH;
            break;
        }

        case RequestState_AUTH :
        {
            static bool first_attemp = false;

            if (line.at(0) != '2')
            {
                // if server doesn't support EHLO mode we
                // will try old HELO mode
                if (!first_attemp)
                {
                    toStream( "HELO user\r\n");

                    first_attemp = true;
                    Request = RequestState_AUTH;

                    break;
                }

                qDebug() << "Servers answer not 250";
                emit sendingProcessState((int)SERVER_NOT_ANSWER, "");

                sslSocket.close();
                break;

            }

            toStream( "AUTH LOGIN\r\n" );

            Request = RequestState_AutorizeLogin;
            break;
        }

        case RequestState_AutorizeLogin :
        {
            if (line.at(0) != '3')
            {
                qDebug() << "Servers answer not 334";
                emit sendingProcessState((int)AUTH_FAIL, "");

                sslSocket.close();
                break;

            }

            const SmtpAuthData::Data &smtpAuth =
                    smtpData.get_SmtpAuth();

            toStream( encodeToBase64(smtpAuth.login) + "\r\n" );

            Request = RequestState_AutorizePass;
            break;
        }

        case RequestState_AutorizePass :
        {
            if (line.at(0) != '3')
            {
                qDebug() << "Servers answer not 334, bad login";
                emit sendingProcessState((int)INVALID_LOGIN, "");

                sslSocket.close();
                break;

            }

            const SmtpAuthData::Data &smtpAuth =
                    smtpData.get_SmtpAuth();

            toStream ( encodeToBase64(smtpAuth.password) + "\r\n" );

            Request = RequestState_From;
            break;
        }

        case RequestState_From :
        {
            if (line.at(0) != '2')
            {
                qDebug() << "Servers ansver not 235, bad pass";
                emit sendingProcessState((int)PASSWORD_INVALID, "");

                sslSocket.close();
                break;
            }

            const SmtpAuthData::Data &smtpAuth =
                    smtpData.get_SmtpAuth();

            toStream (QString("MAIL FROM: <%1> SIZE=%2\r\n")
                    .arg(smtpAuth.email, QString::number(header.size() + text.size() + 4)));

            Request = RequestState_To;
            break;
        }

        case RequestState_BlindCopy :
        {

            if (!recipientsExist)
            {
                qDebug() << "Servers answer not 250";
                emit sendingProcessState((int)RECIEP_NOT_EXISTS, "");

                sslSocket.close();
                break;

            }

            if (blindReceiversList.isEmpty())
            {
                Request = RequestState_Data;
                send_request(line);
                break;
            }

            static int currentItem = 0;

            if (line.at(0) != '2' && !currentItem)
            {
                qDebug() << "Adress for blind copy not valid";
                emit sendingProcessState((int)BCC_ADDR_INVALID, "");

            }

            toStream (QString("RCPT TO: <%1>\r\n")
                                        .arg(blindReceiversList.at(currentItem).second));
            qDebug() << "Emails to -> " << blindReceiversList.at(currentItem).second;

            if (currentItem >= blindReceiversList.count() - 1)
            {
                currentItem = 0;
                recipientsExist = false;
                Request = RequestState_Data;
                break;
            }

            ++currentItem;

            break;
        }

        case RequestState_Data :
        {

            if (line.at(0) != '2')
            {
                qDebug() << "Addresses for blind copy not valid in case RequestState_Data: ";
                emit sendingProcessState((int)BCC_ADDR_INVALID, "");

            }

            toStream ("DATA\r\n");

            Request = RequestState_Mail;
            break;
        }

        case RequestState_Mail :
        {
            if (line.at(0) != '3')
            {
                qDebug() << "Servers answer not 354 = ";
                emit sendingProcessState((int)SERVER_ABORT_DATA, "");

                sslSocket.close();
                break;

            }

            toStream (header + "\r\n");
            toStream (text + "\r\n.\r\n");

            Request = RequestState_Quit;
            break;
        }

        case RequestState_Quit :
        {
            if (line.at(0) != '2')
            {
                qDebug() << "Servers answer not 250";
                emit sendingProcessState((int)SERVER_ABORT_SEND, "");

                sslSocket.close();
                break;

            }

            toStream ( "QUIT" );
            log("----\n\n");

            // close connection
            sslSocket.close();
            emit sendingProcessState((int)OK, "");
            Request = RequestState_AfterEnd;
            break;
        }

        default :
        {
            qDebug() << "In default case!!!";
            qDebug() << line;
            sslSocket.close();
            break;
        }

    }

}

void Letter::addMoreRecipients(const QString &serverSays)
{
    static int currentItem = 0;

    if (serverSays.at(0) != '2' && !currentItem)
    {
        qDebug() << "Servers answer not 250";
        emit sendingProcessState((int)SERVER_ABORT_FROM, "");

        sslSocket.close();
        return;

    }

    else if (serverSays.at(0) == '2' && currentItem)
        recipientsExist = true;

    if (currentItem >= receiversList.count())
    {
        currentItem = 0;
        Request = RequestState_BlindCopy;
        send_request(serverSays);
        return;
    }

    toStream ( QString("RCPT TO: <%1>\r\n")
                                .arg(receiversList.at(currentItem).second));
    qDebug() << "Emails to -> " << receiversList.at(currentItem).second;

    ++currentItem;

    return;

}

void Letter::on_read()
{
    qDebug() << "Ready read -> " << sslSocket.bytesAvailable();
    QDataStream in(&sslSocket);
    in.setVersion(QDataStream::Qt_4_0);

    if (blockSize == 0)
    {
        if (sslSocket.bytesAvailable() < (int)sizeof(quint16))
            return;

        blockSize = sslSocket.bytesAvailable();
    }

    qDebug() << "Block size -> " << blockSize;

    if (sslSocket.bytesAvailable() < blockSize)
        return;

    QString serverRequest;

    while (sslSocket.canReadLine())
    {
        serverRequest = sslSocket.readLine();
        if (serverRequest.at(3) == ' ')
            break;

    }

    // if we need to add recipients
    if (Request == RequestState_To)
        addMoreRecipients(serverRequest);
    else
        send_request(serverRequest);



    qDebug() << "Server request -> " << serverRequest;


}

void Letter::error_happens(QAbstractSocket::SocketError /*socketError*/)
{
    QAbstractSocket* socket = qobject_cast<QAbstractSocket*>(sender());
    emit sendingProcessState((int)ERROR, socket->errorString());

    qDebug() << socket->errorString();
}

#ifndef QT_NO_SSL
void Letter::sslError_happens(const QList<QSslError> &sslErrors)
{
    foreach (const QSslError& error, sslErrors)
        qDebug() << error.errorString();

}
#endif

Letter::~Letter()
{
    sslSocket.close();
}

void Letter::toStream(const QString &str)
{
    stream << str;
    stream.flush();
    log(QString("C: %1").arg(str));
}
