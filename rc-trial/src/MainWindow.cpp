#include "MainWindow.h"

#include <qpb/qpb.h>

#include <QTabWidget>

#include "InspectorPage.h"
#include "PluginsPage.h"
#include "SettingsPage.h"

MainWindow::MainWindow(const QString& settingsFile, QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settingsFile, QSettings::IniFormat)
    , m_tabs(new QTabWidget)
    , m_inspector(new InspectorPage)
    , m_settingsPage(new SettingsPage(m_settings))
    , m_plugins(new PluginsPage)
{
    m_tabs->addTab(m_inspector, tr("Inspector"));
    m_tabs->addTab(m_settingsPage, tr("Settings"));
    m_tabs->addTab(m_plugins, tr("Plugins"));
    setCentralWidget(m_tabs);
    setWindowTitle(tr("qpb RC trial (qpb %1)").arg(QString::fromLatin1(qpb::version())));
    resize(640, 460);
    for (const PluginManifest& m : {PluginManifest {"denoise", {"Fast", "Quality"}},
             PluginManifest {"sharpen", {"Unsharp mask", "Laplacian"}}})
        m_plugins->load(m);
}
