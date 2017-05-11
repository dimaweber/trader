QT += core sql
QT -= gui

CONFIG += c++11 warn_on

TARGET = trader
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

win32: {
    INCLUDEPATH += C:/projects/curl/builds/libcurl-vc12-x64-release-dll-ipv6-sspi-winssl/include
    INCLUDEPATH += C:/OpenSSL-Win64/include
         LIBS += -LC:/projects/curl/builds/libcurl-vc12-x64-release-dll-ipv6-sspi-winssl/lib
         LIBS += -LC:/OpenSSL-Win64/lib
}
LIBS += -lssl -lcrypto -lcurl
INCLUDEPATH += ../common ../btce ../database
     LIBS += -L../lib -lcommon -lbtce -ldatabase


SOURCES += daemon.cpp \
    create_close_order.cpp

HEADERS = create_close_order.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin

OTHER_FILES = ../data/trader.ini
