#pragma once

#include <qpb/qpb.h>

#include <QJsonObject>
#include <QWidget>

class QSettings;
class QStackedWidget;

// Reference scenario 2: application settings stored in QSettings.
// Round 4 (1.2): stored with qpb::serialization, shown as a tree or a form.
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
    qpb::PropertyFormView* form() const
    {
        return m_form;
    }

    void save();
    void resetAll();
    // Tree (false) or form (true) layout.
    void setFormLayout(bool form);
    bool isFormLayout() const;

    QJsonObject exportJson() const;
    // Returns false if a value was rejected (the others are applied).
    bool importJson(const QJsonObject& json);

private:
    QSettings& m_settings;
    qpb::PropertyModel m_model;
    qpb::PropertyTreeView* m_view;
    qpb::PropertyFormView* m_form;
    QStackedWidget* m_stack;
};
