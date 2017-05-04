#ifndef ARUCODETECTORTHREAD_H
#define ARUCODETECTORTHREAD_H


#include <QVideoFilterRunnable>
#include <QDebug>
#include <QElapsedTimer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QThread>
#include "artoolkit.h"
#include <QUrl>


class ARToolkitFilterRunnable: public QObject ,public QVideoFilterRunnable
{
    Q_OBJECT
public:
    ARToolkitFilterRunnable(QObject* parent=0);
    virtual ~ARToolkitFilterRunnable();
    QVideoFrame run(QVideoFrame *input, const QVideoSurfaceFormat &surfaceFormat, RunFlags flags);
    void setProjectionMatrix(QMatrix4x4 m);
    void start();
    void stop();
    void setPause(bool val);
    void setDefaultMarkerSize(qreal size);
    void loadSingleMarkersConfigFile(QUrl url);
    void loadMultiMarkersConfigFile(QString config_name, QUrl url);
    void setDistortionParameters(QVector4D param);
    void setMatrixCode(AR_MATRIX_CODE_TYPE code);
    void setLabelingThreshold(int v);
    void setFilter_sample_rate(qreal v);
    void setFilter_cutoff_freq(qreal v);
    void setFlip_Image(bool val);
signals:
    void cameraResolutionChanged(QSize);
    void projectionMatrixChanged(QMatrix4x4);
    void distortionParametersChanged(QVector4D);
    void objectsReady(const PoseMap&);
private:
    QSize camera_resolution;
    bool pause;
    QThread detector_thread;
    ARToolKit* detector;

    QElapsedTimer timer;
    QByteArray frame;
    QByteArray frame_plane_1;
    QByteArray frame_plane_2;
};



#endif // ARUCODETECTORTHREAD_H
