TEMPLATE  = lib

CONFIG = c++11 warn_on
#CONFIG += staticlib

SOURCES += mailer/mailer.cpp \
           mailer/smtp.cpp \
           mailer/smtpAuthData.cpp

HEADERS += mailer/mailer.h \
           mailer/smtp.h \
           mailer/smptAuthData.h

QT += network

INCLUDEPATH += mailer
