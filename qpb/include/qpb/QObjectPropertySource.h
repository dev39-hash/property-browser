#ifndef QPB_QOBJECTPROPERTYSOURCE_H
#define QPB_QOBJECTPROPERTYSOURCE_H

#include <qpb/qpbglobal.h>

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

namespace qpb {

class PropertyGroup;
class PropertyModel;

namespace detail {
class QObjectPropertySourcePrivate;
}

// Shows the Q_PROPERTYs of QObjects in a PropertyModel and keeps both sides in
// sync (docs/SPEC.md section 4.9). Since 1.2.
//
// addObject() adds a group with one property per Q_PROPERTY: bool, int,
// qint64, double/float, QString, enums (Q_ENUM) and any type registered in
// TypeRegistry for the property's QMetaType. Other types are skipped.
// Properties that cannot be written are read-only. Edits in the model are
// written to the object; changes of the object are read back when the
// Q_PROPERTY has a NOTIFY signal (otherwise call refresh()).
//
// By default the properties declared by the object's class and its base
// classes are used, except QObject's own (objectName). The class can choose
// and describe them with Q_CLASSINFO:
//
//   Q_CLASSINFO("qpb:properties", "name,fov,lut")        // these, in this order
//   Q_CLASSINFO("qpb:fov", "min=10;max=170;suffix= deg;displayName=Field of view")
//   Q_CLASSINFO("qpb:lut", "type=filepath;filter=LUT (*.cube)")
//
// Keys of a property's class info: type (a TypeId), displayName, toolTip,
// readOnly, hidden, disabled, exclude, and any attribute key (min and max
// stand for minimum and maximum). A key without "=value" means true.
class QPB_CORE_EXPORT QObjectPropertySource : public QObject
{
    Q_OBJECT

public:
    // model is not owned and must outlive the source.
    explicit QObjectPropertySource(PropertyModel* model, QObject* parent = nullptr);
    // Stops synchronizing; the groups stay in the model.
    ~QObjectPropertySource() override;

    PropertyModel* model() const;

    // Adds a group for object under parentGroup (the model's root when
    // nullptr; a model without a root gets an empty one), which must belong to
    // the model. The group id is id, else the
    // object name, else the class name, made unique with a "_2", "_3" ...
    // suffix. Returns the group, or nullptr if object is nullptr. Adding an
    // object again returns its existing group. When the object is destroyed its
    // group is removed.
    PropertyGroup* addObject(
        QObject* object, PropertyGroup* parentGroup = nullptr, const QString& id = QString());
    // Removes the object's group and stops synchronizing it. Returns false if
    // the object was not added.
    bool removeObject(QObject* object);

    QList<QObject*> objects() const;
    // The group of an added object, or nullptr.
    PropertyGroup* groupOf(const QObject* object) const;

    // Reads every property of every object again (for Q_PROPERTYs without a
    // NOTIFY signal).
    void refresh();

private:
    std::unique_ptr<detail::QObjectPropertySourcePrivate> d;
};

} // namespace qpb

#endif // QPB_QOBJECTPROPERTYSOURCE_H
