#include "spammer.h"
#include "mailer.h"
#include <QDebug>
#include <QTimer>
#include <QDir>
#include <iostream>

Spammer::Spammer( QObject *parent) :
    QObject(parent)
{
    pTimer = new QTimer(this);
    pMailer = new Mailer(this);
    pMailer->addSmtpServer("smtp.gmail.com",      "465", true,  "diepress@gmail.com",   "Dmitry (gmail)",      "diepress@gmail.com", "0w438tb8T56O");
    pMailer->addSmtpServer("mail.softwarium.net", "25",  false, "weber@softwarium.net", "Dmitry (softwarium)", "weber",              "g00dd1e");
    pMailer->addSmtpServer("smtp.mail.ru",        "25",  false, "diepress@mail.ru",     "Dmitry (mailru)",     "diepress",           "megiddo");

    connect (pTimer, SIGNAL(timeout()), SLOT(onTimer()));
    connect (pMailer, SIGNAL(sentResult(QString)), SLOT(onSend(QString)));
}

void Spammer::spam(int count, int delay)
{
    emailsSent  = 0;
    emailsToSend = count;
    qDebug() << "spam start";
    pTimer->start(delay);
}

void Spammer::onTimer()
{
        pMailer->send(emailsSent, "weber@softwarium.net", getRandomSubject(), getRandomMessage(), getRandomAttach(), -1);

        if (emailsSent == emailsToSend)
        {
            pTimer->stop();
            qDebug() << "spam finish";
        }
}

void Spammer::onSend(const QString &res)
{
    std::cout << res.toUtf8().data() << std::endl;
    if (!--emailsToSend)
    {
        qDebug() << "all mails responded";
        emit done();
    }
}


QString Spammer::getRandomSubject()
{
    switch (qrand() % 10)
    {
    case 0:
        return "10. Jane Eyre, Charlotte Brontë";
    case 1:
        return "1. The Lord of the Rings, JRR Tolkien";
    case 2:
        return "2. Pride and Prejudice, Jane Austen";
    case 3:
        return "3. His Dark Materials, Philip Pullman";
    case 4:
        return "4. The Hitchhiker's Guide to the Galaxy, Douglas Adams";
    case 5:
        return "5. Harry Potter and the Goblet of Fire, JK Rowling";
    case 6:
        return "6. To Kill a Mockingbird, Harper Lee";
    case 7:
        return "7. Winnie the Pooh, AA Milne";
    case 8:
        return "8. Nineteen Eighty-Four, George Orwell";
    case 9:
        return "9. The Lion, the Witch and the Wardrobe, CS Lewis";
    default:
        return "11. Catch-22, Joseph Heller";
    }
    return QString::null;
}

QString Spammer::getRandomMessage()
{
    QString message = "--- this is test message todetect issue: ----\n";
    switch (qrand() % 10)
    {
    case 0:
        message += "This planet has — or rather had — a problem, which was this: most of the people living on it were unhappy for pretty much all of the time. Many solutions were suggested for this problem, but most of these were largely concerned with the movement of small green pieces of paper, which was odd because on the whole it wasn't the small green pieces of paper that were unhappy.";
        break;
    case 1:
        message += "Many were increasingly of the opinion that they'd all made a big mistake in coming down from the trees in the first place. And some said that even the trees had been a bad move, and that no one should ever have left the oceans.";
        break;
    case 2:
        message += "In many of the more relaxed civilizations on the Outer Eastern Rim of the Galaxy, the Hitchhiker's Guide has already supplanted the great Encyclopaedia Galactica as the standard repository of all knowledge and wisdom, for though it has many omissions and contains much that is apocryphal, or at least wildly inaccurate, it scores over the older, more pedestrian work in two important respects."
                "First, it is slightly cheaper; and secondly it has the words DON'T PANIC inscribed in large friendly letters on its cover.";
        break;
    case 3:
        message += "But the plans were on display . . .\n"
                "On display? I eventually had to go down to the cellar to find them.\n"
                "That's the display department.\n"
                "With a torch.\n"
                "Ah, well the lights had probably gone.\n"
                "So had the stairs.\n"
                "But look, you found the notice, didn't you?\n"
                "Yes,\" said Arthur, \"yes I did. It was on display in the bottom of a locked filing cabinet stuck in a disused lavatory with a sign on the door saying Beware of the Leopard.\"";
        break;
    case 4:
        message += "The mere thought,\" growled Mr. Prosser, \"hadn't even begun to speculate,\" he continued, settling himself back, \"about the merest possibility of crossing my mind.\"";
        break;
    case 5:
        message += "[The Guide] says that the best drink in existence is the Pan Galactic Gargle Blaster. It says that the effect of a Pan Galactic Gargle Blaster is like having your brains smashed out by a slice of lemon wrapped round a large gold brick.";
        break;
    case 6:
    case 7:
    case 8:
    case 9:
        return "\"Some factual information for you. Have you any idea how much damage that bulldozer would suffer if I just let it roll straight over you?\n"
                "\"How much?\" said Arthur.\n"
                "\"None at all,\" said Mr. Prosser.";
    }
    message += "\n ---------------------------------";
    return message;
}

QStringList Spammer::getRandomAttach()
{
    QStringList out;
    QDir dir("/home/weber/Pictures/futurama");
    QStringList entries = dir.entryList(QStringList() << "*.png");
    for (int i=0; i< qrand() % 3; i++)
    {
        int index = qrand() % entries.count();
        out.append(dir.absoluteFilePath(entries[index]));
    }
    return out;
}
