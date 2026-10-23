#ifndef QPB_QOBJECTPROPERTYSOURCE_P_H
#define QPB_QOBJECTPROPERTYSOURCE_P_H

// Internal header, not part of the public API.

#include <qpb/qpbglobal.h>

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qvariant.h>

#include <memory>
#include <vector>

namespace qpb {

class Property;
class PropertyGroup;
class PropertyModel;

namespace detail {

// One object shown by a QObjectPropertySource.
struct ObjectBinding
{
    QPointer<QObject> object;
    // Identity only, never dereferenced: QPointer is already null when
    // QObject::destroyed() is emitted.
    const QObject* key = nullptr;
    QString groupPath;
    // Q_PROPERTY name for every property id of the group.
    QHash<QString, QMetaProperty> properties;
    // Property ids refreshed by each NOTIFY signal (by signal index).
    QHash<int, QStringList> notified;
    // Q_PROPERTY giving the group's display name (1.3), if any.
    QMetaProperty title;
    QList<QMetaObject::Connection> connections;
    // Removed while a sync was running: disconnected and out of the model, but
    // the record stays until the outermost sync returns.
    bool dead = false;
};

// Receives the NOTIFY signals of all objects (connected by index, so a slot
// is needed) and the model's value changes.
class QObjectPropertySourcePrivate : public QObject
{
    Q_OBJECT

public:
    explicit QObjectPropertySourcePrivate(PropertyModel* model);
    ~QObjectPropertySourcePrivate() override;

    ObjectBinding* bindingOf(const QObject* object) const;
    PropertyGroup* groupOf(const ObjectBinding& binding) const;
    // Disconnects the object and removes its group at once; the record itself is
    // erased when no sync is running.
    void remove(ObjectBinding* binding, bool removeGroup);
    void eraseDead();
    // Copies the object's value of Q_PROPERTY property into the model.
    void read(ObjectBinding& binding, const QString& id);
    // Sets the group's display name from the title Q_PROPERTY (1.3).
    void readTitle(ObjectBinding& binding);
    void modelValueChanged(const QString& path, const QVariant& value);

    // Marks a sync (a model write, a notification, a refresh) in progress. Records
    // of removed objects are erased when the outermost scope ends, so no loop of a
    // running sync sees a record vanish or move.
    class SyncScope
    {
    public:
        explicit SyncScope(QObjectPropertySourcePrivate* owner)
            : m_owner(owner)
        {
            ++m_owner->syncDepth;
        }
        ~SyncScope()
        {
            if (--m_owner->syncDepth == 0)
                m_owner->eraseDead();
        }
        SyncScope(const SyncScope&) = delete;
        SyncScope& operator=(const SyncScope&) = delete;

    private:
        QObjectPropertySourcePrivate* m_owner;
    };

    QPointer<PropertyModel> model;
    // Records of live objects, and of removed ones until the outermost sync ends.
    std::vector<std::unique_ptr<ObjectBinding>> bindings;
    int syncDepth = 0;
    bool updating = false; // writing one side from the other
    QString titleProperty; // setTitleProperty()
    bool liveReadOnly = false; // setLiveReadOnlyProperties()
    QMetaObject::Connection modelConnection;

public Q_SLOTS:
    void notified();
};

} // namespace detail
} // namespace qpb

#endif // QPB_QOBJECTPROPERTYSOURCE_P_H
