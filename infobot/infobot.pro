QT += core sql
QT -= gui

CONFIG += c++11

TEMAPLTE=app

LIBS  += -lTgBot -L/opt/tgbot/lib -lboost_system -lpthread -lcrypto -lssl

INCLUDEPATH += -I/opt/tgbot/include

SOURCES += bot.cpp \
          ../utils.cpp
