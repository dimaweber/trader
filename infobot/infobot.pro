QT += core sql
QT -= gui

CONFIG += c++11 warn_on

TEMAPLTE=app

LIBS  += -lTgBot -L../tgbot-cpp -lboost_system -lpthread -lcrypto -lssl

LIBS += -L../lib -lcommon

INCLUDEPATH += ../tgbot-cpp/include \
              ../common

SOURCES += bot.cpp

HEADERS += ../common/utils.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin
