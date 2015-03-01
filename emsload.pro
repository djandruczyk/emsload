#-------------------------------------------------
#
# Project created by QtCreator 2013-10-09T20:15:07
#
#-------------------------------------------------

QT       += core gui serialport widgets

TARGET = emsload
TEMPLATE = app

#include(serialport/apmserial.pri)

SOURCES += main.cpp\
        mainwindow.cpp \
    loaderthread.cpp \
    s19file.cpp \
    serialmonitor.cpp

HEADERS  += mainwindow.h \
    loaderthread.h \
    s19file.h \
    serialmonitor.h

FORMS    += mainwindow.ui
