QT       += core gui concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    dfu_bootloader.cpp \
    dfu_ll.cpp \
    hexparser.cpp \
    main.cpp \
    mainwindow.cpp \
    memorymapdialog.cpp

HEADERS += \
    dfu_bootloader.h \
    dfu_ll.h \
    dfu_misc.h \
    hexparser.h \
    mainwindow.h \
    memorymapdialog.h \
    usb_devinfo.h

FORMS += \
    mainwindow.ui \
    memorymapdialog.ui

TRANSLATIONS += \
    STM32_DFU_Bootloader_ru_RU.ts
CONFIG += lrelease
CONFIG += embed_translations

INCLUDEPATH += $$PWD/Libs/Includes/
DEPENDPATH += $$PWD/Libs/Includes/

win32-g++: LIBS += -L$$PWD/Libs/Src/MinGW/ -llibusb-1.0
win32-msvc: LIBS += -L$$PWD/Libs/Src/MSVC/ -llibusb-1.0

CONFIG(release, debug|release) {
    DESTDIR = release_output
    win32-g++ {
        QMAKE_POST_LINK += C:\Qt\6.2.3\mingw_64\bin\windeployqt $$OUT_PWD/$$DESTDIR
    }
    win32-msvc {
        QMAKE_POST_LINK += C:\Qt\6.2.3\msvc2019_64\bin\windeployqt --pdb $$OUT_PWD/$$DESTDIR
    }
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RC_ICONS = DfuSe_Demo.ico
