#-------------------------------------------------
#
# Project created by QtCreator 2013-10-09T20:15:07
#
#-------------------------------------------------

QT       += core gui serialport widgets

TARGET = emsload
TEMPLATE = app

#include(serialport/apmserial.pri)
win32 { #Windows based mingw build
        message("Building for win32")
        DEFINES += GIT_COMMIT=$$system(\"c:/program files (x86)/git/bin/git.exe\" describe --dirty=-DEV --always)
        DEFINES += GIT_HASH=$$system(\"c:/program files (x86)/git/bin/git.exe\" log -n 1 --pretty=format:%H)
        QMAKE_LFLAGS += -static-libgcc -static-libstdc++
} else:mac {
        message("Building for OSX")
        INCLUDEPATH += /opt/local/include
        LIBS += -L/opt/local/lib
        DEFINES += GIT_COMMIT=$$system(git describe --dirty=-DEV --always)
        DEFINES += GIT_HASH=$$system(git log -n 1 --pretty=format:%H)
        DEFINES += GIT_DATE=\""$$system(date)"\"
} else:unix {
        message("Building for *nix")
        isEmpty($$PREFIX) {
                PREFIX = /usr/local
                message("EMSLoad using default install prefix " $$PREFIX)
        } else {
                message("EMSLoad using custom install prefix " $$PREFIX)
        }
        DEFINES += INSTALL_PREFIX=$$PREFIX
        target.path = $$PREFIX/bin
        INSTALLS += target
        DEFINES += GIT_COMMIT=$$system(git describe --dirty=-DEV --always)
        DEFINES += GIT_HASH=$$system(git log -n 1 --pretty=format:%H)
        DEFINES += GIT_DATE=\""$$system(date)"\"
}
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
