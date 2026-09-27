#include "SettingsPage.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {

void saveGroup(const qpb::PropertyGroup& group, QSettings& settings)
{
    for (const qpb::Property* p : group.children()) {
        if (const qpb::PropertyGroup* child = p->toGroup())
            saveGroup(*child, settings);
        else if (!p->isReadOnly())
            settings.setValue(p->path(), p->value());
    }
}

} // namespace

SettingsPage::SettingsPage(QSettings& settings, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_view(new qpb::PropertyTreeView)
{
    auto root = qpb::PropertyGroup::create("Settings");
    auto& general = root->addGroup("General");
    general.addEnum(
        "language", QList<qpb::EnumOption> {{"English", "en"}, {"Vietnamese", "vi"}}, "en");
    general.addBool("autosave", true);
    general.addInt("autosaveMinutes", 5).range(1, 60).suffix(" min");
    auto& paths = root->addGroup("Paths");
    paths.addDirPath("projectDir", QDir::homePath()).mustExist();
    paths.addDirPath("cacheDir", QString());
    auto& about = root->addGroup("About");
    about.addString("version", QCoreApplication::applicationVersion()).readOnly();
    // Updated by the application, never by the user.
    about.addString("lastSaved", tr("never")).readOnly();
    m_model.setRoot(std::move(root));

    // Stored values override the defaults declared above.
    m_model.beginBatch();
    for (const QString& key : m_settings.allKeys()) {
        if (qpb::Property* p = m_model.find(key))
            p->setValue(m_settings.value(key));
    }
    m_model.endBatch();

    qpb::Property* minutes = m_model.find("General/autosaveMinutes");
    minutes->setEnabled(m_model.find("General/autosave")->value().toBool());
    connect(&m_model, &qpb::PropertyModel::valueChanged, this,
        [minutes](const QString& path, const QVariant& value) {
            if (path == QLatin1String("General/autosave"))
                minutes->setEnabled(value.toBool());
        });

    m_view->setModel(&m_model);
    auto* save = new QPushButton(tr("Save"));
    auto* reset = new QPushButton(tr("Reset all"));
    connect(save, &QPushButton::clicked, this, &SettingsPage::save);
    connect(reset, &QPushButton::clicked, this, &SettingsPage::resetAll);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(reset);
    buttons->addWidget(save);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_view);
    layout->addLayout(buttons);
}

void SettingsPage::save()
{
    saveGroup(*m_model.root(), m_settings);
    m_settings.sync();
    m_model.find("About/lastSaved")->setValue(QDateTime::currentDateTime().toString(Qt::ISODate));
}

void SettingsPage::resetAll()
{
    m_model.root()->resetToDefault();
}
