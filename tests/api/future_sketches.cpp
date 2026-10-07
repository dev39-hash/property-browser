// Future-proofing check for the 1.0 API (docs/PLAN.md M1.3).
//
// Rough sketches of the features planned for 1.1 and 1.2, written against the
// public 1.0 headers only. They are compiled (never linked or run) so the
// compiler proves that each feature can be added later without changing the
// 1.0 API. See docs/api-review.md for the conclusions.
//
// The features have since been implemented (1.1, 1.2); the sketches stay as
// application code that must keep compiling: e.g. their unqualified toJson()
// and save() calls must not become ambiguous with qpb's functions (D40).

#include <qpb/qpb.h>

#include <QFormLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QMetaProperty>
#include <QScrollArea>
#include <QSettings>
#include <QSortFilterProxyModel>

#include <limits>

namespace sketch {

// --- 1.1: PropertyFormView -------------------------------------------------------
// A new class on top of PropertyModel + EditorFactory. Committing goes through
// QAbstractItemModel::setData; reverse sync uses the standard model signals.
class PropertyFormView : public QScrollArea
{
public:
    void setModel(QAbstractItemModel* model)
    {
        m_model = model;
        connect(model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex& topLeft) { refresh(topLeft); });
        connect(model, &QAbstractItemModel::modelReset, this, [this] { rebuild(); });
        rebuild();
    }

private:
    void rebuild()
    {
        auto* form = new QFormLayout;
        addRows(form, QModelIndex());
    }

    void addRows(QFormLayout* form, const QModelIndex& parent)
    {
        for (int row = 0; row < m_model->rowCount(parent); ++row) {
            const QModelIndex name = m_model->index(row, qpb::PropertyModel::NameColumn, parent);
            const auto* property
                = name.data(qpb::PropertyModel::PropertyRole).value<const qpb::Property*>();
            if (!property)
                continue;
            if (property->isGroup()) {
                addRows(form, name);
                continue;
            }
            QWidget* editor = qpb::EditorFactory::global().createEditor(nullptr, *property);
            if (const qpb::EditorHandler* handler
                = qpb::EditorFactory::global().handlerFor(*property)) {
                handler->setEditorData(editor, property->value(), *property);
            }
            form->addRow(property->displayName(), editor);
        }
    }

    void commit(QWidget* editor, const QModelIndex& valueIndex)
    {
        const auto* property
            = valueIndex.data(qpb::PropertyModel::PropertyRole).value<const qpb::Property*>();
        const qpb::EditorHandler* handler = qpb::EditorFactory::global().handlerFor(*property);
        m_model->setData(valueIndex, handler->editorData(editor, *property), Qt::EditRole);
    }

    void refresh(const QModelIndex&) { }

    QAbstractItemModel* m_model = nullptr;
};

// --- 1.1: PropertyFilterProxyModel -------------------------------------------------
class PropertyFilterProxyModel : public QSortFilterProxyModel
{
public:
    PropertyFilterProxyModel()
    {
        setRecursiveFilteringEnabled(true);
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setFilterKeyColumn(qpb::PropertyModel::NameColumn);
    }
};

void useFilter(qpb::PropertyModel& model, qpb::PropertyTreeView& view)
{
    auto* proxy = new PropertyFilterProxyModel;
    proxy->setSourceModel(&model);
    view.setModel(proxy); // PropertyTreeView accepts proxies of a PropertyModel
}

// --- 1.2: QObjectPropertySource ----------------------------------------------------
// Maps Q_PROPERTYs to properties. Type lookup uses TypeRegistry::types() and
// TypeHandler::storageType; two-way sync uses PropertyModel::valueChanged and
// Property::setValue.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using MetaType = QMetaType; // the type of TypeHandler::storageType
MetaType metaTypeOf(const QMetaProperty& property) { return property.metaType(); }
MetaType stringMetaType() { return QMetaType::fromType<QString>(); }
#else
using MetaType = int; // Qt 5 configuration (1.7)
MetaType metaTypeOf(const QMetaProperty& property) { return property.userType(); }
MetaType stringMetaType() { return qMetaTypeId<QString>(); }
#endif

qpb::TypeId typeFor(MetaType metaType)
{
    qpb::TypeRegistry& registry = qpb::TypeRegistry::global();
    if (metaType == stringMetaType())
        return qpb::Types::String; // String/FilePath/DirPath ambiguity: default to String
    for (const qpb::TypeId& id : registry.types()) {
        if (registry.handler(id)->storageType == metaType)
            return id;
    }
    return {};
}

void addObject(qpb::PropertyModel& model, QObject* object)
{
    const QMetaObject* meta = object->metaObject();
    qpb::PropertyGroup& group = model.root()->addGroup(object->objectName());
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty metaProperty = meta->property(i);
        const qpb::TypeId type = typeFor(metaTypeOf(metaProperty));
        if (type.isEmpty())
            continue;
        qpb::Property& property
            = group.add(type, QString::fromLatin1(metaProperty.name()), metaProperty.read(object));
        property.setReadOnly(!metaProperty.isWritable());
    }
    QObject::connect(&model, &qpb::PropertyModel::valueChanged, object,
        [object, prefix = group.path() + QLatin1Char('/')](
            const QString& path, const QVariant& value) {
            if (path.startsWith(prefix))
                object->setProperty(path.mid(prefix.size()).toLatin1().constData(), value);
        });
}

// --- 1.2: serialization ------------------------------------------------------------
QJsonObject toJson(const qpb::PropertyGroup& group)
{
    QJsonObject json;
    for (const qpb::Property* property : group.children()) {
        if (const qpb::PropertyGroup* child = property->toGroup())
            json.insert(property->id(), toJson(*child));
        else
            json.insert(property->id(), QJsonValue::fromVariant(property->value()));
    }
    return json;
}

void fromJson(qpb::PropertyGroup& group, const QJsonObject& json)
{
    for (auto it = json.begin(); it != json.end(); ++it) {
        qpb::Property* property = group.child(it.key());
        if (!property)
            continue;
        if (qpb::PropertyGroup* child = property->toGroup())
            fromJson(*child, it.value().toObject());
        else
            property->setValue(it.value().toVariant());
    }
}

void save(const qpb::PropertyGroup& group, QSettings& settings)
{
    for (const qpb::Property* property : group.children()) {
        if (const qpb::PropertyGroup* child = property->toGroup())
            save(*child, settings);
        else
            settings.setValue(property->path(), property->value());
    }
}

// --- 1.2: Int64 --------------------------------------------------------------------
// A new built-in type registered like any custom type; PropertyGroup would gain
// an additional addInt64() returning a new Int64Builder.
void registerInt64()
{
    qpb::TypeHandler handler;
    handler.normalize = [](const QVariant& value, const qpb::Property& property) {
        const qint64 minimum
            = property.attribute(qpb::Attr::Minimum, std::numeric_limits<qint64>::min())
                  .toLongLong();
        const qint64 maximum
            = property.attribute(qpb::Attr::Maximum, std::numeric_limits<qint64>::max())
                  .toLongLong();
        return QVariant::fromValue(qBound(minimum, value.toLongLong(), maximum));
    };
    qpb::TypeRegistry::global().registerType<qint64>(QStringLiteral("int64"), handler);
}

} // namespace sketch
