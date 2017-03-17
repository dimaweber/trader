QT += core sql
QT -= gui

CONFIG += c++11

INCLUDEPATH += ../common
LIBS += -L../lib -lcommon
LIBS += -lcrypto -lcurl -lreadline

TARGET = db_check
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin
