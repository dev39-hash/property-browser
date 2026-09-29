#include "InspectorPage.h"

#include <QHBoxLayout>
#include <QListWidget>

#include "ColorType.h"
#include "SliderEditor.h"

std::unique_ptr<qpb::PropertyGroup> InspectorPage::buildTree(int row) const
{
    const SceneObject& o = m_scene[row];
    auto root = qpb::PropertyGroup::create("Object");
    root->addString("name", o.name)
        .maxLength(64)
        .regularExpression("[A-Za-z_][A-Za-z0-9_ ]*")
        // Names are unique within the scene.
        .validator([this, row](const QVariant& value, const qpb::Property&) {
            for (int i = 0; i < m_scene.size(); ++i) {
                if (i != row && m_scene[i].name == value.toString())
                    return qpb::ValidationResult::error(
                        tr("Another object is already named \"%1\"").arg(value.toString()));
            }
            return qpb::ValidationResult::valid();
        });
    root->addBool("visible", o.visible);
    auto& transform = root->addGroup("Transform");
    transform.addDouble("x", o.x).range(-100, 100).step(0.5).suffix(" m");
    transform.addDouble("y", o.y).range(-100, 100).step(0.5).suffix(" m");
    transform.addDouble("z", o.z).range(-100, 100).step(0.5).suffix(" m");
    auto& camera = root->addGroup("Camera");
    camera.addInt("fov", o.fov)
        .range(10, 170)
        .suffix(" deg")
        .displayName("Field of view")
        .editor(SliderEditor);
    camera.addEnum("projection", {"Perspective", "Orthographic"}, o.projection);
    camera.addFilePath("lut", o.lut).filter("LUT files (*.cube)");
    camera.setVisible(o.isCamera);
    root->addGroup("Render").add(ColorType, "tint", o.tint);
    return root;
}

InspectorPage::InspectorPage(QWidget* parent)
    : QWidget(parent)
    , m_objects(new QListWidget)
    , m_view(new qpb::PropertyTreeView)
{
    registerColorType();
    registerSliderEditor();
    for (const char* name : {"Main camera", "Key light", "Fill light"}) {
        SceneObject o;
        o.name = QString::fromLatin1(name);
        m_scene << o;
    }
    m_scene[0].isCamera = true;
    m_scene[1].tint = QColor(255, 220, 180);

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(m_objects, 1);
    layout->addWidget(m_view, 3);
    for (const SceneObject& o : m_scene)
        m_objects->addItem(o.name);
    m_view->setModel(&m_model);
    m_view->setNameColumnWidth(130);

    connect(m_objects, &QListWidget::currentRowChanged, this, &InspectorPage::showObject);
    // Round 6 (1.4): one callback per path instead of an if-chain on
    // valueChanged. The connections follow the paths across setRoot().
    const auto bind = [this](const char* path, auto write) {
        m_model.onValueChanged(QString::fromLatin1(path), this, [this, write](const QVariant& v) {
            if (m_current >= 0)
                write(m_scene[m_current], v);
        });
    };
    bind("name", [this](SceneObject& o, const QVariant& v) {
        o.name = v.toString();
        m_objects->item(m_current)->setText(o.name);
    });
    bind("visible", [](SceneObject& o, const QVariant& v) { o.visible = v.toBool(); });
    bind("Transform/x", [](SceneObject& o, const QVariant& v) { o.x = v.toDouble(); });
    bind("Transform/y", [](SceneObject& o, const QVariant& v) { o.y = v.toDouble(); });
    bind("Transform/z", [](SceneObject& o, const QVariant& v) { o.z = v.toDouble(); });
    bind("Camera/fov", [](SceneObject& o, const QVariant& v) { o.fov = v.toInt(); });
    bind("Camera/projection", [](SceneObject& o, const QVariant& v) { o.projection = v.toInt(); });
    bind("Camera/lut", [](SceneObject& o, const QVariant& v) { o.lut = v.toString(); });
    bind("Render/tint", [](SceneObject& o, const QVariant& v) { o.tint = v.value<QColor>(); });
    m_objects->setCurrentRow(0);
}

void InspectorPage::showObject(int row)
{
    m_current = row;
    if (row >= 0)
        m_model.setRoot(buildTree(row));
}
