QT += sql

CONFIG += staticlib c++11 warn_on

TEMPLATE = lib

SOURCES += utils.cpp

HEADERS += utils.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib
