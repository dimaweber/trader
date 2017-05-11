QT += core sql
QT -= gui

CONFIG += c++11

INCLUDEPATH += ../common ../database
LIBS += -L../lib -lcommon -ldatabase

win32: {
    INCLUDEPATH += C:/projects/curl/builds/libcurl-vc12-x64-release-dll-ipv6-sspi-winssl/include
    INCLUDEPATH += C:/OpenSSL-Win64/include
         LIBS += -LC:/projects/curl/builds/libcurl-vc12-x64-release-dll-ipv6-sspi-winssl/lib
         LIBS += -LC:/OpenSSL-Win64/lib
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
