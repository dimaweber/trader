QT -= gui
QT += sql

CONFIG +=  c++11 warn_on
#CONFIG += staticlib

TEMPLATE = lib

SOURCES += btce.cpp \
http_query.cpp \
    curl_wrapper.cpp

HEADERS += btce.h \
http_query.h \
curl_wrapper.h

LIBS = -lcurl -L../lib -lcommon
win32 {
	INCLUDEPATH += C:/projects/curl/builds/libcurl-vc12-x64-release-static-ipv6-sspi-winssl/inlude 
	LIBS += -LC:/projects/curl/builds/libcurl-vc12-x64-release-static-ipv6-sspi-winssl/lib
}
INCLUDEPATH += ../common

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib
