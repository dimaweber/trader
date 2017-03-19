QT += core sql
QT -= gui

CONFIG += c++11

INCLUDEPATH += ../common ../database
LIBS += -L../lib -lcommon -ldatabase

TARGET = db_check
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    db_check.cpp

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin

RESOURCES += \
    resources.qrc
