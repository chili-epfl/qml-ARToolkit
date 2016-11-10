TEMPLATE = lib
TARGET = qml-ARToolkit
QT += qml quick multimedia
CONFIG += qt plugin c++11

CONFIG -= android_install

TARGET = $$qtLibraryTarget($$TARGET)
uri = ARToolkit

SOURCES += \
    qml-artoolkit_plugin.cpp \
    artoolkit.cpp \
    artoolkit_filter_runnable.cpp \
    artoolkit_video_filter.cpp \
    artoolkit_object.cpp

HEADERS += \
    qml-artoolkit_plugin.h \
    artoolkit.h \
    artoolkit_video_filter.h \
    artoolkit_filter_runnable.h \
    artoolkit_object.h

DISTFILES = qmldir \
    artoolkit.qmltypes

qmldir.files = qmldir artoolkit.qmltypes


installPath = $$[QT_INSTALL_QML]/$$replace(uri, \\., /)
qmldir.path = $$installPath
target.path = $$installPath
INSTALLS += target qmldir

#Change these paths according to your artoolkit directory
android{
   INCLUDEPATH += /home/chili/ARToolKit5-bin-5.3.2-Android/include
   LIBS+=  -L/home/chili/ARToolKit5-bin-5.3.2-Android/android/libs/armeabi-v7a
   LIBS+=  -lARWrapper
}

!android{
    INCLUDEPATH += /home/chili/ARToolKit5-bin-5.3.2r1-Linux-x86_64/include
    LIBS+=  -L/home/chili/ARToolKit5-bin-5.3.2r1-Linux-x86_64/lib/
    LIBS+= -Wl,-whole-archive -lAR -lARICP -lARMulti -lAR2 -lARUtil -Wl,-no-whole-archive
}

#Define DEBUG_FPS if you want to print the fps
#DEFINES += DEBUG_FPS



#Remember to add make install in the build steps; in android remove the default steps make install and the build apk

#For android projects don't forget to add the libs into the apk
#    ANDROID_EXTRA_LIBS = \
#        $$PWD/../../artoolkit5/android/libs/armeabi-v7a/libc++_shared.so \
#        $$PWD/../../artoolkit5/android/libs/armeabi-v7a/libARWrapper.so
