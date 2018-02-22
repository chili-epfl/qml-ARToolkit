# qml-ARToolkit

qml-ARToolkit is a qml plugin wrapper around ARToolkit v5. Video at <https://www.youtube.com/watch?v=6HB_uRrDi1M>

### Features
- Marker and multimarker tracking (Patter detection not available) 
- Color Detection (when possible, see paragraph about pixel-format conversion);
- Default Detection mode is AR_TEMPLATE_MATCHING_MONO_AND_MATRIX
- Threshold mode is AR_LABELING_THRESH_MODE_MANUAL
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

**IMPORTANT: On some Android platform the image should be mirrored vertically.**
##### Pixel Format Conversion
	
This the table of the current mapping from QVideoFrame::PixelFormat to AR_PIXEL_FORMAT. 
Mirroring vertically is availble only for some formats.
Due to lack of time, we couldn't test if all the conversions are implemented correctly.

| Src (QVideoFrame::PixelFormat) | Dst (AR_PIXEL_FORMAT) | Flip Vertically Available | Tested |
| ------------- |-------------| -----|-----|
| ARGB32| ARGB| true | true |
| RGB32| ARGB| true | true |
| RGB24| RGB| true | false |
| BGRA32| BGRA| true | false |
| BGR32| BGRA| true | false |
| UYVY| 2vuy| false | false |
| YUYV| yuvs| false | false |
| NV12| 420f| false | false |
| NV21| NV21| false | false |
| IM*| MONO| false | true |
| Y8| MONO| false | true |
| YUV420p| MONO| false | true |
| YV12| MONO| false | true |

For the other formats that are convertable to QImage pixel formats (see QVideoFrame::imageFormatFromPixelFormat()), the plugin converts them in QImage with ARGB32 and then to ARGB.

### Build

* Download the sdk [ARToolkit]   
* In the .pro file change INCLUDEPATH and LIBS according to the ARToolkit directory on your system
* (Optional) Add make install in the build step
* (ANDROID) Remeber to add the libs into the apk
    

    ANDROID_EXTRA_LIBS = \
        $$PWD/../../artoolkit5/android/libs/armeabi-v7a/libc++_shared.so \
        $$PWD/../../artoolkit5/android/libs/armeabi-v7a/libARWrapper.so

[ARToolkit]: <http://artoolkit.org/>

### Using ARTags

You will find .png graphic files representing the 64 2D-barcode patterns in the ARToolKit SDK download.
See folder path [ARToolKit SDK path]/doc/patterns/Matrix code 3×3 (72dpi)
