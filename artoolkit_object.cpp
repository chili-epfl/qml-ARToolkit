#include "artoolkit_object.h"

ARToolKitObject::ARToolKitObject(QQuickItem *parent):
    QQuickItem(parent),
    m_rotation(),
    m_translation()
{
    m_visible=false;
    m_objectId=QString();
    m_TLCorner=QVector2D(-1,-1);
    m_TRCorner=QVector2D(-1,-1);
    m_BRCorner=QVector2D(-1,-1);
    m_BLCorner=QVector2D(-1,-1);
}

void ARToolKitObject::setCorners(const QVector2D &TL, const QVector2D &TR, const QVector2D &BR, const QVector2D &BL)
{
    m_TLCorner=TL;
    m_TRCorner=TR;
    m_BLCorner=BL;
    m_BRCorner=BR;
    emit TLCornerChanged();
    emit TRCornerChanged();
    emit BRCornerChanged();
    emit BLCornerChanged();
}

void ARToolKitObject::prependQuaternion(QQuaternion q)
{
    m_pre=q*m_pre;
}

void ARToolKitObject::appendQuaternion(QQuaternion q)
{
    m_app=m_app*q;
}

