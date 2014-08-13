#-------------------------------------------------
#
# Project created by QtCreator 2014-08-13T16:16:38
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = Simple2Var
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    task.cpp

unix:!macx: LIBS += -L$$PWD/../../../libFactorGraph/Debug/ -lFactorGraph

INCLUDEPATH += $$PWD/../../../libFactorGraph/libFactorGraph
DEPENDPATH += $$PWD/../../../libFactorGraph/libFactorGraph

HEADERS += \
    task.h
