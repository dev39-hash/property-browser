#pragma once

#include <QMainWindow>
#include <QSettings>

class InspectorPage;
class SettingsPage;
class PluginsPage;
class DevicesPage;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString& settingsFile, QWidget* parent = nullptr);

    QTabWidget* tabs() const
    {
        return m_tabs;
    }
    InspectorPage* inspector() const
    {
        return m_inspector;
    }
    SettingsPage* settingsPage() const
    {
        return m_settingsPage;
    }
    PluginsPage* plugins() const
    {
        return m_plugins;
    }
    DevicesPage* devices() const
    {
        return m_devices;
    }

private:
    QSettings m_settings;
    QTabWidget* m_tabs;
    InspectorPage* m_inspector;
    SettingsPage* m_settingsPage;
    PluginsPage* m_plugins;
    DevicesPage* m_devices;
};
