// Reference scenario 2 (docs/use-cases.md): application settings backed by
// QSettings, with string-valued enums, read-only entries and a property that
// is enabled only while another one is on.

#include <qpb/qpb.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QSettings>
#include <QVBoxLayout>

namespace {

std::unique_ptr<qpb::PropertyGroup> createSettings(const QSettings& settings)
{
    auto root = qpb::PropertyGroup::create("Settings");

    auto& general = root->addGroup("General");
    general.addEnum("language", QList<qpb::EnumOption> {{"English", "en"}, {"Tiếng Việt", "vi"}},
        settings.value("General/language", "en"));
    general.addBool("autosave", settings.value("General/autosave", true).toBool());
    general.addInt("autosaveMinutes", settings.value("General/autosaveMinutes", 5).toInt())
        .range(1, 60)
        .suffix(" min");

    auto& paths = root->addGroup("Paths");
    paths.addDirPath("projectDir", settings.value("Paths/projectDir", QDir::homePath()).toString())
        .mustExist();
    paths.addDirPath("cacheDir", settings.value("Paths/cacheDir").toString());

    auto& about = root->addGroup("About");
    about.addString("version", QCoreApplication::applicationVersion()).readOnly();
    return root;
}

void save(const qpb::PropertyGroup& group, QSettings& settings)
{
    for (const qpb::Property* property : group.children()) {
        if (const qpb::PropertyGroup* child = property->toGroup())
            save(*child, settings);
        else if (!property->isReadOnly())
            settings.setValue(property->path(), property->value());
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationVersion("1.0.0");
    QSettings settings("qpb", "settings_dialog");

    qpb::PropertyModel model(createSettings(settings));
    qpb::Property* minutes = model.find("General/autosaveMinutes");
    minutes->setEnabled(model.find("General/autosave")->value().toBool());
    QObject::connect(&model, &qpb::PropertyModel::valueChanged,
        [minutes](const QString& path, const QVariant& value) {
            if (path == QLatin1String("General/autosave"))
                minutes->setEnabled(value.toBool());
        });

    QDialog dialog;
    auto* layout = new QVBoxLayout(&dialog);
    auto* view = new qpb::PropertyTreeView;
    view->setModel(&model);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(view);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted)
        save(*model.root(), settings);
    return 0;
}
