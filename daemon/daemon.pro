QT += core sql
QT -= gui

CONFIG += c++11 warn_on

TARGET = trader
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

INCLUDEPATH += ../common

LIBS += -lcrypto -lcurl -lreadline
LIBS += -L../lib -lcommon

SOURCES += daemon.cpp \
	key_storage.cpp \
	http_query.cpp \
	btce.cpp \
	tablefield.cpp \
	sql_database.cpp

HEADERS += \
	btce.h \
	http_query.h \
	curl_wrapper.h \
	key_storage.h \
	tablefield.h \
	sql_database.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../bin

OTHER_FILES = ../data/trader.ini
