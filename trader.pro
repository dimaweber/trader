QT += core
QT -= gui

CONFIG += c++11

TARGET = trader
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

LIBS += -lcrypto -lcurl

SOURCES += main.cpp
