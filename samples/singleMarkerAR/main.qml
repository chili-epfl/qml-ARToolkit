import QtQuick 2.6
import QtQuick.Controls 1.5
import ARToolkit 1.0
import QtMultimedia 5.5
import QtQuick.Scene3D 2.0

ApplicationWindow {
    visible: true
    title: qsTr("Hello World")
    width: 640
    height: 480
    Camera{
        id:cameraDev
        viewfinder.resolution: "640x480"
        deviceId: QtMultimedia.availableCameras[1].deviceId
        imageProcessing.whiteBalanceMode: CameraImageProcessing.WhiteBalanceAuto
        focus.focusMode: CameraFocus.FocusAuto + CameraFocus.FocusContinuous
        focus.focusPointMode: CameraFocus.FocusPointAuto
        captureMode: Camera.CaptureViewfinder
        //exposure.exposureMode: CameraExposure.ExposureBarcode
        //exposure.exposureCompensation:-0.5
        Component.onCompleted: {
            console.log(cameraDev.focus.isFocusModeSupported(CameraFocus.FocusAuto))
            console.log(cameraDev.focus.isFocusModeSupported(CameraFocus.FocusContinuous))
            console.log(cameraDev.focus.isFocusModeSupported(CameraFocus.FocusHyperfocal))
            console.log(cameraDev.focus.isFocusModeSupported(CameraFocus.FocusInfinity))
            console.log(cameraDev.focus.isFocusPointModeSupported(CameraFocus.FocusPointAuto))
        }

    }
    Timer{
        running: true
        interval: 10000
        onTriggered: {cameraDev.unlock();cameraDev.searchAndLock()}
        repeat: true
    }
    VideoOutput{
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
        source: cameraDev
        filters: [artoolkit]
    }
    ARToolkit{
        id:artoolkit
        matrixCode: ARToolkit.MATRIX_CODE_4x4_BCH_13_9_3
        Component.onCompleted: {
            artoolkit.loadSingleMarkersConfigFile("qrc:/single_markers.json");
            artoolkit.loadMultiMarkersConfigFile("MultiMarker","qrc:/MultiMarkerConfig.dat");
        }
    }
    ARToolkitObject{
        objectId: "Mat_36"
        id:ar_obj
        Component.onCompleted: artoolkit.registerObserver(ar_obj)
        onObjectIsVisibleChanged: console.log(objectIsVisible)
    }
    ARToolkitObject{
        objectId: "MultiMarker"
        id:ar_obj_multi
        Component.onCompleted: artoolkit.registerObserver(ar_obj_multi)
        onObjectIsVisibleChanged: console.log(objectIsVisible)
    }
    Scene3D{
        anchors.fill: parent;
        Scene{

        }

    }



}
