#-------------------------------------------------
#
# Project created by QtCreator 2014-01-25T23:17:50
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = genesis
TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++11 -Wno-unused-parameter


SOURCES += \
    src/mainwindow.cpp \
    src/main.cpp \
    src/lv2selectorwindow.cpp \
    src/soundmodule.cpp \
    src/soundengine.cpp

HEADERS  += \
    src/mainwindow.h \
    src/lv2selectorwindow.h \
    src/soundmodule.h \
    src/soundengine.h

FORMS    += \
    src/mainwindow.ui \
    src/lv2selectorwindow.ui

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += lilv-0


unix: PKGCONFIG += portaudio-2.0
