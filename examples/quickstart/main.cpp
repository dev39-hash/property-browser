// Quickstart: a 10-property panel (docs/SPEC.md S1: at most 30 lines without includes).

#include <qpb/qpb.h>

#include <QApplication>
#include <QDebug>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto root = qpb::PropertyGroup::create("Camera");
    root->addString("name", "Main camera");
    root->addBool("visible", true);
    auto& transform = root->addGroup("Transform");
    transform.addDouble("x", 0.0).range(-100.0, 100.0).step(0.5).suffix(" m");
    transform.addDouble("y", 0.0).range(-100.0, 100.0).step(0.5).suffix(" m");
    transform.addDouble("z", 0.0).range(-100.0, 100.0).step(0.5).suffix(" m");
    root->addInt("fov", 60).range(10, 170).suffix("°");
    root->addEnum("projection", {"Perspective", "Orthographic"}, 0);
    root->addFilePath("lut", {}).filter("LUT files (*.cube)");
    root->addDirPath("cacheDir", {});

    qpb::PropertyModel model(std::move(root));
    QObject::connect(&model, &qpb::PropertyModel::valueChanged,
        [](const QString& path, const QVariant& value) { qDebug() << path << "=" << value; });

    qpb::PropertyTreeView view;
    view.setModel(&model);
    view.show();
    return app.exec();
}
