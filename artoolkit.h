#ifndef ARTOOLKIT_H
#define ARTOOLKIT_H
extern "C"{
#include <AR/ar.h>
#include <AR/config.h>
#include <AR/arFilterTransMat.h>
#include <AR/arMulti.h>
}
#include <QDebug>
#include <QTime>
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QSize>
#include <QMatrix4x4>
#include <QUrl>
#include <QVector2D>
/*
 * A handle can load multiple patterns(up to ARPattHandle->patt_num_max).
 * The pattern id depends on the order of loading of the file.
 * When loading a multimarker configuration that uses patterns, the single patterns are loaded in ARPattHandle.
 * The ids are accessible in the multimarker configuration.
*/


struct Pose{
    QVector3D translation;
    QQuaternion rotation;
    QVector2D TLCorner;
    QVector2D TRCorner;
    QVector2D BRCorner;
    QVector2D BLCorner;
    Pose();
    Pose(const QVector3D& translation, const QQuaternion& rotation,
         const QVector2D& TLCorner=QVector2D(-1,-1),const QVector2D& TRCorner=QVector2D(-1,-1),
         const QVector2D& BRCorner=QVector2D(-1,-1),const QVector2D& BLCorner=QVector2D(-1,-1));
};
typedef QHash<QString,Pose> PoseMap;


struct AR3DObject {
    ARdouble pose[3][4];

    /*In Opengl coordinate system*/
    QQuaternion rotation;
    QVector3D translation;
    /*....*/
    ARFilterTransMatInfo* ftmi;
    bool visible,was_visible;
    qreal size;
    inline AR3DObject& operator=(const AR3DObject& o) {
        size=o.size;
        visible=o.visible;
        was_visible=o.was_visible;
        ftmi=o.ftmi;
        rotation=o.rotation;
        translation=o.translation;
        for(int i=0;i<3;i++)
            for(int j=0;j<4;j++)
                pose[i][j]=o.pose[i][j];
        return *this;
    }
};
struct AR3DMultiPatternObject {
    ARMultiMarkerInfoT* marker_info;
    /*In Opengl coordinate system*/
    QQuaternion rotation;
    QVector3D translation;
    /*....*/
    ARFilterTransMatInfo* ftmi;
    bool visible;
    inline AR3DMultiPatternObject& operator=(const AR3DMultiPatternObject& o) {
        marker_info=o.marker_info;
        rotation=o.rotation;
        translation=o.translation;
        visible=o.visible;
        ftmi=o.ftmi;
        return *this;
    }
};

class ARToolKit: public QObject
{
    Q_OBJECT
public:
    ARToolKit();
    ~ARToolKit();
    void presentFrame(QByteArray &frame);
    void stop();
    void setProjectionMatrix(QMatrix4x4 m);
    void loadSingleMarkersConfigFile(QUrl url);
    void loadMultiMarkersConfigFile(QString config_name, QUrl url);
    void setDistortionParameters(QVector4D param);
    void setMatrixCode(AR_MATRIX_CODE_TYPE code);
    void setLabelingThreshold(int v);
public slots:
    void run();
    void setCameraResolution(QSize size);
    void setDefaultMarkerSize(qreal size);
signals:
    void objectsReady(const PoseMap& poses);
    void projectionMatrixChanged(const QMatrix4x4&);
    void distortionParametersChanged(const QVector4D&);

private:
    void cleanup();    
    QMutex frameLock;                   ///< Mutex that locks the frame transaction
    QWaitCondition nextFrameCond;       ///< Condition to wait on until the next frame arrives

    bool nextFrameAvailable = false;    ///< Whether the next frame is ready and in place
    bool running = false;               ///< Whether the should keep running, we don't need a mutex for this

    QByteArray buffer;

    AR2VideoBufferT* ar_buffer;
    int memory_size;
    ARParamLT* ar_param;
    ARHandle* ar_handle;
    AR3DHandle* ar_3d_handle;
    ARPattHandle* ar_patt_handle;
    AR_PIXEL_FORMAT pixFormat;
    AR_LABELING_THRESH_MODE threshold_mode;
    QMatrix4x4 projectionMatrix;
    bool using_default_projection;
    QVector4D distortionParameters;
    bool using_default_distortion_parameters;

    int labelingThreshold;

    QSize cameraResolution;
    void setupCameraParameters();

    int patter_detection_mode;
    AR_MATRIX_CODE_TYPE code_type;
    void setupMarkerParameters();

    /*Negative index is for pattern, positive for matrix; id is never 0 because ids are stored with -/+1 */
    QHash<int,AR3DObject*> ar_objects;

    QHash<QString,AR3DMultiPatternObject*> ar_multimarker_objects;

    qreal default_marker_size;

};

#endif // ARTOOLKIT_H
