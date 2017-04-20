QT += core network
QT -= gui

TEMPLATE  = lib

CONFIG += c++11 warn_on
CONFIG += staticlib

SOURCES += mailer.cpp \
           smtp.cpp \
           smtpAuthData.cpp

HEADERS += mailer.h \
           smtp.h \
           smptAuthData.h

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib
