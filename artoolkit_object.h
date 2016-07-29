#ifndef ARUCOOBJECT_H
#define ARUCOOBJECT_H

#include <QMatrix4x4>
#include <QVector3D>
#include <QQuickItem>
#include <QString>
#include <QQuaternion>
#include <QVector2D>
class ARToolKitObject : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QVector3D translation READ translation NOTIFY poseChanged)
    Q_PROPERTY(QQuaternion rotationQuaternion READ rotationQuaternion NOTIFY poseChanged)
    Q_PROPERTY(QVector2D TLCorner READ TLCorner NOTIFY TLCornerChanged)
    Q_PROPERTY(QVector2D TRCorner READ TRCorner NOTIFY TRCornerChanged)
    Q_PROPERTY(QVector2D BRCorner READ BRCorner NOTIFY BRCornerChanged)
    Q_PROPERTY(QVector2D BLCorner READ BLCorner NOTIFY BLCornerChanged)

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
    QVector2D TLCorner(){return m_TLCorner;}
    QVector2D TRCorner(){return m_TRCorner;}
    QVector2D BRCorner(){return m_BRCorner;}
    QVector2D BLCorner(){return m_BLCorner;}

    void setVisible(bool val){
        if(val!=m_visible){
            m_visible=val;
            if(!m_visible){
                m_TLCorner=QVector2D(-1,-1);
                m_TRCorner=QVector2D(-1,-1);
                m_BRCorner=QVector2D(-1,-1);
                m_BLCorner=QVector2D(-1,-1);
            }
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
    void setCorners(const QVector2D& TL,const QVector2D& TR,const QVector2D& BR,const QVector2D& BL);
    Q_INVOKABLE void  prependQuaternion(QQuaternion q);
    Q_INVOKABLE void appendQuaternion(QQuaternion q);
signals:
    void poseChanged();
    void objectIsVisibleChanged();
    void objectIdChanged(QString previousObjID);
    void TLCornerChanged();
    void TRCornerChanged();
    void BRCornerChanged();
    void BLCornerChanged();

private:
    bool m_visible;
    QString m_objectId;
    QQuaternion m_rotation;
    QVector3D m_translation;
    QQuaternion m_pre,m_app;
    QVector2D m_TLCorner;
    QVector2D m_TRCorner;
    QVector2D m_BRCorner;
    QVector2D m_BLCorner;

};



#endif // ARUCOOBJECT_H
