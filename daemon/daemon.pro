QT += core sql
QT -= gui

CONFIG += c++11 warn_on

TARGET = trader
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

INCLUDEPATH += ../common ../btce ../database

LIBS += -L../lib -lcommon -lbtce -ldatabase

SOURCES += daemon.cpp

HEADERS =

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin

OTHER_FILES = ../data/trader.ini
