#-------------------------------------------------
#
# Project created by QtCreator 2015-07-14T19:08:34
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qaccelerator
TEMPLATE = app


SOURCES +=\
        qaccelerator.cc \
    categorizer.cc \
    download-dialog.cc \
    download-monitor.cc \
    download-monitor-page.cc \
    downloads-table.cc \
    fetcher.cc \
    main.cc \
    preferences-dialog.cc \
    speed-grapher.cc \
    spinner.cc \
    qaccelerator-db.cc \
    qaccelerator-utils.cc

HEADERS  += qaccelerator.h \
    categorizer.h \
    download-dialog.h \
    download-monitor.h \
    download-monitor-page.h \
    downloads-table.h \
    fetcher.h \
    preferences-dialog.h \
    speed-grapher.h \
    spinner.h \
    qaccelerator-db.h \
    qaccelerator-utils.h \
    version.h

CONFIG += c++11

QT += network
QT += sql

RESOURCES = images.qrc

win32:RC_ICONS += images/qx_flash.ico
