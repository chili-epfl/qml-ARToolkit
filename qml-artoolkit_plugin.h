#ifndef QML_ARTOOLKIT_PLUGIN_H
#define QML_ARTOOLKIT_PLUGIN_H

#include <QQmlExtensionPlugin>

class Qml_ARToolkitPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void registerTypes(const char *uri);
};

#endif // QML_ARTOOLKIT_PLUGIN_H
