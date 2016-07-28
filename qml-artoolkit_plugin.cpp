#include "qml-artoolkit_plugin.h"
#include "artoolkit_video_filter.h"
#include "artoolkit_object.h"
#include <qqml.h>

void Qml_ARToolkitPlugin::registerTypes(const char *uri)
{
    qmlRegisterType<ARToolKitVideoFilter>(uri, 1, 0, "ARToolkit");
    qmlRegisterType<ARToolKitObject>(uri, 1, 0, "ARToolkitObject");

}

