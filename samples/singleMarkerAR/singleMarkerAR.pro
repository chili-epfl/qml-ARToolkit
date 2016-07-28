TEMPLATE = app

QT += qml quick widgets

CONFIG += c++11

SOURCES += main.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

!android{
LIBS+= -L$$PWD/../../../../artoolkit5/lib -L$$PWD/../../../../artoolkit5/lib/linux-x86_64
}

android{
LIBS+= -L$$PWD/../../../../artoolkit5/android/libs/armeabi-v7a/
}

DISTFILES += \
    single_markers.json

contains(ANDROID_TARGET_ARCH,armeabi-v7a) {
    ANDROID_EXTRA_LIBS = \
        $$PWD/../../../../artoolkit5/android/libs/armeabi-v7a/libc++_shared.so\
        $$PWD/../../../../artoolkit5/android/libs/armeabi-v7a/libARWrapper.so
}

