#include "artoolkit.h"
#include <QDebug>
#include <QHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
//#include <AR/video.h>
#include <QImage>

#ifdef DEBUG_FPS
#include <QElapsedTimer>
const float FPS_RATE = 0.9f;            ///< Rate of using the older FPS estimates
const int FPS_PRINT_PERIOD = 500;       ///< Period of printing the FPS estimate, in milliseconds
#endif

ARToolKit::ARToolKit()
{
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

    memory_size=0;

    ar_buffer=(AR2VideoBufferT*)malloc(sizeof(AR2VideoBufferT));

    projectionMatrix=QMatrix4x4(
                610, 0., 640/2 ,0,
                0., 610, 480/2,0,
                0,              0,              1,  0,
                0,              0,              0,  1
                );
    distortionParameters=QVector4D(0,0,0,0);
    using_default_distortion_parameters=true;
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

    frameLock.lock();
    int marker_num;
    ARMarkerInfo* marker_info;
    QHash<int, ARMarkerInfo> current_markers;
    ARdouble new_pose[3][4];
    QMatrix3x3 rotMat;
    PoseMap posemap;
    qreal err;
    QQuaternion openglAlignment=QQuaternion::fromAxisAndAngle(1,0,0,180);
    while(running){
        if (nextFrameAvailable) {
            nextFrameAvailable=false;
            if(memory_size!=buffer.size()){
                if(memory_size!=0){
                    free(ar_buffer->buff);
                }
                ar_buffer->buff=(ARUint8*)malloc(sizeof(ARUint8)*buffer.size());
                memory_size=buffer.size();
            }
            memcpy(ar_buffer->buff,buffer.constData(),sizeof(ARUint8)*buffer.size());
            frameLock.unlock();
            ar_buffer->bufPlaneCount=0;
            ar_buffer->bufPlanes=NULL;
            ar_buffer->fillFlag=1;
            ar_buffer->time_sec=(ARUint32)QDateTime::currentMSecsSinceEpoch()/1000;
            ar_buffer->time_usec=(ARUint32)QDateTime::currentMSecsSinceEpoch()-ar_buffer->time_sec*1000;
            ar_buffer->buffLuma=ar_buffer->buff;
//            QImage img(ar_buffer->buff,cameraResolution.width(),cameraResolution.height(),QImage::Format_Grayscale8);
//            img.save(QString(getenv("EXTERNAL_STORAGE"))+"/Pictures/"+QString::number(rand()).append(".png"));
           // Detect the markers in the video frame.
            if (arDetectMarker(ar_handle, ar_buffer) < 0) {
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

        }
        else
            nextFrameCond.wait(&frameLock);
#ifdef DEBUG_FPS
        millis = (long)timer.restart();
        millisElapsed += millis;
        if(millis>0){
            fps = FPS_RATE*fps + (1.0f - FPS_RATE)*(1000.0f/millis);
            if(millisElapsed >= FPS_PRINT_PERIOD){
                qDebug("ARtoolkit is running at %f FPS",fps);
                millisElapsed = 0;
            }
        }
#endif
    }

    frameLock.unlock();

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
    nextFrameAvailable = true;
    nextFrameCond.wakeAll();
    frameLock.unlock();
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
                    o->ftmi=arFilterTransMatInit(AR_FILTER_TRANS_MAT_SAMPLE_RATE_DEFAULT,AR_FILTER_TRANS_MAT_CUTOFF_FREQ_DEFAULT);
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
        o->ftmi=arFilterTransMatInit(AR_FILTER_TRANS_MAT_SAMPLE_RATE_DEFAULT,AR_FILTER_TRANS_MAT_CUTOFF_FREQ_DEFAULT);
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
        arSetPatternDetectionMode(ar_handle,patter_detection_mode);
        arSetMatrixCodeType(ar_handle, code_type);
        if(ar_patt_handle==NULL){
            ar_patt_handle=arPattCreateHandle();
        }
        arPattAttach(ar_handle, ar_patt_handle);
    }
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
