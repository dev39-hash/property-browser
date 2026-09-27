#include "InspectorPage.h"

#include <QHBoxLayout>
#include <QListWidget>

#include "ColorType.h"

namespace {

std::unique_ptr<qpb::PropertyGroup> buildTree(const SceneObject& o)
{
    auto root = qpb::PropertyGroup::create("Object");
    root->addString("name", o.name).maxLength(64).regularExpression("[A-Za-z_][A-Za-z0-9_ ]*");
    root->addBool("visible", o.visible);
    auto& transform = root->addGroup("Transform");
    transform.addDouble("x", o.x).range(-100, 100).step(0.5).suffix(" m");
    transform.addDouble("y", o.y).range(-100, 100).step(0.5).suffix(" m");
    transform.addDouble("z", o.z).range(-100, 100).step(0.5).suffix(" m");
    auto& camera = root->addGroup("Camera");
    camera.addInt("fov", o.fov).range(10, 170).suffix(" deg").displayName("Field of view");
    camera.addEnum("projection", {"Perspective", "Orthographic"}, o.projection);
    camera.addFilePath("lut", o.lut).filter("LUT files (*.cube)");
    root->addGroup("Render").add(ColorType, "tint", o.tint);
    return root;
}

} // namespace

InspectorPage::InspectorPage(QWidget* parent)
    : QWidget(parent)
    , m_objects(new QListWidget)
    , m_view(new qpb::PropertyTreeView)
{
    registerColorType();
    m_scene = {{"Main camera"}, {"Key light"}, {"Fill light"}};
    m_scene[1].tint = QColor(255, 220, 180);

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(m_objects, 1);
    layout->addWidget(m_view, 3);
    for (const SceneObject& o : m_scene)
        m_objects->addItem(o.name);
    m_view->setModel(&m_model);
    m_view->setNameColumnWidth(130);

    connect(m_objects, &QListWidget::currentRowChanged, this, &InspectorPage::showObject);
    connect(&m_model, &qpb::PropertyModel::valueChanged, this,
        [this](const QString& path, const QVariant& value) { apply(path, value); });
    m_objects->setCurrentRow(0);
}

void InspectorPage::showObject(int row)
{
    m_current = row;
    if (row >= 0)
        m_model.setRoot(buildTree(m_scene[row]));
}

// Writes an edited property back into the application's data.
void InspectorPage::apply(const QString& path, const QVariant& value)
{
    if (m_current < 0)
        return;
    SceneObject& o = m_scene[m_current];
    if (path == "name") {
        o.name = value.toString();
        m_objects->item(m_current)->setText(o.name);
    } else if (path == "visible") {
        o.visible = value.toBool();
    } else if (path == "Transform/x") {
        o.x = value.toDouble();
    } else if (path == "Transform/y") {
        o.y = value.toDouble();
    } else if (path == "Transform/z") {
        o.z = value.toDouble();
    } else if (path == "Camera/fov") {
        o.fov = value.toInt();
    } else if (path == "Camera/projection") {
        o.projection = value.toInt();
    } else if (path == "Camera/lut") {
        o.lut = value.toString();
    } else if (path == "Render/tint") {
        o.tint = value.value<QColor>();
    }
}
