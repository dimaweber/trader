QT += core websockets sql
QT -= gui

CONFIG += c++17

TARGET = bitfinex_client
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    client.cpp \
    ratesdb.cpp \
    btcetradeclient.cpp \
    pusherclient.cpp


win32: {
    INCLUDEPATH += C:/libs/curl/include \
                   C:/libs/openssl/include \
                   c:/libs/boost/include

         LIBS += -LC:/libs/curl/lib \
                 -LC:/libs/openssl/lib \
                 -LC:/libs/boost/lib

         LIBS += -ladvapi32 -luser32 -lcrypt32 -lws2_32
}
INCLUDEPATH += ../common ../btce
LIBS += -L../lib -lcommon -lcrypto -lbtce -lcurl


# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS += \
    client.h \
    ratesdb.h \
    btcetradeclient.h \
    pusherclient.h

DESTDIR = ../bin


OBJECTS_DIR = .obj
UI_DIR = .ui
MOC_DIR = .moc
