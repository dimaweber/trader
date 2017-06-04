QT -= gui
QT += sql

CONFIG +=  c++11 warn_on
win32: {
 CONFIG += staticlib
}


TEMPLATE = lib

SOURCES += btce.cpp \
http_query.cpp \
    curl_wrapper.cpp

HEADERS += btce.h \
http_query.h \
curl_wrapper.h

win32: {
    INCLUDEPATH += C:/libs/curl/include \
                   C:/libs/openssl/include \
                   c:/libs/boost/include

         LIBS += -LC:/libs/curl/lib \
                 -LC:/libs/openssl/lib \
                 -LC:/libs/boost/lib

         LIBS += -ladvapi32 -luser32 -lcrypt32 -lws2_32
}
LIBS += -lssl -lcrypto -lcurl

INCLUDEPATH += ../common

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib
