#include "artoolkit.h"
#include <QDebug>
#include <QHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
//#include <AR/video.h>

#ifdef DEBUG_FPS
#include <QElapsedTimer>
const float FPS_RATE = 0.9f;            ///< Rate of using the older FPS estimates
const int FPS_PRINT_PERIOD = 500;       ///< Period of printing the FPS estimate, in milliseconds
#endif

inline void do_flip_mono_plane(QByteArray& src, int w, int h, int depth)
{
    const int data_bytes_per_line = w * (depth / 8);
    uint *srcPtr = reinterpret_cast<uint *>(src.data());
    uint *dstPtr = reinterpret_cast<uint *>(src.data() + (h - 1) * data_bytes_per_line);
    h = h / 2;
    const int uint_per_line = (data_bytes_per_line + 3) >> 2; // bytes per line must be a multiple of 4
    for (int y = 0; y < h; ++y) {
        // This is auto-vectorized, no need for SSE2 or NEON versions:
        for (int x = 0; x < uint_per_line; x++) {
            const uint d = dstPtr[x];
            const uint s = srcPtr[x];
            dstPtr[x] = s;
            srcPtr[x] = d;
        }
        srcPtr += data_bytes_per_line >> 2;
        dstPtr -= data_bytes_per_line >> 2;
    }
}


ARToolKit::ARToolKit()
{
    labelingThreshold=AR_DEFAULT_LABELING_THRESH;

    default_marker_size=50;
    using_default_projection=true;
    pixFormat=AR_PIXEL_FORMAT_MONO;
    //threshold_mode=AR_LABELING_THRESH_MODE_AUTO_ADAPTIVE;
    threshold_mode=AR_LABELING_THRESH_MODE_MANUAL;
    cameraResolution=QSize(640,480);

    ar_3d_handle=NULL;
    ar_handle=NULL;
    ar_param=NULL;
    ar_patt_handle=NULL;

    patter_detection_mode=AR_TEMPLATE_MATCHING_MONO_AND_MATRIX;
    code_type=AR_MATRIX_CODE_3x3;

    ar_buffer=(AR2VideoBufferT*)malloc(sizeof(AR2VideoBufferT));
    ar_buffer->buff=NULL;
    ar_buffer->bufPlanes=NULL;
    ar_buffer->bufPlaneCount=0;

    projectionMatrix=QMatrix4x4(
                610, 0., 640/2 ,0,
                0., 610, 480/2,0,
                0,              0,              1,  0,
                0,              0,              0,  1
                );
    distortionParameters=QVector4D(0,0,0,0);
    using_default_distortion_parameters=true;
    m_cutoff_freq=AR_FILTER_TRANS_MAT_CUTOFF_FREQ_DEFAULT;
    m_sample_freq=AR_FILTER_TRANS_MAT_SAMPLE_RATE_DEFAULT;

    m_flip_image=false;

    setupCameraParameters();
    setupMarkerParameters();
}

ARToolKit::~ARToolKit()
{
    Q_FOREACH(int id, ar_objects.keys()){
        arFilterTransMatFinal(ar_objects[id]->ftmi);
        delete ar_objects[id];
    }
    Q_FOREACH(QString id, ar_multimarker_objects.keys()){
        arFilterTransMatFinal(ar_multimarker_objects[id]->ftmi);
        arMultiFreeConfig(ar_multimarker_objects[id]->marker_info);
        delete ar_multimarker_objects[id];
    }
    cleanup();
    if(planes_size[0]!=0)
        free(ar_buffer->buff);
    if(planes_size[1]!=0)
        free(ar_buffer->bufPlanes[1]);
    if(planes_size[2]!=0)
        free(ar_buffer->bufPlanes[2]);
    if(ar_buffer->bufPlanes!=NULL)
        free(ar_buffer->bufPlanes);
}

void ARToolKit::run()
{
    running=true;
#ifdef DEBUG_FPS
    QElapsedTimer timer;
    float fps = 0.0f;
    long millisElapsed = 0;
    long millis;
    timer.start();
#endif

//    frameLock.lock();
    int marker_num;
    ARMarkerInfo* marker_info;
    QHash<int, ARMarkerInfo> current_markers;
    ARdouble new_pose[3][4];
    QMatrix3x3 rotMat;
    PoseMap posemap;
    qreal err;
    QQuaternion openglAlignment=QQuaternion::fromAxisAndAngle(1,0,0,180);
    while(running){
        frameLock.lock();
        if (nextFrameAvailable) {
            nextFrameAvailable=false;

            if(planes_size[0]!=buffer.size()){
                if(planes_size[0]!=0){
                    free(ar_buffer->buff);
                    ar_buffer->buff=NULL;
                }
                if(buffer.size()>0)
                    ar_buffer->buff=(ARUint8*)malloc(sizeof(ARUint8)*buffer.size());
                planes_size[0]=buffer.size();
            }
            if(m_flip_image){
                switch (pixFormat) {
                case AR_PIXEL_FORMAT_RGB:
                case AR_PIXEL_FORMAT_BGR:
                    do_flip_mono_plane(buffer,cameraResolution.width(),cameraResolution.height(),24);
                    break;
                case AR_PIXEL_FORMAT_BGRA:
                case AR_PIXEL_FORMAT_ARGB:
                    do_flip_mono_plane(buffer,cameraResolution.width(),cameraResolution.height(),32);
                    break;
                case AR_PIXEL_FORMAT_MONO:
                    do_flip_mono_plane(buffer,cameraResolution.width(),cameraResolution.height(),8);
                    break;
                default:
                    break;
                }
            }
            memcpy(ar_buffer->buff,buffer.constData(),sizeof(ARUint8)*buffer.size());

            if(buffer_plane_1.size()==0 && buffer_plane_2.size()==0){
                if(ar_buffer->bufPlanes!=NULL){
                    if(ar_buffer->bufPlanes[1]!=NULL)
                        free(ar_buffer->bufPlanes[1]);
                    if(ar_buffer->bufPlanes[2]!=NULL)
                        free(ar_buffer->bufPlanes[2]);
                    free(ar_buffer->bufPlanes);
                    ar_buffer->bufPlaneCount=0;
                    ar_buffer->bufPlanes=NULL;
                }
            }
            else{
                ar_buffer->bufPlaneCount=3;

                if(ar_buffer->bufPlanes==NULL)
                    ar_buffer->bufPlanes=(ARUint8**)malloc(sizeof(ARUint8*)*3);

                ar_buffer->bufPlanes[0]=ar_buffer->buff;

                if(planes_size[1]!=buffer_plane_1.size()){
                    if(planes_size[1]!=0){
                        free(ar_buffer->bufPlanes[1]);
                        ar_buffer->bufPlanes[1]=NULL;
                    }
                    if(buffer_plane_1.size()>0)
                        ar_buffer->bufPlanes[1]=(ARUint8*)malloc(sizeof(ARUint8)*buffer_plane_1.size());
                    planes_size[1]=buffer_plane_1.size();
                }

                if(planes_size[2]!=buffer_plane_2.size()){
                    if(planes_size[2]!=0){
                        free(ar_buffer->bufPlanes[2]);
                        ar_buffer->bufPlanes[2]=NULL;
                    }
                    if(buffer_plane_2.size()>0)
                        ar_buffer->bufPlanes[2]=(ARUint8*)malloc(sizeof(ARUint8)*buffer_plane_2.size());
                    planes_size[2]=buffer_plane_2.size();
                }
                memcpy(ar_buffer->bufPlanes[1],buffer_plane_1.constData(),sizeof(ARUint8)*buffer_plane_1.size());
                memcpy(ar_buffer->bufPlanes[2],buffer_plane_2.constData(),sizeof(ARUint8)*buffer_plane_2.size());
            }

            frameLock.unlock();
            ar_buffer->fillFlag=1;
            ar_buffer->time_sec=(ARUint32)QDateTime::currentMSecsSinceEpoch()/1000;
            ar_buffer->time_usec=(ARUint32)QDateTime::currentMSecsSinceEpoch()-ar_buffer->time_sec*1000;
//          ar_buffer->buffLuma=ar_buffer->buff;
//            QImage img(ar_buffer->buff,cameraResolution.width(),cameraResolution.height(),QImage::Format_Grayscale8);
//            img.save(QString(getenv("EXTERNAL_STORAGE"))+"/Pictures/"+QString::number(rand()).append(".png"));
           // Detect the markers in the video frame.
            if (arDetectMarker(ar_handle, ar_buffer->buff) < 0) {
                qDebug()<<"Error in arDetectMarker";
                continue;
            }
            current_markers.clear();
            posemap.clear();
            marker_num=arGetMarkerNum(ar_handle);

            Q_FOREACH(int id,ar_objects.keys())
                ar_objects[id]->visible=false;

            if(marker_num>0){
                marker_info=arGetMarker(ar_handle);
                /*Pick best markers based on confidence*/
                for(int i=0;i<marker_num;i++){
                    int id=0;
                    short markerType=0;//-1 patt, 1 mat
                    if(patter_detection_mode!=AR_MATRIX_CODE_DETECTION && marker_info[i].idPatt>=0){
                        if(marker_info[i].cfPatt < 0.25 )
                            continue;
                        id=marker_info[i].idPatt+1;
                        markerType=-1;
                    }
                    else if(patter_detection_mode!=AR_TEMPLATE_MATCHING_COLOR &&
                            patter_detection_mode!=AR_TEMPLATE_MATCHING_MONO
                            && marker_info[i].idMatrix>=0){
                        if(marker_info[i].cfMatrix < 0.25 )
                            continue;
                        id=marker_info[i].idMatrix+1;
                        markerType=1;
                    }
                    if(id>0){
                        id=id*markerType;
                        if(current_markers.contains(id*markerType)){
                            if(
                                    (id<0 && current_markers[id].cfPatt<marker_info[i].cfPatt) ||
                                    (id>0 && current_markers[id].cfMatrix<marker_info[i].cfMatrix)){
                                current_markers[id]=marker_info[i];
                            }
                        }else{
                            current_markers[id]=marker_info[i];
                        }
                    }
                }
                /*FIX due to https://github.com/artoolkit/artoolkit5/pull/163*/
                Q_FOREACH(int id,current_markers.keys()){
                    ARMarkerInfo o=current_markers[id];
                    if(id>0){
                        o.cf=o.cfMatrix;
                        o.dir=o.dirMatrix;
                        o.id=o.idMatrix;
                    }
                    else if(id<0){
                        o.cf=o.cfPatt;
                        o.dir=o.dirPatt;
                        o.id=o.idPatt;
                    }
                    current_markers[id]=o;
                }
                /*.....*/

                Q_FOREACH(int id,current_markers.keys()){
                    if(ar_objects.contains(id)){
                        AR3DObject* o=ar_objects[id];
                        ARMarkerInfo current_marker=current_markers[id];
                        if(o->was_visible)
                            o->size<0 ? err=arGetTransMatSquareCont(ar_3d_handle, &current_marker, o->pose , default_marker_size, new_pose) : err=arGetTransMatSquareCont(ar_3d_handle, &(current_markers[id]), o->pose , o->size, new_pose);
                        else
                            o->size<0 ? err=arGetTransMatSquare(ar_3d_handle, &current_marker, default_marker_size, new_pose): err=arGetTransMatSquare(ar_3d_handle, &(current_markers[id]), o->size, new_pose);
                        if(err<0)
                            continue;
                        /*Filter*/
                        arFilterTransMat(o->ftmi,new_pose,!o->was_visible);
                        /*...*/

                        for(int i=0;i<3;i++)
                            for(int j=0;j<4;j++){
                                o->pose[i][j]=new_pose[i][j];
                                if(j<3)
                                    rotMat(i,j)=new_pose[i][j];
                            }

                          o->rotation=openglAlignment*QQuaternion::fromRotationMatrix(rotMat);
                          o->translation.setX(new_pose[0][3]);
                          o->translation.setY(-new_pose[1][3]);
                          o->translation.setZ(-new_pose[2][3]);

//                        qDebug()<<"("<<new_pose[0][0]<<","<<new_pose[0][1]<<","<<new_pose[0][2]<<","<<new_pose[0][3];
//                        qDebug()<<new_pose[1][0]<<","<<new_pose[1][1]<<","<<new_pose[1][2]<<","<<new_pose[1][3];
//                        qDebug()<<new_pose[2][0]<<","<<new_pose[2][1]<<","<<new_pose[2][2]<<","<<new_pose[2][3];
//                        qDebug()<<")";

                        o->visible=true;

                        if(id>0)
                            posemap[QString("Mat_")+QString::number(--id)]=Pose(o->translation,o->rotation,
                                                                                QVector2D(current_marker.vertex[(4-current_marker.dirMatrix)%4][0],current_marker.vertex[(4-current_marker.dirMatrix)%4][1]),
                                                                                QVector2D(current_marker.vertex[(5-current_marker.dirMatrix)%4][0],current_marker.vertex[(5-current_marker.dirMatrix)%4][1]),
                                                                                QVector2D(current_marker.vertex[(6-current_marker.dirMatrix)%4][0],current_marker.vertex[(6-current_marker.dirMatrix)%4][1]),
                                                                                QVector2D(current_marker.vertex[(7-current_marker.dirMatrix)%4][0],current_marker.vertex[(7-current_marker.dirMatrix)%4][1])
                                                                                );
                        else if(id<0)
                            posemap[QString("Patt_")+QString::number(-id-1)]=Pose(o->translation,o->rotation,
                                                                                  QVector2D(current_marker.vertex[(4-current_marker.dirMatrix)%4][0],current_marker.vertex[(4-current_marker.dirMatrix)%4][1]),
                                                                                  QVector2D(current_marker.vertex[(5-current_marker.dirMatrix)%4][0],current_marker.vertex[(5-current_marker.dirMatrix)%4][1]),
                                                                                  QVector2D(current_marker.vertex[(6-current_marker.dirMatrix)%4][0],current_marker.vertex[(6-current_marker.dirMatrix)%4][1]),
                                                                                  QVector2D(current_marker.vertex[(7-current_marker.dirMatrix)%4][0],current_marker.vertex[(7-current_marker.dirMatrix)%4][1])
                                                                                 );
                    }
                    else{
                       AR3DObject* new_object=new AR3DObject;
                       new_object->visible=false;
                       new_object->was_visible=false;
                       new_object->size=-1;
                       new_object->ftmi=arFilterTransMatInit(AR_FILTER_TRANS_MAT_SAMPLE_RATE_DEFAULT,AR_FILTER_TRANS_MAT_CUTOFF_FREQ_DEFAULT);
                       ar_objects[id]=new_object;
                    }

                }
                /*Multipatter*/
                Q_FOREACH(QString id, ar_multimarker_objects.keys()){
                    AR3DMultiPatternObject* o=ar_multimarker_objects[id];
                    err=arGetTransMatMultiSquareRobust(ar_3d_handle,marker_info,marker_num,o->marker_info);
                    if(err<0)
                        o->visible=false;
                    else{
                        /*Filter*/
                        arFilterTransMat(o->ftmi,o->marker_info->trans,!o->visible);
                        /*...*/

                        o->visible=true;
                        for(int i=0;i<3;i++)
                            for(int j=0;j<3;j++)
                                rotMat(i,j)=o->marker_info->trans[i][j];

                        o->rotation=openglAlignment*QQuaternion::fromRotationMatrix(rotMat);
                        o->translation.setX(o->marker_info->trans[0][3]);
                        o->translation.setY(-o->marker_info->trans[1][3]);
                        o->translation.setZ(-o->marker_info->trans[2][3]);
                        posemap[id]=Pose(o->translation,o->rotation);
                    }
                }
            }
            else{
                Q_FOREACH(QString id, ar_multimarker_objects.keys())
                    ar_multimarker_objects[id]->visible=false;
            }

            Q_FOREACH(int id,ar_objects.keys())
                ar_objects[id]->was_visible=ar_objects[id]->visible;

            //notify
            emit objectsReady(posemap);

#ifdef DEBUG_FPS
            millis = (long)timer.restart();
            millisElapsed += millis;
            if(millis>0){
                fps = FPS_RATE*fps + (1.0f - FPS_RATE)*(1000.0f/millis);
                if(millisElapsed >= FPS_PRINT_PERIOD){
                    qDebug("ARToolkit is running at %f FPS",fps);
                    millisElapsed = 0;
                }
            }
#endif

        }
        else{
            nextFrameCond.wait(&frameLock);
            frameLock.unlock();
        }



    }

//    frameLock.unlock();

}

void ARToolKit::cleanup()
{
    if(ar_3d_handle){
        ar3DDeleteHandle(&ar_3d_handle);
        ar_3d_handle=NULL;
    }
    if(ar_patt_handle){
        arPattDeleteHandle(ar_patt_handle);
        ar_patt_handle=NULL;
    }
    if(ar_handle){
        arPattDetach(ar_handle);
        ar_handle=NULL;
    }
    if(ar_patt_handle){
        arPattDeleteHandle(ar_patt_handle);
        ar_patt_handle=NULL;
    }
    if(ar_param){
        arParamLTFree(&ar_param);
        ar_param=NULL;
    }
}

void ARToolKit::presentFrame(QByteArray& frame)
{

    frameLock.lock();
    buffer=frame;
    buffer_plane_1.clear();
    buffer_plane_2.clear();
    nextFrameAvailable = true;
    nextFrameCond.wakeAll();
    frameLock.unlock();
}

void ARToolKit::presentFrame(QByteArray& frame,QByteArray& frame_plane_1,QByteArray& frame_plane_2)
{
    frameLock.lock();
    buffer=frame;
    buffer_plane_1=frame_plane_1;
    buffer_plane_2=frame_plane_2;
    nextFrameAvailable = true;
    nextFrameCond.wakeAll();
    frameLock.unlock();
}

void ARToolKit::setPixelFormat(AR_PIXEL_FORMAT code)
{
    if(code!=pixFormat){
        pixFormat=code;
        if(ar_handle!=NULL)
            if (arSetPixelFormat(ar_handle, pixFormat) < 0) {
                cleanup();
                qWarning("Error in setting pixel format");
            }
    }
}

void ARToolKit::setFlip_Image(bool val)
{
    m_flip_image=val;
}


void ARToolKit::stop()
{
    running=false;
    nextFrameCond.wakeAll();
}

void ARToolKit::setProjectionMatrix(QMatrix4x4 m)
{
    projectionMatrix=m;
    using_default_projection=false;
    setupCameraParameters();
    setupMarkerParameters();
}

void ARToolKit::loadSingleMarkersConfigFile(QUrl url)
{
    QString file_path;
    if(url.scheme()=="file")
        file_path=url.toLocalFile();
    else if(url.scheme()=="qrc"){
        file_path=url.toString();
        file_path.remove(0,3);
    }
    QFile file(file_path);
    if(!file.open(QIODevice::ReadOnly)){
        qWarning("Cannot open single marker file");
        return;
    }
    QByteArray data = file.readAll();

    QJsonDocument jsonDoc(QJsonDocument::fromJson(data));
    if(jsonDoc.isNull()){
        qWarning("Invalid json format in single marker file");
        return;
    }

    QJsonArray objList=jsonDoc.array();
    if(!objList.isEmpty()){
        for(int i=0;i<objList.size();i++){
            QJsonObject o=objList[i].toObject();
            if(!o.isEmpty()){
                int id;
                if(!o["id"].isDouble())
                    continue;
                id=(int)o["id"].toDouble()+1;
                qreal size;
                if(!o["size"].isDouble())
                    continue;
                size=o["size"].toDouble();
                frameLock.lock();
                if(ar_objects.contains(id)){
                    ar_objects[id]->size=size;
                }
                else{
                    AR3DObject* o=new AR3DObject;
                    o->visible=false;
                    o->was_visible=false;
                    o->ftmi=arFilterTransMatInit(m_sample_freq,m_cutoff_freq);
                    o->size=size;
                    ar_objects[id]=o;
                }
                frameLock.unlock();
            }
        }
    }
    file.close();
}

void ARToolKit::loadMultiMarkersConfigFile(QString config_name,QUrl url)
{
    if(!ar_handle){
        qWarning("AR Handle is null");
        return;
    }

    QString file_path;
    if(url.scheme()=="file")
        file_path=url.toLocalFile();
    else if(url.scheme()=="qrc"){
        file_path=url.toString();
        file_path.remove(0,3);
    }
    QFile file(file_path);
    if(!file.open(QIODevice::ReadOnly)){
        qWarning("Cannot open multi marker file");
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QFile tmp_file("tmp_multi_file");
    if(!tmp_file.open(QIODevice::WriteOnly)){
        qWarning("Cannot open multi marker temporary file");
        return;
    }
    QTextStream tmp_stream(&tmp_file);
    tmp_stream << data;
    tmp_stream.flush();
    tmp_file.close();

    ARMultiMarkerInfoT* marker_info=arMultiReadConfigFile("tmp_multi_file",ar_patt_handle);
    if(marker_info!=NULL){
        marker_info->min_submarker=0;
        AR3DMultiPatternObject* o=new AR3DMultiPatternObject;
        o->marker_info=marker_info;
        o->visible=false;
        o->ftmi=arFilterTransMatInit(m_sample_freq,m_cutoff_freq);
        frameLock.lock();
        if(ar_multimarker_objects.contains(config_name)){
            arFilterTransMatFinal(ar_multimarker_objects[config_name]->ftmi);
            arMultiFreeConfig(ar_multimarker_objects[config_name]->marker_info);
            delete ar_multimarker_objects[config_name];
        }
        ar_multimarker_objects[config_name]=o;
        frameLock.unlock();
    }
    else{
        qWarning()<<"Cannot load multipatter file";
    }

    tmp_file.remove();
}

void ARToolKit::setDistortionParameters(QVector4D param)
{
    distortionParameters=param;
    using_default_distortion_parameters=false;
    setupCameraParameters();
    setupMarkerParameters();
}

void ARToolKit::setMatrixCode(AR_MATRIX_CODE_TYPE code)
{
    code_type=code;
    setupMarkerParameters();
}

void ARToolKit::setLabelingThreshold(int v)
{
    labelingThreshold=v;
    arSetLabelingThresh(ar_handle,labelingThreshold);

}

void ARToolKit::setFilter_sample_rate(qreal v)
{
    m_sample_freq=v;
    frameLock.lock();
    Q_FOREACH(AR3DObject* object, ar_objects.values())
           arFilterTransMatSetParams(object->ftmi,m_sample_freq,m_cutoff_freq);
    Q_FOREACH(AR3DMultiPatternObject* object, ar_multimarker_objects.values())
           arFilterTransMatSetParams(object->ftmi,m_sample_freq,m_cutoff_freq);
    frameLock.unlock();
}

void ARToolKit::setFilter_cutoff_freq(qreal v)
{
    m_cutoff_freq=v;
    frameLock.lock();
    Q_FOREACH(AR3DObject* object, ar_objects.values())
           arFilterTransMatSetParams(object->ftmi,m_sample_freq,m_cutoff_freq);
    Q_FOREACH(AR3DMultiPatternObject* object, ar_multimarker_objects.values())
           arFilterTransMatSetParams(object->ftmi,m_sample_freq,m_cutoff_freq);
    frameLock.unlock();
}

void ARToolKit::setCameraResolution(QSize size)
{
    if(cameraResolution!=size){
        cameraResolution=size;
        qDebug()<<"Camera:"<<cameraResolution;
        setupCameraParameters();
        setupMarkerParameters();
    }
}

void ARToolKit::setDefaultMarkerSize(qreal size)
{
    default_marker_size=size;
}

void ARToolKit::setupCameraParameters()
{
    ARParam cparam,scaled_cparam;
    /*
        * The values for  dist_function_version  correspond to the following algorithms:
        version 1: The original ARToolKit lens model, with a single radial distortion factor, plus center of distortion.<br>
        version 2: Improved distortion model, introduced in ARToolKit v4.0. This algorithm adds a quadratic term to the radial distortion factor of the version 1 algorithm.<br>
        version 3: Improved distortion model with aspect ratio, introduced in ARToolKit v4.0. The addition of an aspect ratio to the version 2 algorithm allows for non-square pixels, as found e.g. in DV image streams.<br>
        version 4: OpenCV-based distortion model, introduced in ARToolKit v4.3. This differs from the standard OpenCV model by the addition of a scale factor, so that input values do not exceed the range [-1.0, 1.0] in either forward or inverse conversions.

        https://github.com/artoolkit/artoolkit5/blob/3b8c20be09908cbf903915100e54aa2cfcbd5386/util/calib_camera/calib_camera.cpp
    */

    bool scaling=false;
    cparam.xsize=640;
    cparam.ysize=480;
    cparam.dist_function_version=4;
    cparam.dist_factor[0]=distortionParameters.x();
    cparam.dist_factor[1]=distortionParameters.y();
    cparam.dist_factor[2]=distortionParameters.z();
    cparam.dist_factor[3]=distortionParameters.w();

    cparam.dist_factor[4]=projectionMatrix.operator ()(0,0);
    cparam.dist_factor[5]=projectionMatrix.operator ()(1,1);
    cparam.dist_factor[6]=projectionMatrix.operator ()(0,3);
    cparam.dist_factor[7]=projectionMatrix.operator ()(1,3);
    cparam.dist_factor[8]=1;

    int i,j;
    for(i=0;i<3;i++){
        for(j=0;j<3;j++)
            cparam.mat[i][j]=projectionMatrix.operator ()(i,j);
        cparam.mat[i][3]=0;
    }

    ARdouble s = getSizeFactor(cparam.dist_factor, cparam.xsize, cparam.ysize, cparam.dist_function_version);
    cparam.mat[0][0] /= s;
    cparam.mat[0][1] /= s;
    cparam.mat[1][0] /= s;
    cparam.mat[1][1] /= s;
    cparam.dist_factor[8] = s;

    if(cameraResolution.width()!=640 || cameraResolution.height()!=480){
        //Scaling
        scaling=true;
        arParamChangeSize(&cparam,cameraResolution.width(),cameraResolution.height(),&scaled_cparam);
    }

    if(scaling){
        cparam.xsize=scaled_cparam.xsize;
        cparam.ysize=scaled_cparam.ysize;
        if(using_default_projection){
            for(int i=0;i<3;i++)
                for(int j=0;j<3;j++){
                    projectionMatrix.operator ()(i,j)=scaled_cparam.mat[i][j];
                    cparam.mat[i][j]=scaled_cparam.mat[i][j];
                }
            emit projectionMatrixChanged(projectionMatrix);
            cparam.dist_factor[4] = scaled_cparam.dist_factor[4];
            cparam.dist_factor[5] = scaled_cparam.dist_factor[5];
            cparam.dist_factor[6] = scaled_cparam.dist_factor[6];
            cparam.dist_factor[7] = scaled_cparam.dist_factor[7];
            cparam.dist_factor[8] = scaled_cparam.dist_factor[8];
        }

        if(using_default_distortion_parameters){
            QVector4D scaled_dist_params( cparam.dist_factor[0], cparam.dist_factor[1], cparam.dist_factor[2], cparam.dist_factor[3]);
            if(!qFuzzyCompare(distortionParameters,scaled_dist_params)){
                distortionParameters=scaled_dist_params;
                emit distortionParametersChanged(distortionParameters);
                cparam.dist_factor[0]=scaled_cparam.dist_factor[0];
                cparam.dist_factor[1]=scaled_cparam.dist_factor[1];
                cparam.dist_factor[2]=scaled_cparam.dist_factor[2];
                cparam.dist_factor[3]=scaled_cparam.dist_factor[3];
            }
        }
    }

    if(ar_param)
        arParamLTFree(&ar_param);

    if ((ar_param = arParamLTCreate(&cparam, AR_PARAM_LT_DEFAULT_OFFSET)) == NULL) {
        cleanup();
        qWarning("Error in creating ar_param");
        return;
    }

    if(ar_handle)
        arDeleteHandle(ar_handle);

    if ((ar_handle = arCreateHandle(ar_param)) == NULL) {
        cleanup();
        qWarning("Error in creating ar_handle");
        return;

    }
    if (arSetPixelFormat(ar_handle, pixFormat) < 0) {
        cleanup();
        qWarning("Error in setting pixel format");
        return;
    }
    if (arSetDebugMode(ar_handle, AR_DEBUG_DISABLE) < 0) {
        cleanup();
        qWarning("Error in setting debug mode");
        return;
    }

    if(ar_3d_handle)
        ar3DDeleteHandle(&ar_3d_handle);

    if ((ar_3d_handle = ar3DCreateHandle(&cparam)) == NULL) {
        cleanup();
        qWarning("Error in creating ar_3d_handle");
        return;
    }

}

void ARToolKit::setupMarkerParameters()
{
    if(ar_handle){
        arSetLabelingThreshMode(ar_handle, threshold_mode);
        arSetLabelingThresh(ar_handle,labelingThreshold);
        arSetPatternDetectionMode(ar_handle,patter_detection_mode);
        arSetMatrixCodeType(ar_handle, code_type);
        if(ar_patt_handle==NULL){
            ar_patt_handle=arPattCreateHandle();
        }
        arPattAttach(ar_handle, ar_patt_handle);
    }
}

ARdouble ARToolKit::getSizeFactor(ARdouble dist_factor[], int xsize, int ysize, int dist_function_version)
{
    ARdouble  ox, oy, ix, iy;
    ARdouble  olen, ilen;
    ARdouble  sf, sf1;

    sf = 100.0;

    ox = 0.0;
    oy = dist_factor[7];
    olen = dist_factor[6];
    arParamObserv2Ideal( dist_factor, ox, oy, &ix, &iy, dist_function_version );
    ilen = dist_factor[6] - ix;
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }

    ox = xsize;
    oy = dist_factor[7];
    olen = xsize - dist_factor[6];
    arParamObserv2Ideal( dist_factor, ox, oy, &ix, &iy, dist_function_version );
    ilen = ix - dist_factor[6];
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }

    ox = dist_factor[6];
    oy = 0.0;
    olen = dist_factor[7];
    arParamObserv2Ideal( dist_factor, ox, oy, &ix, &iy, dist_function_version );
    ilen = dist_factor[7] - iy;
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }

    ox = dist_factor[6];
    oy = ysize;
    olen = ysize - dist_factor[7];
    arParamObserv2Ideal( dist_factor, ox, oy, &ix, &iy, dist_function_version );
    ilen = iy - dist_factor[7];
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }


    ox = 0.0;
    oy = 0.0;
    arParamObserv2Ideal( dist_factor, ox, oy, &ix, &iy, dist_function_version );
    ilen = dist_factor[6] - ix;
    olen = dist_factor[6];
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }
    ilen = dist_factor[7] - iy;
    olen = dist_factor[7];
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }

    ox = xsize;
    oy = 0.0;
    arParamObserv2Ideal( dist_factor, ox, oy, &ix, &iy, dist_function_version );
    ilen = ix - dist_factor[6];
    olen = xsize - dist_factor[6];
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }
    ilen = dist_factor[7] - iy;
    olen = dist_factor[7];
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }

    ox = 0.0;
    oy = ysize;
    arParamObserv2Ideal( dist_factor, ox, oy, &ix, &iy, dist_function_version );
    ilen = dist_factor[6] - ix;
    olen = dist_factor[6];
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }
    ilen = iy - dist_factor[7];
    olen = ysize - dist_factor[7];
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }

    ox = xsize;
    oy = ysize;
    arParamObserv2Ideal( dist_factor, ox, oy, &ix, &iy, dist_function_version );
    ilen = ix - dist_factor[6];
    olen = xsize - dist_factor[6];
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }
    ilen = iy - dist_factor[7];
    olen = ysize - dist_factor[7];
    //ARLOG("Olen = %f, Ilen = %f, s = %f\n", olen, ilen, ilen / olen);
    if( ilen > 0 ) {
        sf1 = ilen / olen;
        if( sf1 < sf ) sf = sf1;
    }

    if( sf == 100.0 ) sf = 1.0;

    return sf;
}

Pose::Pose()
{
   translation=QVector3D();
   rotation=QQuaternion();
   TLCorner=QVector2D();
   TRCorner=QVector2D();
   BLCorner=QVector2D();
   BRCorner=QVector2D();
}

Pose::Pose(const QVector3D &translation, const QQuaternion &rotation, const QVector2D &TLCorner, const QVector2D &TRCorner, const QVector2D &BRCorner, const QVector2D &BLCorner)
{
    this->translation=translation;
    this->rotation=rotation;
    this->TLCorner=TLCorner;
    this->TRCorner=TRCorner;
    this->BRCorner=BRCorner;
    this->BLCorner=BLCorner;
}
