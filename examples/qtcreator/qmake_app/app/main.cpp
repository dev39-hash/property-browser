// A small application that uses qpb from a qmake project (../qmake_app.pro).
// The window title shows the version the library was built from.

#include <qpb/qpb.h>

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto root = qpb::PropertyGroup::create("Camera");
    root->addString("name", "Main camera");
    root->addBool("visible", true);
    auto& transform = root->addGroup("Transform");
    transform.addDouble("x", 0.0).range(-100.0, 100.0).step(0.5).suffix(" m");
    transform.addDouble("y", 0.0).range(-100.0, 100.0).step(0.5).suffix(" m");
    root->addInt("fov", 60).range(10, 170);
    root->addEnum("projection", {"Perspective", "Orthographic"}, 0);
    qpb::PropertyModel model(std::move(root));

    qpb::PropertyTreeView view;
    view.setModel(&model);
    view.setWindowTitle(QStringLiteral("qmake app, qpb ") + QString::fromLatin1(QPB_VERSION_STR));
    view.resize(420, 320);
    view.show();
    return app.exec();
}
