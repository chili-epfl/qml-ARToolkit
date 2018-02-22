#include "artoolkit_filter_runnable.h"
#include <QDateTime>
#include <QUrl>
#include <QImage>

ARToolkitFilterRunnable::ARToolkitFilterRunnable(QObject *parent):
    QObject(parent)
{
    detector=new ARToolKit();

    detector->moveToThread(&detector_thread);

    pause=false;

    connect(&detector_thread, SIGNAL(started()), detector, SLOT(run()));

    connect(detector, SIGNAL(objectsReady(PoseMap)),
            this, SIGNAL(objectsReady(PoseMap)));
    connect(detector, SIGNAL(projectionMatrixChanged(QMatrix4x4)),
            this, SIGNAL(projectionMatrixChanged(QMatrix4x4)));
    connect(detector, SIGNAL(distortionParametersChanged(QVector4D)),
            this, SIGNAL(distortionParametersChanged(QVector4D)));
    connect(this, SIGNAL(cameraResolutionChanged(QSize)),
            detector, SLOT(setCameraResolution(QSize)));

}

ARToolkitFilterRunnable::~ARToolkitFilterRunnable()
{
    stop();
    delete detector;
    detector=NULL;
}

QVideoFrame ARToolkitFilterRunnable::run(QVideoFrame *input,
                                         const QVideoSurfaceFormat &surfaceFormat,
                                         QVideoFilterRunnable::RunFlags flags)
{
    if(pause) return *input;

    timer.start();
    auto pixelFormat=surfaceFormat.pixelFormat();
    if(input->map(QAbstractVideoBuffer::ReadOnly)){
        auto width=input->width();
        auto height=input->height();
        auto planes=input->planeCount();
        if(camera_resolution.width()!= width || camera_resolution.height() != height){
            camera_resolution=QSize(width,height);
            detector->setCameraResolution(camera_resolution);
            emit cameraResolutionChanged(camera_resolution);
        }
        if ((pixelFormat == QVideoFrame::PixelFormat::Format_NV21 ||
                pixelFormat == QVideoFrame::PixelFormat::Format_NV12) && planes==3){

            if(pixelFormat == QVideoFrame::PixelFormat::Format_NV21)
                detector->setPixelFormat(AR_PIXEL_FORMAT_NV21);
            else
                detector->setPixelFormat(AR_PIXEL_FORMAT_420f);
            int buffer_size=input->width()*input->height();
            int half_buffer_size=buffer_size*0.5;
            if(frame.size()!=buffer_size)
                frame.resize(buffer_size);
            if(frame_plane_1.size()!=half_buffer_size)
                frame_plane_1.resize(half_buffer_size);
            if(frame_plane_2.size()!=half_buffer_size)
                frame_plane_2.resize(half_buffer_size);
            memcpy(frame.data(),input->bits(0),sizeof(ARUint8)*buffer_size);
            memcpy(frame_plane_1.data(),input->bits(1),sizeof(ARUint8)*half_buffer_size);
            memcpy(frame_plane_2.data(),input->bits(2),sizeof(ARUint8)*half_buffer_size);
            detector->presentFrame(frame,frame_plane_1,frame_plane_2);
        }
        else if(pixelFormat == QVideoFrame::PixelFormat::Format_YUV420P
                || pixelFormat == QVideoFrame::PixelFormat::Format_YV12
                || pixelFormat == QVideoFrame::PixelFormat::Format_IMC1
                || pixelFormat == QVideoFrame::PixelFormat::Format_IMC2
                || pixelFormat == QVideoFrame::PixelFormat::Format_IMC3
                || pixelFormat == QVideoFrame::PixelFormat::Format_IMC4
                || pixelFormat == QVideoFrame::PixelFormat::Format_Y8){
             detector->setPixelFormat(AR_PIXEL_FORMAT_MONO);
             int buffer_size=input->width()*input->height();
             if(frame.size()!=buffer_size)
                 frame.resize(buffer_size);
             memcpy(frame.data(),input->bits(0),sizeof(ARUint8)*buffer_size);
             detector->presentFrame(frame);
        }
        else if(pixelFormat == QVideoFrame::PixelFormat::Format_ARGB32 ||
                pixelFormat == QVideoFrame::PixelFormat::Format_RGB32){
             detector->setPixelFormat(AR_PIXEL_FORMAT_ARGB);
             int buffer_size=4*input->width()*input->height();
             if(frame.size()!=buffer_size)
                 frame.resize(buffer_size);
             memcpy(frame.data(),input->bits(0),sizeof(ARUint8)*buffer_size);
             detector->presentFrame(frame);
        }
        else if(pixelFormat == QVideoFrame::PixelFormat::Format_RGB24){
             detector->setPixelFormat(AR_PIXEL_FORMAT_RGB);
             int buffer_size=3*input->width()*input->height();
             if(frame.size()!=buffer_size)
                 frame.resize(buffer_size);
             memcpy(frame.data(),input->bits(0),sizeof(ARUint8)*buffer_size);
             detector->presentFrame(frame);
        }
        else if(pixelFormat == QVideoFrame::PixelFormat::Format_BGRA32||
                pixelFormat == QVideoFrame::PixelFormat::Format_BGR32){
             detector->setPixelFormat(AR_PIXEL_FORMAT_BGRA);
             int buffer_size=4*input->width()*input->height();
             if(frame.size()!=buffer_size)
                 frame.resize(buffer_size);
             memcpy(frame.data(),input->bits(0),sizeof(ARUint8)*buffer_size);
             detector->presentFrame(frame);
        }
        else if(pixelFormat == QVideoFrame::PixelFormat::Format_BGR24){
             detector->setPixelFormat(AR_PIXEL_FORMAT_BGR);
             int buffer_size=3*input->width()*input->height();
             if(frame.size()!=buffer_size)
                 frame.resize(buffer_size);
             memcpy(frame.data(),input->bits(0),sizeof(ARUint8)*buffer_size);
             detector->presentFrame(frame);
        }
        else if(pixelFormat == QVideoFrame::PixelFormat::Format_UYVY){
             detector->setPixelFormat(AR_PIXEL_FORMAT_2vuy);
             int buffer_size=2*input->width()*input->height();
             if(frame.size()!=buffer_size)
                 frame.resize(buffer_size);
             memcpy(frame.data(),input->bits(0),sizeof(ARUint8)*buffer_size);
             detector->presentFrame(frame);
        }
        else if(pixelFormat == QVideoFrame::PixelFormat::Format_YUYV){
             detector->setPixelFormat(AR_PIXEL_FORMAT_yuvs);
             int buffer_size=2*input->width()*input->height();
             if(frame.size()!=buffer_size)
                 frame.resize(buffer_size);
             memcpy(frame.data(),input->bits(0),sizeof(ARUint8)*buffer_size);
             detector->presentFrame(frame);
        }
        else if( QVideoFrame::imageFormatFromPixelFormat(pixelFormat)!=QImage::Format_Invalid ){
            QImage img( input->bits(),
                        input->width(),
                        input->height(),
                        input->bytesPerLine(),
                        QVideoFrame::imageFormatFromPixelFormat(pixelFormat));
            img=img.convertToFormat(QImage::Format_ARGB32);
            memcpy(frame.data(),img.bits(),4*sizeof(ARUint8)*input->width()*input->height());
            detector->presentFrame(frame);
        }
        input->unmap();
    }
    return *input;

}

void ARToolkitFilterRunnable::setProjectionMatrix(QMatrix4x4 m)
{
    if(detector)
        detector->setProjectionMatrix(m);
}

void ARToolkitFilterRunnable::start()
{
    detector_thread.start();
}

void ARToolkitFilterRunnable::stop()
{
    if(detector != NULL){
        detector->stop();
    }
    detector_thread.quit();
    detector_thread.wait();
}

void ARToolkitFilterRunnable::setPause(bool val)
{
    if(pause!=val){
        pause=val;
    }
}

void ARToolkitFilterRunnable::setDefaultMarkerSize(qreal size)
{
    if(detector)
        detector->setDefaultMarkerSize(size);
}

void ARToolkitFilterRunnable::loadSingleMarkersConfigFile(QUrl url)
{
    if(detector)
        detector->loadSingleMarkersConfigFile(url);
}

void ARToolkitFilterRunnable::loadMultiMarkersConfigFile(QString config_name, QUrl url)
{
    if(detector)
        detector->loadMultiMarkersConfigFile(config_name,url);
}

void ARToolkitFilterRunnable::setDistortionParameters(QVector4D param)
{
    if(detector)
        detector->setDistortionParameters(param);
}

void ARToolkitFilterRunnable::setMatrixCode(AR_MATRIX_CODE_TYPE code)
{
    if(detector)
        detector->setMatrixCode(code);
}

void ARToolkitFilterRunnable::setLabelingThreshold(int v)
{
    if(detector)
        detector->setLabelingThreshold(v);
}

void ARToolkitFilterRunnable::setFilter_sample_rate(qreal v)
{
    if(detector)
        detector->setFilter_sample_rate(v);
}

void ARToolkitFilterRunnable::setFilter_cutoff_freq(qreal v)
{
    if(detector)
        detector->setFilter_cutoff_freq(v);
}

void ARToolkitFilterRunnable::setFlip_Image(bool val)
{
    if(detector)
        detector->setFlip_Image(val);
}
