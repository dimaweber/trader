#include <QCoreApplication>
#include <QTimer>
#include "spammer.h"

int main (int argc, char* argv[])
{
    QCoreApplication a(argc, argv);
    Spammer spammer;
    a.connect (&spammer, SIGNAL(done()), SLOT(quit()));
    int count = 10;
    int delay = 5000;
    if (argc>1)
        count = QString(argv[1]).toInt();
    if (argc>2)
        delay = QString(argv[2]).toInt();
    spammer.spam(count, delay);

    return a.exec();
}
