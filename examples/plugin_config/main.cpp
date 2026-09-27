// Reference scenario 3 (docs/use-cases.md): configuration built at run time
// from data (plugin manifests), with groups added and removed on a live model,
// shown in List mode.

#include <qpb/qpb.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

namespace {

struct PluginManifest
{
    QString name;
    QStringList modes;
};

void addPlugin(qpb::PropertyGroup& root, const PluginManifest& manifest)
{
    auto& group = root.addGroup(manifest.name);
    group.addBool("enabled", false);
    group.addDouble("threshold", 0.5).range(0.0, 1.0).decimals(2).step(0.05);
    group.addEnum("mode", manifest.modes, 0);
    group.addFilePath("output", {}).dialogMode(qpb::FileMode::Save);
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    qpb::PropertyModel model(qpb::PropertyGroup::create("Plugins"));
    const QList<PluginManifest> available {
        {"denoise", {"Fast", "Quality"}},
        {"sharpen", {"Unsharp mask", "Laplacian"}},
        {"export", {"PNG", "EXR", "TIFF"}},
    };

    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* buttons = new QHBoxLayout;
    for (const PluginManifest& manifest : available) {
        auto* load = new QPushButton("Load " + manifest.name);
        auto* unload = new QPushButton("Unload " + manifest.name);
        QObject::connect(load, &QPushButton::clicked, &model, [&model, manifest] {
            if (!model.root()->child(manifest.name))
                addPlugin(*model.root(), manifest);
        });
        QObject::connect(unload, &QPushButton::clicked, &model,
            [&model, name = manifest.name] { model.root()->remove(name); });
        buttons->addWidget(load);
        buttons->addWidget(unload);
    }

    auto* view = new qpb::PropertyTreeView;
    view->setMode(qpb::PropertyTreeView::Mode::List);
    view->setModel(&model);
    layout->addLayout(buttons);
    layout->addWidget(view);
    window.show();
    return app.exec();
}
