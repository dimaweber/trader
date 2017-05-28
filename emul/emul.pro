QT += core sql testlib concurrent
QT -= gui

#CONFIG += c++1z
CONFIG += c++14

TARGET = emul
CONFIG += console
CONFIG -= app_bundle

LIBS += -lfcgi
INCLUDEPATH += ../common ../database ../btce ../decimal_for_cpp/include
LIBS += -L../lib -lcommon -ldatabase -lbtce -lmemcached

LIBS += -lgcov
QMAKE_CXXFLAGS += -fprofile-arcs -ftest-coverage
QMAKE_CFLAGS += -fprofile-arcs -ftest-coverage

TEMPLATE = app

SOURCES += \
    responce.cpp \
    emul.cpp \
    authentificator.cpp \
    unit_tests.cpp \
    sqlclient.cpp \
    memcachedsqldataaccessor.cpp \
    types.cpp

HEADERS += \
    query_parser.h \
    unit_tests.h \
    responce.h \
    fcgi_request.h \
    authentificator.h \
    sqlclient.h \
    types.h \
    memcachedsqldataaccessor.h

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

OTHER_FILES += \
    ../data/emul.ini

DISTFILES += \
    users.csv

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin
