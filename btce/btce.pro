QT -= gui
QT += sql

CONFIG +=  c++11 warn_on
CONFIG += staticlib

TEMPLATE = lib

SOURCES += btce.cpp \
http_query.cpp \
    curl_wrapper.cpp

HEADERS += btce.h \
http_query.h \
curl_wrapper.h

win32: {
    INCLUDEPATH += C:/projects/curl/builds/libcurl-vc12-x64-release-dll-ipv6-sspi-winssl/include
    INCLUDEPATH += C:/OpenSSL-Win64/include
}

INCLUDEPATH += ../common

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib
