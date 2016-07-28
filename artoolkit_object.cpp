#include "artoolkit_object.h"

ARToolKitObject::ARToolKitObject(QQuickItem *parent):
    QQuickItem(parent),
    m_rotation(),
    m_translation()
{
    m_visible=false;
    m_objectId=QString();
}

void ARToolKitObject::prependQuaternion(QQuaternion q)
{
    m_pre=q*m_pre;
}

void ARToolKitObject::appendQuaternion(QQuaternion q)
{
    m_app=m_app*q;
}

