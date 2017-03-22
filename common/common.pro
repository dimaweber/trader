QT -= gui
QT += sql

CONFIG +=  c++11 warn_on
#CONFIG += staticlib

TEMPLATE = lib

SOURCES += utils.cpp \
key_storage.cpp

LIBS = -lssl -lcrypto -lreadline

HEADERS += utils.h \
key_storage.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib
