QT -= gui
QT += sql

CONFIG +=  c++11 warn_on
CONFIG += staticlib

TEMPLATE = lib

SOURCES += utils.cpp \
key_storage.cpp

#LIBS = -lssl -lcrypto

win32 {
    INCLUDEPATH += C:\OpenSSL-Win64\include
#    LIBS += -LC:\OpenSSL-Win64\lib
}

HEADERS += utils.h \
key_storage.h

!win32 {
    CONFIG += readline
}

readline {
    DEFINES += USE_READLINE
#    LIBS += -lreadline
}

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib
