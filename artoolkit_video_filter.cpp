#include "artoolkit_video_filter.h"
#include "artoolkit_filter_runnable.h"

ARToolKitVideoFilter::ARToolKitVideoFilter(QQuickItem *parent):
    QAbstractVideoFilter(parent)
{
    qRegisterMetaType<PoseMap>("PoseMap");
    m_labeling_threshold=AR_DEFAULT_LABELING_THRESH;
    m_pause=false;

    m_cameraResolution=QSize(640,480);
    m_distortionParameters=QVector4D(0,0,0,0);

    m_projectionMatrix=QMatrix4x4(
                610, 0., m_cameraResolution.width()/2 ,0,
                0., 610, m_cameraResolution.height()/2,0,
                0,              0,              1,  0,
                0,              0,              0,  0
                );

    m_using_default_projection=true;
    m_using_default_distortion_parameters=true;
    m_default_marker_size=30;
    m_filter_runnable=NULL;
    m_matrix_code=MATRIX_CODE_3x3;
    m_cutoff_freq=15;
    m_sample_freq=30;
}

ARToolKitVideoFilter::~ARToolKitVideoFilter(){

}

QVideoFilterRunnable *ARToolKitVideoFilter::createFilterRunnable()
{
    m_filter_runnable=new ARToolkitFilterRunnable();

    if(!m_using_default_projection)
        m_filter_runnable->setProjectionMatrix(m_projectionMatrix);
    if(!m_using_default_distortion_parameters)
        m_filter_runnable->setDistortionParameters(m_distortionParameters);
    if(!m_single_markers_config_file_url.isEmpty())
        m_filter_runnable->loadSingleMarkersConfigFile(m_single_markers_config_file_url);
    if(!m_multi_markers_config_file_url.isEmpty())
        m_filter_runnable->loadMultiMarkersConfigFile(m_multi_marker_config_name,m_multi_markers_config_file_url);

    m_filter_runnable->setMatrixCode((AR_MATRIX_CODE_TYPE)m_matrix_code);
    m_filter_runnable->setDefaultMarkerSize(m_default_marker_size);
    m_filter_runnable->setLabelingThreshold(m_labeling_threshold);
    m_filter_runnable->setFilter_cutoff_freq(m_cutoff_freq);
    m_filter_runnable->setFilter_sample_rate(m_sample_freq);
    m_filter_runnable->setPause(m_pause);
    m_filter_runnable->start();

    connect(this,SIGNAL(destroyed(QObject*)),m_filter_runnable,SLOT(deleteLater()));
    connect(m_filter_runnable,SIGNAL(destroyed(QObject*)),this,SLOT(cleanFilter()));

    connect(m_filter_runnable,SIGNAL(objectsReady(PoseMap)),this,SLOT(notifyObservers(PoseMap)));
    connect(m_filter_runnable,SIGNAL(projectionMatrixChanged(QMatrix4x4)),this,SLOT(setProjectionMatrix_private(QMatrix4x4)));
    connect(m_filter_runnable,SIGNAL(distortionParametersChanged(QVector4D)),this,SLOT(setDistortionParameters_private(QVector4D)));
    connect(m_filter_runnable,SIGNAL(cameraResolutionChanged(QSize)),this,SLOT(setCameraResolution(QSize)));

    return m_filter_runnable;
}

QMatrix4x4 ARToolKitVideoFilter::getProjectionMatrix() const
{
    return m_projectionMatrix;
}

QSize ARToolKitVideoFilter::cameraResolution() const
{
    return m_cameraResolution;
}

void ARToolKitVideoFilter::loadSingleMarkersConfigFile(QUrl url)
{
    if(!url.isEmpty()){
        m_single_markers_config_file_url=url;
        if(m_filter_runnable)
            m_filter_runnable->loadSingleMarkersConfigFile(m_single_markers_config_file_url);
    }
}

void ARToolKitVideoFilter::loadMultiMarkersConfigFile(QString config_name,QUrl url)
{
    if(config_name.isEmpty()){
        qWarning()<<"Invalid MultiMarker config name";
        return;
    }
    if(!url.isEmpty()){
        m_multi_markers_config_file_url=url;
        m_multi_marker_config_name=config_name;
        if(m_filter_runnable)
            m_filter_runnable->loadMultiMarkersConfigFile(config_name,m_single_markers_config_file_url);
    }
}

void ARToolKitVideoFilter::setProjectionMatrix(QMatrix4x4 m)
{
    m_using_default_projection=false;
    setProjectionMatrix_private(m);
    if(m_filter_runnable)
        m_filter_runnable->setProjectionMatrix(m_projectionMatrix);
}

void ARToolKitVideoFilter::setDistortionParameters(QVector4D params)
{
    m_using_default_distortion_parameters=false;
    m_distortionParameters=params;
    if(m_filter_runnable)
        m_filter_runnable->setDistortionParameters(m_distortionParameters);

    emit distortionParametersChanged();

}

void ARToolKitVideoFilter::setDistortionParameters_private(QVector4D params){
    m_distortionParameters=params;
    emit distortionParametersChanged();
}


void ARToolKitVideoFilter::setProjectionMatrix_private(QMatrix4x4 m)
{
    m_projectionMatrix=m;
    emit projectionMatrixChanged();
}

void ARToolKitVideoFilter::setCameraResolution(QSize cameraResolution)
{
    if(cameraResolution!=m_cameraResolution){
        m_cameraResolution=cameraResolution;
        emit cameraResolutionChanged();
    }
}

void ARToolKitVideoFilter::registerObserver(ARToolKitObject *o)
{
    if(o){
        m_observers.insert(o->objectId(),o);
        connect(o,SIGNAL(destroyed(QObject*)),this,SLOT(unregisterDestroyedObserver(QObject*)));
        connect(o,SIGNAL(objectIdChanged(QString)),this,SLOT(updateObserver(QString)));
    }
}

void ARToolKitVideoFilter::setPause(bool val)
{
    if(val!=m_pause){
        m_pause=val;
        if(m_filter_runnable)
            m_filter_runnable->setPause(m_pause);
        emit pauseChanged();
    }
}

void ARToolKitVideoFilter::setDefaultMarkerSize(qreal size)
{
    if(size>0 && size!=m_default_marker_size){
        m_default_marker_size=size;
        if(m_filter_runnable)
            m_filter_runnable->setDefaultMarkerSize(m_default_marker_size);
        emit defaultMarkerSizeChanged();
    }
}

void ARToolKitVideoFilter::setMatrixCode(MATRIX_CODE_TYPE code)
{
    if(m_matrix_code!=code){
        m_matrix_code=code;
        if(m_filter_runnable)
            m_filter_runnable->setMatrixCode((AR_MATRIX_CODE_TYPE)m_matrix_code);
        emit matrixCodeChanged();
    }
}

void ARToolKitVideoFilter::setLabelingThreshold(int v)
{
    if(v<0)
        v=0;
    else if(v>255)
        v=255;
    if(v!=m_labeling_threshold){
        m_labeling_threshold=v;
        if(m_filter_runnable)
            m_filter_runnable->setLabelingThreshold(m_labeling_threshold);
        emit labelingThresholdChanged();
    }
}

void ARToolKitVideoFilter::setFilter_cutoff_freq(qreal val)
{
    if(val>0 && val!=m_cutoff_freq){
        m_cutoff_freq=val;
        if(m_filter_runnable)
            m_filter_runnable->setFilter_cutoff_freq(m_labeling_threshold);
        emit filter_cutoff_freqChanged();
    }

}

void ARToolKitVideoFilter::setFilter_sample_rate(qreal val)
{
    if(val>0 && val!=m_sample_freq){
        m_sample_freq=val;
        if(m_filter_runnable)
            m_filter_runnable->setFilter_sample_rate(m_labeling_threshold);
        emit filter_sample_rateChanged();
    }
}

void ARToolKitVideoFilter::unregisterObserver(ARToolKitObject *o)
{
    m_observers.remove(o->objectId(),o);
}

void ARToolKitVideoFilter::unregisterDestroyedObserver(QObject *o)
{
    QMultiHash<QString, ARToolKitObject*>::iterator i =m_observers.begin();
    while (i != m_observers.end()) {
        if(i.value()==o){
            m_observers.remove(i.key(),i.value());
            break;
        }
        ++i;
    }
}

void ARToolKitVideoFilter::updateObserver(QString prevObjId)
{
    ARToolKitObject* o=static_cast<ARToolKitObject*>(QObject::sender());
    m_observers.remove(prevObjId,o);
    m_observers.insert(o->objectId(),o);
    //TODO: update matrix and visibility
}

void ARToolKitVideoFilter::notifyObservers(const PoseMap &poses)
{
    //Change to concurrent
    m_detected_markers.clear();
    QVariantMap marker_info;
    Q_FOREACH(QString key, poses.keys()){
        Pose p=poses[key];
        marker_info["id"]=key;
        marker_info["rotation"]=p.rotation;
        marker_info["translation"]=p.translation;
        marker_info["TLCorner"]=p.TLCorner;
        marker_info["TRCorner"]=p.TRCorner;
        marker_info["BRCorner"]=p.BRCorner;
        marker_info["BLCorner"]=p.BLCorner;
        m_detected_markers.append(marker_info);
    }
    Q_FOREACH(QString key, m_observers.keys()){
        if(poses.contains(key)){
            Q_FOREACH(ARToolKitObject* observer, m_observers.values(key)){
                Pose p=poses[key];
                observer->setPose(p.translation,p.rotation);
                observer->setCorners(p.TLCorner,p.TRCorner,p.BRCorner,p.BLCorner);
                observer->setVisible(true);
            }
        }
        else{
            Q_FOREACH(ARToolKitObject* observer, m_observers.values(key))
                observer->setVisible(false);
        }
    }
    emit detectedMarkersChanged();
}

void ARToolKitVideoFilter::cleanFilter()
{
    m_filter_runnable=NULL;
}

