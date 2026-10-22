#pragma once

#include <qpb/qpb.h>

#include <QStringList>
#include <QWidget>

class QLineEdit;

// Plugin configuration built from manifests at run time.
struct PluginManifest
{
    QString name;
    QStringList modes;
};

class PluginsPage : public QWidget
{
    Q_OBJECT
public:
    explicit PluginsPage(QWidget* parent = nullptr);

    void load(const PluginManifest& manifest);
    void unload(const QString& name);
    // Enables or disables every loaded plugin as one batch.
    void setAllEnabled(bool enabled);

    qpb::PropertyModel& model()
    {
        return m_model;
    }
    qpb::PropertyFilterProxyModel& proxy()
    {
        return m_proxy;
    }
    qpb::PropertyTreeView* view() const
    {
        return m_view;
    }
    QLineEdit* search() const
    {
        return m_search;
    }

private:
    qpb::PropertyModel m_model;
    qpb::PropertyFilterProxyModel m_proxy; // filters the tree for the search box
    qpb::PropertyTreeView* m_view;
    QLineEdit* m_search;
};
