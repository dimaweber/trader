QT += core sql
QT -= gui

CONFIG += c++11 warn_on

TEMPLATE=app

LIBS += -L../lib -L../tgbot-cpp
LIBS += -ltgbot-cpp -lcommon
LIBS += -lboost_system -lpthread -lcrypto -lssl

INCLUDEPATH += ../tgbot-cpp/include \
               ../common

SOURCES += bot.cpp

HEADERS += ../common/utils.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin
