QT += core sql
QT -= gui

CONFIG += c++11 warn_on

TEMPLATE=app

LIBS += -L../lib
LIBS += -ltgbot-cpp -lcommon -ldatabase
LIBS += -lboost_system -lpthread -lcrypto -lssl

INCLUDEPATH += ../tgbot-cpp/include \
               ../common \
               ../database

SOURCES += bot.cpp

HEADERS += ../common/utils.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin
