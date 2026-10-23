#include <qpb/PropertyModel.h>
#include <qpb/TypeRegistry.h>

#include <QtCore/qhash.h>
#include <QtCore/qpointer.h>
#include <QtCore/qset.h>

#include <utility>
#include <vector>

#include "LanguageChange_p.h"
#include "Property_p.h"

namespace qpb {

using detail::PropertyPrivate;

namespace detail {

class PropertyModelPrivate : public TreeObserver
{
public:
    explicit PropertyModelPrivate(PropertyModel* model)
        : q(model)
    { }

    ~PropertyModelPrivate() override
    {
        // The model may be deleted by a handler while a walk over the tree runs:
        // the walk finishes over the detached tree, which the hold state then
        // destroys. Without a hold, root is destroyed with this object.
        detach();
        holdData->observerGone(root);
    }

    void attach(std::unique_ptr<PropertyGroup> newRoot)
    {
        root = std::move(newRoot);
        if (root)
            PropertyPrivate::get(root.get())->treeObserver = this;
    }

    void detach()
    {
        if (root)
            PropertyPrivate::get(root.get())->treeObserver = nullptr;
    }

    // Model index of a group as a parent: invalid for the root.
    QModelIndex parentIndex(PropertyGroup* group) const
    {
        return group == root.get() ? QModelIndex() : q->indexOf(group, PropertyModel::NameColumn);
    }

    bool isRegistered(const Property* property) const
    {
        if (property->isGroup() || TypeRegistry::global().contains(property->typeId()))
            return true;
        if (!warnedTypes.contains(property->typeId())) {
            warnedTypes.insert(property->typeId());
            qWarning("qpb: property type \"%s\" is not registered; \"%s\" is shown read-only",
                qUtf8Printable(property->typeId()), qUtf8Printable(property->path()));
        }
        return false;
    }

    QString displayText(const Property* property) const
    {
        if (property->isGroup() || property->typeId() == Types::Bool)
            return QString();
        const TypeHandler* handler = TypeRegistry::global().handler(property->typeId());
        if (handler && handler->displayText)
            return handler->displayText(property->value(), *property);
        return property->value().toString();
    }

    void emitRowsChanged(const QModelIndex& parent, int first, int last)
    {
        emit q->dataChanged(q->index(first, PropertyModel::NameColumn, parent),
            q->index(last, PropertyModel::ValueColumn, parent));
    }

    // A handler of dataChanged may delete the model: nothing of this object is
    // used after an emission unless the guard says the model is still there.
    void emitSubtreeChanged(PropertyGroup* group, const QPointer<PropertyModel>& guard)
    {
        const int count = group->childCount();
        if (count == 0)
            return;
        emitRowsChanged(parentIndex(group), 0, count - 1);
        if (!guard)
            return; // a handler deleted the model, and the group with it
        for (Property* child : group->children()) {
            if (!guard)
                return;
            if (PropertyGroup* childGroup = child->toGroup())
                emitSubtreeChanged(childGroup, guard);
        }
    }

    // A hold for the duration of a notification. A handler may delete the model,
    // which destroys this object: the hold keeps only the shared state, which
    // outlives it and destroys the tree when the hold ends.
    class NotificationHold
    {
    public:
        explicit NotificationHold(PropertyModelPrivate* owner)
            : state(owner->holdData)
        {
            state->begin();
        }
        ~NotificationHold()
        {
            state->end();
        }
        NotificationHold(const NotificationHold&) = delete;
        NotificationHold& operator=(const NotificationHold&) = delete;

    private:
        std::shared_ptr<TreeHoldState> state;
    };

    // --- TreeObserver ------------------------------------------------------------

    void aboutToInsert(PropertyGroup* parent, int row) override
    {
        q->beginInsertRows(parentIndex(parent), row, row);
    }

    void inserted(PropertyGroup*, int) override
    {
        q->endInsertRows();
        evaluateAllConditions(true, false); // a source may still be added later
    }

