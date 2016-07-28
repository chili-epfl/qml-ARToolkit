#ifndef ARUCODETECTORTHREAD_H
#define ARUCODETECTORTHREAD_H


#include <QVideoFilterRunnable>
#include<QDebug>
#include <QElapsedTimer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QThread>
#include "artoolkit.h"
#include <QUrl>

typedef QPair<QVector3D,QQuaternion> Pose;
typedef QHash<QString,Pose> PoseMap;

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
    QOpenGLExtraFunctions* gl;
    QOpenGLShaderProgram program;
    GLint imageLocation;
    GLuint framebuffer;
    GLuint renderbuffer;
    QElapsedTimer timer;
    QByteArray frame;
};



#endif // ARUCODETECTORTHREAD_H
