#ifndef ARUCOOBJECT_H
#define ARUCOOBJECT_H

#include <QMatrix4x4>
#include <QVector3D>
#include <QQuickItem>
#include <QString>
#include <QQuaternion>

class ARToolKitObject : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QVector3D translation READ translation NOTIFY poseChanged)
    Q_PROPERTY(QQuaternion rotationQuaternion READ rotationQuaternion NOTIFY poseChanged)
    Q_PROPERTY(bool objectIsVisible READ objectIsVisible NOTIFY objectIsVisibleChanged)
    Q_PROPERTY(QString objectId READ objectId WRITE setObjectId NOTIFY objectIdChanged)
public:
    ARToolKitObject(QQuickItem * parent = 0);

    QQuaternion rotationQuaternion(){return m_rotation;}
    QVector3D translation(){return m_translation;}

    void setPose(QVector3D translation, QQuaternion rotation){
        m_rotation=m_pre*rotation*m_app;
        m_translation=translation;
        emit poseChanged();
    }

    bool objectIsVisible(){
        return m_visible;
    }

    void setVisible(bool val){
        if(val!=m_visible){
            m_visible=val;
            emit objectIsVisibleChanged();
        }
    }

    QString objectId(){return m_objectId;}
    void setObjectId(QString val){
        if(val!=m_objectId){
            QString tmp=m_objectId;
            m_objectId=val;
            emit objectIdChanged(tmp);
        }
    }

    Q_INVOKABLE void  prependQuaternion(QQuaternion q);
    Q_INVOKABLE void appendQuaternion(QQuaternion q);
signals:
    void poseChanged();
    void objectIsVisibleChanged();
    void objectIdChanged(QString previousObjID);
private:
    bool m_visible;
    QString m_objectId;
    QQuaternion m_rotation;
    QVector3D m_translation;
    QQuaternion m_pre,m_app;
};



#endif // ARUCOOBJECT_H
