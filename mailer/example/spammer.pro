QT += network

include (mailer.pri)

HEADERS +=  spammer.h

SOURCES += main.cpp \
           spammer.cpp

DEFINES += QT_NO_DEBUG_OUTPUT
