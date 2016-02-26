QT += core sql
QT -= gui

CONFIG += c++11

TARGET = trader
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

LIBS += -lcrypto -lcurl -lreadline

SOURCES += main.cpp \
    key_storage.cpp \
    utils.cpp \
    http_query.cpp \
    btce.cpp

HEADERS += \
    btce.h \
    utils.h \
    http_query.h \
    curl_wrapper.h \
    key_storage.h
