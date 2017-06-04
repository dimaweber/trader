QT += core sql
QT -= gui

CONFIG += c++11

INCLUDEPATH += ../common ../database
LIBS += -L../lib -lcommon -ldatabase

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

TARGET = db_check
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    db_check.cpp

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc
RCC_DIR = .rcc

DESTDIR = ../bin

RESOURCES += \
    resources.qrc