    void aboutToRemove(PropertyGroup* parent, int row) override
    {
        dependentsDirty = true; // may point into the removed subtree
        // Outside a model conditions count as met.
        resetConditions(parent->child(row));
        q->beginRemoveRows(parentIndex(parent), row, row);
    }

    void removed(PropertyGroup*, int) override
    {
        q->endRemoveRows();
        evaluateAllConditions(true, false);
    }

    void changed(Property* property, bool recursive) override
    {
        const QPointer<PropertyModel> guard(q);
        if (property != root.get()) {
            const QModelIndex index = q->indexOf(property);
            emitRowsChanged(index.parent(), index.row(), index.row());
        }
        if (recursive && guard) {
            if (PropertyGroup* group = property->toGroup())
                emitSubtreeChanged(group, guard);
        }
    }

    void valueChanged(Property* property, const QVariant& newValue, const QVariant& oldValue,
        bool byUser) override
    {
        // Handlers may replace the tree or remove the property; it stays alive
        // until the outermost notification has returned.
        const NotificationHold hold(this);
        // A handler may delete the model, which destroys this object: nothing
        // of it is used after a signal except through the guard.
        PropertyModel* const model = q;
        const QPointer<PropertyModel> guard(model);
        changed(property, false);
        if (!guard)
            return;
        const QString path = property->path();
        // Dependent states first, so valueChanged() handlers see them updated.
        evaluateDependents(path, guard);
        if (!guard)
            return;
        if (batchDepth > 0 && !batchPaths.contains(path))
            batchPaths.append(path);
        emit model->valueChanged(path, newValue, oldValue);
        if (byUser && guard)
            emit model->userValueChanged(path, newValue, oldValue);
    }

    void validationFailed(
        Property* property, const QVariant& rejectedValue, const QString& message) override
    {
        const NotificationHold hold(this);
        emit q->validationFailed(property->path(), rejectedValue, message);
    }

    const std::shared_ptr<TreeHoldState>& holdState() override
    {
        return holdData;
    }

    void retire(std::unique_ptr<Property> detached) override
    {
        holdData->retire(std::move(detached));
    }

    void beginBatch() override
    {
        ++batchDepth;
    }

    void endBatch() override
    {
        if (batchDepth == 0) {
            qWarning("qpb: PropertyModel::endBatch() without beginBatch()");
            return;
        }
        if (--batchDepth == 0 && !batchPaths.isEmpty()) {
            const QStringList paths = std::exchange(batchPaths, QStringList());
            emit q->batchValueChanged(paths);
        }
    }

    // --- conditions (1.4) ------------------------------------------------------

    void conditionsChanged(Property*) override
    {
        // The source may simply not be added yet: no warning here.
        evaluateAllConditions(true, false);
    }

    // Evaluates one condition; returns true if its result changed.
    bool evaluate(Property* property, PropertyCondition* condition, bool warn)
    {
        if (!condition)
            return false;
        const Property* source = root ? root->find(condition->sourcePath) : nullptr;
        if (!source && warn && !condition->warned) {
            condition->warned = true;
            qWarning("qpb: condition of \"%s\" refers to \"%s\", which does not exist; "
                     "it counts as met",
                qUtf8Printable(property->path()), qUtf8Printable(condition->sourcePath));
        }
        const bool met = !source || condition->test(source->value());
        if (met == condition->met)
            return false;
        condition->met = met;
        return true;
    }

    void evaluate(Property* property, bool notify, bool warn = true)
    {
        PropertyPrivate* data = PropertyPrivate::get(property);
        const bool enabledChanged = evaluate(property, data->enabledWhen.get(), warn);
        const bool visibleChanged = evaluate(property, data->visibleWhen.get(), warn);
        if (notify && (enabledChanged || visibleChanged))
            changed(property, true); // descendants' effective state too
    }

    static void resetConditions(Property* property)
    {
        if (!property)
            return;
        PropertyPrivate* data = PropertyPrivate::get(property);
        for (PropertyCondition* condition : {data->enabledWhen.get(), data->visibleWhen.get()}) {
            if (condition)
                condition->met = true;
        }
        if (PropertyGroup* group = property->toGroup()) {
            for (Property* child : group->children())
                resetConditions(child);
        }
    }

    void collectConditions(Property* property)
    {
        PropertyPrivate* data = PropertyPrivate::get(property);
        for (const PropertyCondition* condition :
            {data->enabledWhen.get(), data->visibleWhen.get()}) {
            if (condition && !dependents[condition->sourcePath].contains(property))
                dependents[condition->sourcePath].append(property);
        }
        if (PropertyGroup* group = property->toGroup()) {
            for (Property* child : group->children())
                collectConditions(child);
        }
    }

    void rebuildDependents()
    {
        dependents.clear();
        if (root)
            collectConditions(root.get());
        dependentsDirty = false;
    }

    void evaluateAllConditions(bool notify, bool warn = true)
    {
        rebuildDependents();
        for (const QList<Property*>& properties : std::as_const(dependents)) {
            for (Property* property : properties)
                evaluate(property, notify, warn);
        }
    }

    void evaluateDependents(const QString& sourcePath, const QPointer<PropertyModel>& guard)
    {
        if (dependentsDirty)
            rebuildDependents();
        const QList<Property*> properties = dependents.value(sourcePath);
        for (Property* property : properties) {
            evaluate(property, true);
            if (!guard)
                return;
        }
    }

    PropertyModel* q;
    std::unique_ptr<PropertyGroup> root;
    // Properties with a condition, by source path. Rebuilt when the tree or
    // the conditions change.
    QHash<QString, QList<Property*>> dependents;
    bool dependentsDirty = true;
    int batchDepth = 0;
    QStringList batchPaths;
    // Subtrees detached while a value notification or a group walk is running;
    // shared with the holds, so it outlives a deleted model.
    std::shared_ptr<TreeHoldState> holdData = std::make_shared<TreeHoldState>(this);
    mutable QSet<TypeId> warnedTypes;
};

} // namespace detail

PropertyModel::PropertyModel(QObject* parent)
    : QAbstractItemModel(parent)
    , d(std::make_unique<detail::PropertyModelPrivate>(this))
{
    // The column titles are translated: views read them again when the
    // application's language changes (doc/guide/translations.md).
    if (detail::LanguageChangeNotifier* notifier = detail::LanguageChangeNotifier::instance()) {
        connect(notifier, &detail::LanguageChangeNotifier::languageChanged, this,
            [this] { emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1); });
    }
}

PropertyModel::PropertyModel(std::unique_ptr<PropertyGroup> root, QObject* parent)
    : PropertyModel(parent)
{
    d->attach(std::move(root));
    d->evaluateAllConditions(false);
}

PropertyModel::~PropertyModel() = default;

void PropertyModel::setRoot(std::unique_ptr<PropertyGroup> root)
{
    beginResetModel();
    d->detach();
    std::unique_ptr<PropertyGroup> old = std::move(d->root);
    d->attach(std::move(root));
    d->evaluateAllConditions(false); // before views read the new tree
    endResetModel();
    // old is destroyed once views no longer refer to it, and not before the
    // value handlers running further up the call stack have returned.
    d->retire(std::move(old));
}

PropertyGroup* PropertyModel::root() const
{
    return d->root.get();
}

Property* PropertyModel::propertyAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != this)
        return nullptr;
    return static_cast<Property*>(index.internalPointer());
}

QModelIndex PropertyModel::indexOf(const Property* property, int column) const
{
    if (!property || !d->root || property == d->root.get() || column < 0
        || column >= columnCount()) {
        return QModelIndex();
    }
    const Property* top = property;
    while (top->parent())
        top = top->parent();
    if (top != d->root.get())
        return QModelIndex();
    const int row = property->parent()->indexOf(property);
    return createIndex(row, column, const_cast<Property*>(property));
}

Property* PropertyModel::find(const QString& path) const
{
    return d->root ? d->root->find(path) : nullptr;
}

bool PropertyModel::setValue(const QString& path, const QVariant& value)
{
    Property* property = find(path);
    return property && property->setValue(value);
}

bool PropertyModel::resetToDefault(const QModelIndex& index)
{
    Property* property = propertyAt(index);
    if (!property)
        return false;
    // A handler may delete the model: the batch is then gone with it.
    const QPointer<PropertyModel> guard(this);
    beginBatch();
    const bool ok = property->resetToDefault();
    if (guard)
        endBatch();
    return ok;
}

bool PropertyModel::resetAllToDefault()
{
    return !d->root || d->root->resetToDefault(); // a group reset is one batch
}

bool PropertyModel::normalizeAllValues()
{
    return !d->root || d->root->normalizeValue(); // a group is one batch
}

void PropertyModel::makeAllValuesDefault()
{
    if (d->root)
        d->root->makeValueDefault();
}

void PropertyModel::beginBatch()
{
    d->beginBatch();
}

void PropertyModel::endBatch()
{
    d->endBatch();
}

QModelIndex PropertyModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();
    const Property* parentProperty = parent.isValid() ? propertyAt(parent) : d->root.get();
    const PropertyGroup* group = parentProperty ? parentProperty->toGroup() : nullptr;
    Property* child = group ? group->child(row) : nullptr;
    return child ? createIndex(row, column, child) : QModelIndex();
}

QModelIndex PropertyModel::parent(const QModelIndex& child) const
{
    const Property* property = propertyAt(child);
    if (!property)
        return QModelIndex();
    PropertyGroup* parentGroup = property->parent();
    if (!parentGroup || parentGroup == d->root.get())
        return QModelIndex();
    return createIndex(parentGroup->parent()->indexOf(parentGroup), NameColumn, parentGroup);
}

int PropertyModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;
    const Property* property = parent.isValid() ? propertyAt(parent) : d->root.get();
    const PropertyGroup* group = property ? property->toGroup() : nullptr;
    return group ? group->childCount() : 0;
}

int PropertyModel::columnCount(const QModelIndex&) const
{
    return 2;
}

bool PropertyModel::hasChildren(const QModelIndex& parent) const
{
    return rowCount(parent) > 0;
}

QVariant PropertyModel::data(const QModelIndex& index, int role) const
{
    const Property* property = propertyAt(index);
    if (!property)
        return QVariant();

    switch (role) {
    case PropertyRole:
        return QVariant::fromValue(property);
    case TypeIdRole:
        return property->typeId();
    case PathRole:
        return property->path();
    case IsGroupRole:
        return property->isGroup();
    case IsModifiedRole:
        return property->isModified();
    case AttributesRole:
        return property->attributes();
    case IsVisibleRole:
        return property->isVisible();
    default:
        break;
    }

    if (index.column() == NameColumn) {
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return property->displayName();
        case Qt::ToolTipRole:
            return property->toolTip().isEmpty() ? QVariant() : QVariant(property->toolTip());
        default:
            return QVariant();
        }
    }

    // Value column.
    switch (role) {
    case Qt::DisplayRole:
        return property->isGroup() ? QVariant() : QVariant(d->displayText(property));
    case Qt::EditRole:
        return property->value();
    case Qt::ToolTipRole: {
        if (!property->toolTip().isEmpty())
            return property->toolTip();
        const QString text = d->displayText(property);
        return text.isEmpty() ? QVariant() : QVariant(text);
    }
    case Qt::CheckStateRole:
        if (property->typeId() == Types::Bool)
            return property->value().toBool() ? Qt::Checked : Qt::Unchecked;
        return QVariant();
    default:
        return QVariant();
    }
}

bool PropertyModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    Property* property = propertyAt(index);
    if (!property || index.column() != ValueColumn || property->isGroup())
        return false;
    // Edits through the model come from views, i.e. from the user.
    PropertyPrivate* data = PropertyPrivate::get(property);
    if (role == Qt::CheckStateRole && property->typeId() == Types::Bool)
        return data->assign(value.toInt() == Qt::Checked, PropertyPrivate::Origin::User);
    if (role == Qt::EditRole)
        return data->assign(value, PropertyPrivate::Origin::User);
    return false;
}

Qt::ItemFlags PropertyModel::flags(const QModelIndex& index) const
{
    const Property* property = propertyAt(index);
    if (!property)
        return Qt::NoItemFlags;

    Qt::ItemFlags result = Qt::ItemIsSelectable;
    if (property->isEnabled())
        result |= Qt::ItemIsEnabled;
    if (property->isGroup())
        return result;
    if (index.column() == NameColumn)
        return result | Qt::ItemNeverHasChildren;

    result |= Qt::ItemNeverHasChildren;
    if (property->isEnabled() && !property->isReadOnly() && d->isRegistered(property)) {
        result |= property->typeId() == Types::Bool ? Qt::ItemIsUserCheckable : Qt::ItemIsEditable;
    }
    return result;
}

QVariant PropertyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    switch (section) {
    case NameColumn:
        return tr("Property");
    case ValueColumn:
        return tr("Value");
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PropertyModel::roleNames() const
{
    QHash<int, QByteArray> names = QAbstractItemModel::roleNames();
    names.insert(PropertyRole, QByteArrayLiteral("property"));
    names.insert(TypeIdRole, QByteArrayLiteral("typeId"));
    names.insert(PathRole, QByteArrayLiteral("path"));
    names.insert(IsGroupRole, QByteArrayLiteral("isGroup"));
    names.insert(IsModifiedRole, QByteArrayLiteral("isModified"));
    names.insert(AttributesRole, QByteArrayLiteral("attributes"));
    names.insert(IsVisibleRole, QByteArrayLiteral("isVisible"));
    return names;
}

QMetaObject::Connection PropertyModel::onValueChanged(
    const QString& path, const QObject* context, std::function<void(const QVariant& value)> handler)
{
    if (!handler)
        return {};
    return connect(this, &PropertyModel::valueChanged, context ? context : this,
        [path, handler = std::move(handler)](const QString& changed, const QVariant& value) {
            if (changed == path)
                handler(value);
        });
}

QMetaObject::Connection PropertyModel::onValueChanged(const QString& path, const QObject* context,
    std::function<void(const QString& path, const QVariant& value)> handler)
{
    if (!handler)
        return {};
    const QString prefix = path.isEmpty() ? QString() : path + QLatin1Char('/');
    return connect(this, &PropertyModel::valueChanged, context ? context : this,
        [path, prefix, handler = std::move(handler)](
            const QString& changed, const QVariant& value) {
            if (path.isEmpty() || changed == path || changed.startsWith(prefix))
                handler(changed, value);
        });
}

QMetaObject::Connection PropertyModel::onUserValueChanged(
    const QString& path, const QObject* context, std::function<void(const QVariant& value)> handler)
{
    if (!handler)
        return {};
    return connect(this, &PropertyModel::userValueChanged, context ? context : this,
        [path, handler = std::move(handler)](const QString& changed, const QVariant& value) {
            if (changed == path)
                handler(value);
        });
}

QMetaObject::Connection PropertyModel::onUserValueChanged(const QString& path,
    const QObject* context,
    std::function<void(const QString& path, const QVariant& value)> handler)
{
    if (!handler)
        return {};
    const QString prefix = path.isEmpty() ? QString() : path + QLatin1Char('/');
    return connect(this, &PropertyModel::userValueChanged, context ? context : this,
        [path, prefix, handler = std::move(handler)](
            const QString& changed, const QVariant& value) {
            if (path.isEmpty() || changed == path || changed.startsWith(prefix))
                handler(changed, value);
        });
}

} // namespace qpb
