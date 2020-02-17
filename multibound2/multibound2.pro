#-------------------------------------------------
#
# Project created by QtCreator 2019-08-19T00:52:01
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QT += qml # required for QJSEngine
QT += webenginewidgets

TARGET = multibound
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

DISTFILES += ../.astylerc

CONFIG += c++17

# use all optimizations that won't generally interfere with debugging
QMAKE_CXXFLAGS_DEBUG += -Og

DEFINES += USE_PAGE_SCRAPING

# automatically build file lists
SOURCES += $$files(*.cpp, true)
HEADERS += $$files(*.h, true) \
           $$files(*.hpp, true)
FORMS += $$files(*.ui, true)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /usr/bin
!isEmpty(target.path): INSTALLS += target

desktop.path = /usr/share/applications
desktop.files += multibound.desktop
INSTALLS += desktop
