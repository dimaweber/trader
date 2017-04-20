QT -= gui
QT += sql

TEMPLATE = lib

CONFIG +=  c++11 warn_on
CONFIG += staticlib

LIBS += -L../lib -lcommon

INCLUDEPATH += ../common

HEADERS += \
    tablefield.h \
    sql_database.h

SOURCES += \
    tablefield.cpp \
    sql_database.cpp

win32: {
    INCLUDEPATH += C:/projects/curl/builds/libcurl-vc12-x64-release-dll-ipv6-sspi-winssl/include
    INCLUDEPATH += C:/OpenSSL-Win64/include
         LIBS += -LC:/projects/curl/builds/libcurl-vc12-x64-release-dll-ipv6-sspi-winssl/lib
         LIBS += -LC:/OpenSSL-Win64/lib
}
LIBS += -lssl -lcrypto -lcurl

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib

RESOURCES += \
    resources.qrc
