#pragma once

#include <qpb/qpb.h>

#include <QColor>
#include <QList>
#include <QWidget>

class QListWidget;

// Reference scenario 1: inspector for the selected object of a scene.
struct SceneObject
{
    QString name;
    bool isCamera = false; // lights have no camera settings
    bool visible = true;
    double x = 0, y = 0, z = 0;
    int fov = 60;
    int projection = 0; // 0 perspective, 1 orthographic
    QString lut;
    QColor tint = Qt::white;
};

class InspectorPage : public QWidget
{
    Q_OBJECT
public:
    explicit InspectorPage(QWidget* parent = nullptr);

    QList<SceneObject>& scene()
    {
        return m_scene;
    }
    qpb::PropertyModel& model()
    {
        return m_model;
    }
    qpb::PropertyTreeView* view() const
    {
        return m_view;
    }
    QListWidget* objectList() const
    {
        return m_objects;
    }

private:
    std::unique_ptr<qpb::PropertyGroup> buildTree(int row) const;
    void showObject(int row);

    QList<SceneObject> m_scene;
    int m_current = -1;
    qpb::PropertyModel m_model;
    QListWidget* m_objects;
    qpb::PropertyTreeView* m_view;
};
