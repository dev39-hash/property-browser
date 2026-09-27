#include "SettingsPage.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QSettings& settings, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_view(new qpb::PropertyTreeView)
    , m_form(new qpb::PropertyFormView)
    , m_stack(new QStackedWidget)
{
    auto root = qpb::PropertyGroup::create("Settings");
    auto& general = root->addGroup("General");
    general.addEnum(
        "language", QList<qpb::EnumOption> {{"English", "en"}, {"Vietnamese", "vi"}}, "en");
    general.addBool("autosave", true);
    general.addInt("autosaveMinutes", 5).range(1, 60).suffix(" min");
    general.addString("signature", QString())
        .multiline()
        .placeholder(tr("Added to exported files"));
    auto& paths = root->addGroup("Paths");
    paths.addDirPath("projectDir", QDir::homePath()).mustExist();
    paths.addDirPath("cacheDir", QString());
    auto& limits = root->addGroup("Limits");
    limits.addInt64("cacheBytes", qint64(10) << 30)
        .range(0, qint64(1) << 50)
        .step(qint64(1) << 20)
        .suffix(" B")
        .displayName(tr("Cache size"));
    auto& about = root->addGroup("About");
    about.addString("version", QCoreApplication::applicationVersion()).readOnly();
    // Updated by the application, never by the user.
    about.addString("lastSaved", tr("never")).readOnly();
    m_model.setRoot(std::move(root));

    // Stored values override the defaults declared above.
    m_model.beginBatch();
    qpb::serialization::load(*m_model.root(), m_settings);
    m_model.endBatch();

    qpb::Property* minutes = m_model.find("General/autosaveMinutes");
    minutes->setEnabled(m_model.find("General/autosave")->value().toBool());
    connect(&m_model, &qpb::PropertyModel::valueChanged, this,
        [minutes](const QString& path, const QVariant& value) {
            if (path == QLatin1String("General/autosave"))
                minutes->setEnabled(value.toBool());
        });

    m_view->setModel(&m_model);
    m_form->setModel(&m_model);
    m_stack->addWidget(m_view);
    m_stack->addWidget(m_form);
    auto* formLayout = new QCheckBox(tr("Form layout"));
    connect(formLayout, &QCheckBox::toggled, this, &SettingsPage::setFormLayout);
    auto* save = new QPushButton(tr("Save"));
    auto* reset = new QPushButton(tr("Reset all"));
    connect(save, &QPushButton::clicked, this, &SettingsPage::save);
    connect(reset, &QPushButton::clicked, this, &SettingsPage::resetAll);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(formLayout);
    buttons->addStretch();
    buttons->addWidget(reset);
    buttons->addWidget(save);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_stack);
    layout->addLayout(buttons);
}

void SettingsPage::save()
{
    qpb::serialization::save(*m_model.root(), m_settings);
    m_settings.sync();
    m_model.find("About/lastSaved")->setValue(QDateTime::currentDateTime().toString(Qt::ISODate));
}

void SettingsPage::resetAll()
{
    m_model.root()->resetToDefault();
}

void SettingsPage::setFormLayout(bool form)
{
    m_stack->setCurrentWidget(form ? static_cast<QWidget*>(m_form) : m_view);
}

bool SettingsPage::isFormLayout() const
{
    return m_stack->currentWidget() == m_form;
}

QJsonObject SettingsPage::exportJson() const
{
    return qpb::serialization::toJson(*m_model.root());
}

bool SettingsPage::importJson(const QJsonObject& json)
{
    m_model.beginBatch();
    const bool accepted = qpb::serialization::fromJson(*m_model.root(), json);
    m_model.endBatch();
    return accepted;
}
