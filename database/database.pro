QT += sql

TEMPLATE = lib

CONFIG +=  c++11 warn_on
#CONFIG += staticlib

LIBS += -L../lib -lcommon

INCLUDEPATH += ../common

HEADERS += \
    tablefield.h \
    sql_database.h

SOURCES += \
    tablefield.cpp \
    sql_database.cpp

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib

RESOURCES += \
    resources.qrc
