// Reference scenario 1 (docs/use-cases.md): scene object inspector in a tree view,
// with nested groups, constraints and a custom color type.

#include <qpb/qpb.h>

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QVBoxLayout>
#include <QWidget>

#include "../custom_type/ColorType.h"

namespace {

std::unique_ptr<qpb::PropertyGroup> createCameraProperties()
{
    auto root = qpb::PropertyGroup::create("Camera");
    root->addString("name", "Main camera")
        .maxLength(64)
        .regularExpression("[A-Za-z_][A-Za-z0-9_ ]*")
        .toolTip("Unique object name");
    root->addBool("visible", true);

    auto& transform = root->addGroup("Transform");
    for (const char* axis : {"x", "y", "z"})
        transform.addDouble(axis, 0.0).range(-100.0, 100.0).step(0.5).suffix(" m");

    auto& camera = root->addGroup("Camera");
    camera.addInt("fov", 60).range(10, 170).suffix("°").displayName("Field of view");
    camera.addEnum("projection", {"Perspective", "Orthographic"}, 0);
    // 1.4: shown only for the orthographic projection.
    camera.addDouble("orthoScale", 1.0).range(0.01, 100.0).visibleWhen("Camera/projection", 1);
    camera.addFilePath("lut", {})
        .filter("LUT files (*.cube)")
        .dialogMode(qpb::FileMode::Open)
        .mustExist();

    auto& render = root->addGroup("Render");
    render.add(example::ColorTypeId, "tint", QColor(Qt::white));
    return root;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    example::registerColorType();

    qpb::PropertyModel model(createCameraProperties());
    QObject::connect(&model, &qpb::PropertyModel::valueChanged,
        [](const QString& path, const QVariant& newValue, const QVariant& oldValue) {
            qDebug() << path << ":" << oldValue << "->" << newValue;
        });
    QObject::connect(&model, &qpb::PropertyModel::validationFailed,
        [](const QString& path, const QVariant&, const QString& message) {
            qWarning() << path << "rejected:" << message;
        });

    QWidget window;
    // 1.4: react to one property without comparing paths in valueChanged.
    model.onValueChanged("name", &window,
        [&window](const QVariant& name) { window.setWindowTitle(name.toString()); });
    auto* layout = new QVBoxLayout(&window);
    auto* modeBox = new QComboBox;
    modeBox->addItems({"Tree", "List"});
    auto* view = new qpb::PropertyTreeView;
    view->setModel(&model);
    view->setNameColumnWidth(160);
    QObject::connect(modeBox, qOverload<int>(&QComboBox::currentIndexChanged), view, [view](int index) {
        view->setMode(
            index == 0 ? qpb::PropertyTreeView::Mode::Tree : qpb::PropertyTreeView::Mode::List);
    });
    layout->addWidget(modeBox);
    layout->addWidget(view);
    window.resize(420, 480);
    window.show();
    return app.exec();
}
