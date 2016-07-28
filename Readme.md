# qml-ARToolkit

qml-ARToolkit is a qml plugin wrapper around ARToolkit v5. 

### Features
- Marker and multimarker tracking (Patter detection not available) 
- Greyscale Detection (frames are converted in greyscale images);
- Detection mode is AR_TEMPLATE_MATCHING_MONO_AND_MATRIX
- Threshold mode is AR_LABELING_THRESH_MODE_AUTO_ADAPTIVE
- Pose filtering
- Robust estimation for multipattern configurations

The string id for matrix markers should start with Mat_* (es. Mat_1), whereas for patter markers it should start with Patt_*
Default matrix code is AR_MATRIX_CODE_3x3, with default marker size 50.
Different sizes for different markers can be specified in a json file.
See sample directory. 

The default camera matrix is 

    610, 0 , 320 
    0 , 610 , 240 
    0 , 0 , 1 
assuming a camera resolution of (640,480) and zero distortion coefficients.
The camera matrix and the distortion coefficients are scaled according to the camera resolution UNLESS they have beeen manually specified from the qml object.  

On systems where the frame is a OpenGL texture (Android), the frame is scaled to have a height of 972px.

### Build

* Download and build [ARToolkit]   
* In the .pro file change INCLUDEPATH and LIBS according to the ARToolkit directory on your system
* (Optional) Add make install in the build step
* (ANDROID) Remeber to add the libs into the apk
    

    ANDROID_EXTRA_LIBS = \
        $$PWD/../../artoolkit5/android/libs/armeabi-v7a/libc++_shared.so \
        $$PWD/../../artoolkit5/android/libs/armeabi-v7a/libARWrapper.so

[ARToolkit]: <http://artoolkit.org/>
