#ifndef ARUCODETECTOR_H
#define ARUCODETECTOR_H

#include <QtMultimedia>
#include <QQuickItem>
#include <QUrl>
#include <QVector4D>
#include "artoolkit_filter_runnable.h"
#include "artoolkit_object.h"
extern "C"{
#include <AR/ar.h>
}
class ARToolKitVideoFilter : public QAbstractVideoFilter
{
    Q_OBJECT
    Q_ENUMS(MATRIX_CODE_TYPE)
    Q_PROPERTY(QMatrix4x4 projectionMatrix READ getProjectionMatrix WRITE setProjectionMatrix NOTIFY projectionMatrixChanged)
    Q_PROPERTY(QVector4D distortionParameters READ distortionParameters WRITE setDistortionParameters NOTIFY distortionParametersChanged)
    Q_PROPERTY(QSize cameraResolution READ cameraResolution NOTIFY cameraResolutionChanged)
    Q_PROPERTY(bool pause READ pause WRITE setPause NOTIFY pauseChanged)
    Q_PROPERTY(qreal defaultMarkerSize READ defaultMarkerSize WRITE setDefaultMarkerSize NOTIFY defaultMarkerSizeChanged)
    Q_PROPERTY(MATRIX_CODE_TYPE matrixCode READ matrixCode WRITE setMatrixCode NOTIFY matrixCodeChanged)
    Q_PROPERTY(QVariantList detectedMarkers READ detectedMarkers NOTIFY detectedMarkersChanged)
    Q_PROPERTY(int labelingThreshold READ labelingThreshold WRITE setLabelingThreshold NOTIFY labelingThresholdChanged)
public:
    enum MATRIX_CODE_TYPE {
        MATRIX_CODE_3x3= AR_MATRIX_CODE_3x3,                                        // Matrix code in range 0-63.
        MATRIX_CODE_3x3_PARITY65=AR_MATRIX_CODE_3x3_PARITY65,                       // Matrix code in range 0-31.
        MATRIX_CODE_3x3_HAMMING63=AR_MATRIX_CODE_3x3_HAMMING63,                     // Matrix code in range 0-7.
        MATRIX_CODE_4x4=AR_MATRIX_CODE_4x4 ,                                        // Matrix code in range 0-8191.
        MATRIX_CODE_4x4_BCH_13_9_3= AR_MATRIX_CODE_4x4_BCH_13_9_3,                  // Matrix code in range 0-511.
        MATRIX_CODE_4x4_BCH_13_5_5= AR_MATRIX_CODE_4x4_BCH_13_5_5,                  // Matrix code in range 0-31.
        MATRIX_CODE_5x5_BCH_22_12_5= AR_MATRIX_CODE_5x5_BCH_22_12_5,                // Matrix code in range 0-4095.
        MATRIX_CODE_5x5_BCH_22_7_7= AR_MATRIX_CODE_5x5_BCH_22_7_7 ,                 // Matrix code in range 0-127.
        MATRIX_CODE_5x5 = AR_MATRIX_CODE_5x5 ,                                      // Matrix code in range 0-4194303.
        MATRIX_CODE__6x6= AR_MATRIX_CODE_6x6                                        // Matrix code in range 0-8589934591.
    };
    ARToolKitVideoFilter(QQuickItem *parent = 0);
    ~ARToolKitVideoFilter();
    QVideoFilterRunnable *createFilterRunnable();

    QMatrix4x4 getProjectionMatrix() const;
    void setProjectionMatrix(QMatrix4x4);

    QVector4D distortionParameters(){return m_distortionParameters;}
    void setDistortionParameters(QVector4D params);
    QSize cameraResolution() const;

    Q_INVOKABLE void loadSingleMarkersConfigFile(QUrl url);
    Q_INVOKABLE void loadMultiMarkersConfigFile(QString config_name,QUrl url);

    Q_INVOKABLE void registerObserver(ARToolKitObject* o);

    bool pause(){return m_pause;}
    void setPause(bool val);
    qreal defaultMarkerSize(){return m_default_marker_size;}
    void setDefaultMarkerSize(qreal size);
    MATRIX_CODE_TYPE matrixCode(){return m_matrix_code;}
    void setMatrixCode(MATRIX_CODE_TYPE code);
    QVariantList detectedMarkers(){return m_detected_markers;}
    int labelingThreshold(){return m_labeling_threshold;}
    void setLabelingThreshold(int v);
signals:
    void projectionMatrixChanged();
    void pauseChanged();
    void cameraResolutionChanged();
    void defaultMarkerSizeChanged();
    void distortionParametersChanged();
    void matrixCodeChanged();
    void detectedMarkersChanged();
    void labelingThresholdChanged();
public slots:

    Q_INVOKABLE void unregisterObserver(ARToolKitObject* o);

private slots:

    /*Slot used when the matrix updated comes from the filter_runnable (ex. auto-calibration)*/
    void setProjectionMatrix_private(QMatrix4x4 m);
    void setCameraResolution(QSize);
    void unregisterDestroyedObserver(QObject* o);
    void updateObserver(QString prevObjId);
    void notifyObservers(const PoseMap& poses);
    void cleanFilter();

    void setDistortionParameters_private(QVector4D params);
private:
    QMultiHash<QString, ARToolKitObject*> m_observers;
    qreal m_default_marker_size;
    bool m_pause;
    MATRIX_CODE_TYPE m_matrix_code;
    QSize m_cameraResolution;
    /*This is actually a 3x3 matrix...*/
    QMatrix4x4 m_projectionMatrix;
    bool m_using_default_projection;

    QVector4D m_distortionParameters;
    bool m_using_default_distortion_parameters;

    QUrl m_single_markers_config_file_url;
    QUrl m_multi_markers_config_file_url;
    QString m_multi_marker_config_name;

    ARToolkitFilterRunnable* m_filter_runnable;
    QVariantList m_detected_markers;
    int m_labeling_threshold;
};

#endif // ARUCODETECTOR_H
