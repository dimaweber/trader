QT += core sql
QT -= gui

CONFIG += c++1z

TARGET = emulatorClients
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

LIBS += -L../lib -lcommon -lbtce
INCLUDEPATH += ../common ../btce ../decimal_for_cpp/include

SOURCES += main.cpp \
    ../emul/sqlclient.cpp

DEFINES += DEC_NAMESPACE=cppdec

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin
