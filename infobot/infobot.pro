QT += core sql
QT -= gui

CONFIG += c++11 warn_on

TEMPLATE=app

LIBS += -L../lib
LIBS += -ltgbot-cpp -lcommon -ldatabase
LIBS += -lboost_system -lpthread -lcrypto -lssl

win32 {
    INCLUDEPATH += C:/libs/curl/include \
                   C:/libs/openssl/include \
                   c:/libs/boost/include

         LIBS += -LC:/libs/curl/lib \
                 -LC:/libs/openssl/lib \
                 -LC:/libs/boost/lib
}

INCLUDEPATH += ../tgbot-cpp/include \
               ../common \
               ../database

SOURCES += bot.cpp

HEADERS += ../common/utils.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin
