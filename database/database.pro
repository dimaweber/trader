QT -= gui
QT += sql

TEMPLATE = lib

CONFIG +=  c++11 warn_on
win32: {
 CONFIG += staticlib
}


LIBS += -L../lib -lcommon

INCLUDEPATH += ../common

HEADERS += \
    tablefield.h \
    sql_database.h

SOURCES += \
    sql_database.cpp

win32: {
    INCLUDEPATH += C:/libs/curl/include \
                   C:/libs/openssl/include \
                   c:/libs/boost/include

         LIBS += -LC:/libs/curl/lib \
                 -LC:/libs/openssl/lib \
                 -LC:/libs/boost/lib
         LIBS += -ladvapi32 -luser32 -lcrypt32 -lws2_32
}
LIBS += -lssl -lcrypto -lcurl

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc
RCC_DIR = .rcc

DESTDIR = ../lib

RESOURCES += \
    resources.qrc
