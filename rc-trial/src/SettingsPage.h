#pragma once

#include <qpb/qpb.h>

#include <QWidget>

class QSettings;

// Reference scenario 2: application settings stored in QSettings.
class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(QSettings& settings, QWidget* parent = nullptr);

    qpb::PropertyModel& model()
    {
        return m_model;
    }
    qpb::PropertyTreeView* view() const
    {
        return m_view;
    }

    void save();
    void resetAll();

private:
    QSettings& m_settings;
    qpb::PropertyModel m_model;
    qpb::PropertyTreeView* m_view;
};
