// Custom type example: a QColor property type registered by the application.

#include <qpb/qpb.h>

#include <QApplication>

#include "ColorType.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    example::registerColorType();

    auto root = qpb::PropertyGroup::create("Material");
    root->add(example::ColorTypeId, "baseColor", QColor(Qt::white));
    root->add(example::ColorTypeId, "emission", QColor(Qt::black)).setToolTip("Emitted light");
    root->addDouble("roughness", 0.5).range(0.0, 1.0).step(0.05);

    qpb::PropertyModel model(std::move(root));
    qpb::PropertyTreeView view;
    view.setModel(&model);
    view.show();
    return app.exec();
}
