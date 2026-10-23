#ifndef QPB_PROPERTY_P_H
#define QPB_PROPERTY_P_H

// Internal header, not part of the public API.

#include <qpb/Property.h>
#include <qpb/PropertyGroup.h>

#include <memory>
#include <utility>
#include <vector>

namespace qpb::detail {

class TreeHoldState;

// Receives change notifications from a property tree. Implemented by
// PropertyModel; only the root of a tree holds the pointer, every node finds
// it by walking up (see PropertyPrivate::observer()).
class TreeObserver
{
public:
    virtual ~TreeObserver() = default;

    virtual void aboutToInsert(PropertyGroup* parent, int row) = 0;
    virtual void inserted(PropertyGroup* parent, int row) = 0;
    virtual void aboutToRemove(PropertyGroup* parent, int row) = 0;
    virtual void removed(PropertyGroup* parent, int row) = 0;
    // Presentation, attributes or flags of property changed. With recursive,
    // the effective state of all descendants may have changed too.
    virtual void changed(Property* property, bool recursive) = 0;
    // byUser: the value was written through PropertyModel::setData (the user),
    // not by application code; the model then also announces it as a user edit.
    virtual void valueChanged(
        Property* property, const QVariant& newValue, const QVariant& oldValue, bool byUser)
        = 0;
    virtual void validationFailed(
        Property* property, const QVariant& rejectedValue, const QString& message)
        = 0;
    virtual void beginBatch() = 0;
    virtual void endBatch() = 0;
    // A condition (enabledWhen / visibleWhen) was set or cleared on property.
    virtual void conditionsChanged(Property* property) = 0;
    // The hold state of the tree (see TreeHoldState). It is shared with every
    // hold, so it outlives the observer while a hold is active.
    virtual const std::shared_ptr<TreeHoldState>& holdState() = 0;
    // Takes over a subtree that was detached from the tree. It is destroyed at
    // once, or when the outermost hold ends.
    virtual void retire(std::unique_ptr<Property> detached) = 0;
};

// While at least one hold is active, a subtree detached from the tree is kept
// alive: handlers of value signals may replace or remove parts of the tree
// while code further up the call stack still uses them. The observer (the
// model) may also be deleted by such a handler: it then hands its tree to the
// state and marks itself gone, so the walks in progress finish over the tree
// without notifications, and the tree is destroyed when the outermost hold
// ends.
class TreeHoldState
{
public:
    explicit TreeHoldState(TreeObserver* tree)
        : observer(tree)
    { }

    void begin()
    {
        ++depth;
    }

    void end()
    {
        if (--depth > 0)
            return;
        // Destroying a property runs user code (custom types); it may detach
        // more subtrees, which are destroyed at once (no hold is active).
        std::vector<std::unique_ptr<Property>> detached = std::exchange(retired, {});
        std::unique_ptr<PropertyGroup> tree = std::move(orphan);
        detached.clear();
        tree.reset();
    }

    // Takes over a subtree that was detached from the tree. It is destroyed at
    // once, or when the outermost hold ends.
    void retire(std::unique_ptr<Property> detached)
    {
        if (depth > 0)
            retired.push_back(std::move(detached));
        // else destroyed here
    }

    // The observer is being destroyed. With a hold active, the state keeps the
    // tree until the last hold ends; otherwise root is destroyed by the caller.
    void observerGone(std::unique_ptr<PropertyGroup>& root)
    {
        observer = nullptr;
        if (depth > 0)
            orphan = std::move(root);
    }

    TreeObserver* observer; // nullptr once the observer is gone
    int depth = 0;

private:
    std::vector<std::unique_ptr<Property>> retired;
    std::unique_ptr<PropertyGroup> orphan; // the tree of a deleted observer
};

// Holds the tree of an observer (see TreeHoldState) for the lifetime of the
// object. Batches of the observer are reached through it, so nothing is called
// on an observer that a handler deleted meanwhile.
class TreeHold
{
public:
    explicit TreeHold(TreeObserver* tree)
        : state(tree ? tree->holdState() : nullptr)
    {
        if (state)
            state->begin();
    }
    ~TreeHold()
    {
        if (state)
            state->end();
    }
    TreeHold(const TreeHold&) = delete;
    TreeHold& operator=(const TreeHold&) = delete;

    // No-ops without an observer, or once it is gone.
    void beginBatch() const
    {
        if (state && state->observer)
            state->observer->beginBatch();
    }
    void endBatch() const
    {
        if (state && state->observer)
            state->observer->endBatch();
    }

private:
    std::shared_ptr<TreeHoldState> state;
};

// A condition on another property's value (1.4), evaluated by the model.
struct PropertyCondition
{
    QString sourcePath; // from the root, like PropertyGroup::find()
    Property::Condition test;
    bool met = true; // last evaluation; true while not evaluated
    bool warned = false; // missing source reported
};

class PropertyPrivate
{
public:
    PropertyPrivate(const TypeId& typeId, const QString& id);

    static PropertyPrivate* get(Property* property)
    {
        return property->d.get();
    }
    static const PropertyPrivate* get(const Property* property)
    {
        return property->d.get();
    }

    // Observer of the tree this property belongs to, or nullptr.
    TreeObserver* observer() const;

    // Converts value to the type's storage type if possible (no validation).
    // Used for initial values.
    QVariant convertInitial(const QVariant& value) const;

    // Who is setting a value: the user through a view (PropertyModel::setData),
    // or application code (Property::setValue, PropertyModel::setValue, resets).
    enum class Origin {
        User,
        Application,
    };

    // The value pipeline (doc/guide/values-and-validation.md). Returns false if the value was
    // rejected; emits notifications through the observer. Read-only and
    // disabled properties only reject values that come from the user.
    bool assign(const QVariant& value, Origin origin);

    void notifyChanged(bool recursive = false);

    // Sets (test non-empty) or clears a condition and lets the model evaluate it.
    void setCondition(std::unique_ptr<PropertyCondition>& slot, const QString& sourcePath,
        Property::Condition test);

    Property* q = nullptr;
    QString id;
    TypeId typeId;
    QString displayName;
    QString toolTip;
    QVariant value;
    QVariant defaultValue;
    QVariantMap attributes;
    Property::Flags flags;
    Property::Validator validator;
    PropertyGroup* parent = nullptr;
    std::unique_ptr<PropertyCondition> enabledWhen; // 1.4
    std::unique_ptr<PropertyCondition> visibleWhen; // 1.4

    // Groups only.
    std::vector<std::unique_ptr<Property>> children;

    // Root only: set by PropertyModel while it owns the tree.
    TreeObserver* treeObserver = nullptr;
};

} // namespace qpb::detail

#endif // QPB_PROPERTY_P_H
