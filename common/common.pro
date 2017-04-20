QT -= gui
QT += sql

CONFIG +=  c++11 warn_on
win32: {
 CONFIG += staticlib
}


TEMPLATE = lib

SOURCES += utils.cpp \
key_storage.cpp

win32: {
    INCLUDEPATH += C:/projects/curl/builds/libcurl-vc12-x64-release-dll-ipv6-sspi-winssl/include
    INCLUDEPATH += C:/OpenSSL-Win64/include
         LIBS += -LC:/projects/curl/builds/libcurl-vc12-x64-release-dll-ipv6-sspi-winssl/lib
         LIBS += -LC:/OpenSSL-Win64/lib
}
LIBS += -lssl -lcrypto -lcurl

HEADERS += utils.h \
key_storage.h

!win32 {
    CONFIG += readline
}

readline {
    DEFINES += USE_READLINE
    LIBS += -lreadline
}

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib
