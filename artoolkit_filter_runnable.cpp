#include "artoolkit_filter_runnable.h"
#include <QDateTime>
#include <QUrl>


ARToolkitFilterRunnable::ARToolkitFilterRunnable(QObject *parent):
    QObject(parent),
    gl(nullptr)
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

    auto handle=input->handleType();




    switch (handle) {
    case QAbstractVideoBuffer::NoHandle: ///< Mainly desktop configuration
        if(input->map(QAbstractVideoBuffer::ReadOnly)){
            auto width=input->width();
            auto height=input->height();
            if(camera_resolution.width()!= width || camera_resolution.height() != height){
                camera_resolution=QSize(width,height);
                detector->setCameraResolution(camera_resolution);
                emit cameraResolutionChanged(camera_resolution);
            }
            if(frame.size()!=width*height){
                frame.resize(width*height);
            }
            if( QVideoFrame::imageFormatFromPixelFormat(pixelFormat)!=QImage::Format_Invalid ){
                QImage img( input->bits(),
                                   input->width(),
                            input->height(),
                            input->bytesPerLine(),
                            QVideoFrame::imageFormatFromPixelFormat(pixelFormat));
                img=img.convertToFormat(QImage::Format_Grayscale8);

                memcpy(frame.data(),img.bits(),sizeof(ARUint8)*input->width()*input->height());
                detector->presentFrame(frame);

            }
            else if (pixelFormat == QVideoFrame::PixelFormat::Format_YUV420P ||
                     pixelFormat == QVideoFrame::PixelFormat::Format_NV21 ||
                     pixelFormat == QVideoFrame::PixelFormat::Format_NV12
                     ) {
                memcpy(frame.data(),input->bits(),sizeof(ARUint8)*input->width()*input->height());
                detector->presentFrame(frame);
            }
            else if(pixelFormat==QVideoFrame::PixelFormat::Format_BGR32) {
                QImage img( input->bits(),
                                   input->width(),
                            input->height(),
                            input->bytesPerLine(),
                            QImage::Format_RGB32);
                img=img.convertToFormat(QImage::Format_Grayscale8);
                memcpy(frame.data(),img.bits(),sizeof(ARUint8)*input->width()*input->height());
                detector->presentFrame(frame);
            }
            else{
                qWarning()<<" Handle: NoHandle, unsupported pixel format:"<<pixelFormat;
            }
            input->unmap();
        }
        else{
            qWarning()<<"Cannot map video buffer";
        }
        break;
    case QAbstractVideoBuffer::GLTextureHandle:
    {
        if(pixelFormat!=QVideoFrame::PixelFormat::Format_BGR32){
            qWarning()<<" Handle: GLTextureHandle, unsupported pixel format:"<<pixelFormat;
        }else{
            auto outputHeight=972;
            auto outputWidth(outputHeight * input->width() / input->height());
            if(camera_resolution.width()!= outputWidth || camera_resolution.height() != outputHeight){
                camera_resolution=QSize(outputWidth,outputHeight);
                detector->setCameraResolution(camera_resolution);
                emit cameraResolutionChanged(camera_resolution);
            }
            if(frame.size()!=outputWidth*outputHeight){
                frame.resize(outputWidth*outputHeight);
            }


            if (gl == nullptr) {

                auto context(QOpenGLContext::currentContext());
                gl = context->extraFunctions();

                auto version(context->isOpenGLES() ? "#version 300 es\n" : "#version 130\n");

                QString vertex(version);
                vertex += R"(
                          out vec2 coord;
                          void main(void) {
                          int id = gl_VertexID;
                          coord = vec2((id << 1) & 2, id & 2);
                          gl_Position = vec4(coord * 2.0 - 1.0, 0.0, 1.0);
                          }
                          )";

                QString fragment(version);

                /*Assuming bgr32 color format*/
                fragment += R"(
                            in lowp vec2 coord;
                            uniform sampler2D image;
                            const lowp vec3 luma = vec3(0.1140, 0.5870,0.2989 );
                            out lowp float fragment;
                            void main(void) {
                            lowp vec3 color = texture(image, coord).rgb;
                            fragment = dot(color, luma);
                            //fragment = pow(fragment,2.0);
                            }
                            )";

                program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex);
                program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragment);
                program.link();
                imageLocation = program.uniformLocation("image");

                gl->glGenRenderbuffers(1, &renderbuffer);
                gl->glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
                gl->glRenderbufferStorage(GL_RENDERBUFFER, QOpenGLTexture::R8_UNorm, outputWidth, outputHeight);
                gl->glBindRenderbuffer(GL_RENDERBUFFER, 0);

                gl->glGenFramebuffers(1, &framebuffer);
                gl->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                gl->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);
                gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);

            }

            gl->glActiveTexture(GL_TEXTURE0);
            gl->glBindTexture(QOpenGLTexture::Target2D, input->handle().toUInt());
            gl->glGenerateMipmap(QOpenGLTexture::Target2D);
            gl->glTexParameteri(QOpenGLTexture::Target2D, GL_TEXTURE_MIN_FILTER, QOpenGLTexture::LinearMipMapLinear);

            program.bind();
            program.setUniformValue(imageLocation, 0);
            program.enableAttributeArray(0);
            gl->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            gl->glViewport(0, 0, outputWidth, outputHeight);
            gl->glDisable(GL_BLEND);
            gl->glDrawArrays(GL_TRIANGLES, 0, 3);

            gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);
            gl->glReadPixels(0, 0, outputWidth, outputHeight, QOpenGLTexture::Red, QOpenGLTexture::UInt8,frame.data());
            detector->presentFrame(frame);
        }
    }
        break;
    default:
        qWarning()<<"Unsupported Video Frame Handle:"<<handle;
        break;

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
