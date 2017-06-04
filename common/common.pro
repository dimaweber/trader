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
    INCLUDEPATH += C:/libs/curl/include \
                   C:/libs/openssl/include \
                   c:/libs/boost/include

         LIBS += -LC:/libs/curl/lib \
                 -LC:/libs/openssl/lib \
                 -LC:/libs/boost/lib
}
LIBS += -lssl -lcrypto -lcurl

HEADERS += utils.h \
key_storage.h

!win32: {
    CONFIG += readline
}

readline: {
    DEFINES += USE_READLINE
    LIBS += -lreadline
}

OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc

DESTDIR = ../lib
