QT += core sql
QT -= gui

CONFIG += c++11

TEMAPLTE=app

LIBS  += -lTgBot -L/opt/tgbot/lib -lboost_system -lpthread -lcrypto -lssl

LIBS += -L../common -lcommon

INCLUDEPATH += opt/tgbot/include \
              ../common

SOURCES += bot.cpp

HEADERS += ../common/utils.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc
