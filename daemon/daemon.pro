QT += core sql network
QT -= gui

CONFIG += c++14 warn_on

TARGET = trader
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

win32: {
    INCLUDEPATH += C:/libs/curl/include \
                   C:/libs/openssl/include \
                   c:/libs/boost/include

         LIBS += -LC:/libs/curl/lib \
                 -LC:/libs/openssl/lib \
                 -LC:/libs/boost/lib
}
LIBS += -lssl -lcrypto -lcurl
INCLUDEPATH += ../common ../btce ../database
     LIBS += -L../lib -lcommon -lbtce -ldatabase


SOURCES += daemon.cpp \
    create_close_order.cpp \
    trader.cpp \
    statusserver.cpp

HEADERS = create_close_order.h \
    trader.h \
    statusserver.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin

OTHER_FILES = ../data/trader.ini

DISTFILES += \
    drewqfew
