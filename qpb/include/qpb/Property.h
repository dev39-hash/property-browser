#ifndef QPB_PROPERTY_H
#define QPB_PROPERTY_H

#include <qpb/Types.h>
#include <qpb/ValidationResult.h>
#include <qpb/qpbglobal.h>

#include <QtCore/qflags.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtCore/qvariantmap.h>

#include <functional>
#include <memory>

namespace qpb {

class PropertyGroup;

namespace detail {
class PropertyPrivate;
}

// A node of the property tree: one editable value, or a PropertyGroup
// (docs/SPEC.md section 4.2).
//
// Properties are created through PropertyGroup::add*() or Property::create()
// and owned by their parent group; the root group is owned by the
// PropertyModel it is attached to. Property is not a QObject and is not meant
// to be subclassed by applications.
//
// Every value change - from the UI, from PropertyModel::setValue() or from
// setValue() below - goes through the same pipeline: conversion to the type's
// storage type, normalization, type validation, the property's own validator,
// and change notification through the attached PropertyModel.
class QPB_CORE_EXPORT Property
{
public:
    // Per-property state. Effective state also depends on the ancestors, see
    // isReadOnly(), isEnabled() and isVisible().
    enum class Flag {
        ReadOnly = 0x1, // value cannot be edited
        Disabled = 0x2, // shown greyed out, cannot be edited
        Hidden = 0x4, // not shown by views (still part of the model)
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    // Extra validation run after the type's own validation. Returning an error
    // rejects the value; the old value is kept.
    using Validator
        = std::function<ValidationResult(const QVariant& value, const Property& property)>;

    // Creates a detached property of any registered (or not yet registered)
    // type. Attach it with PropertyGroup::add(std::unique_ptr<Property>).
    // While its type is not registered the value is stored as given and
    // setValue() fails; once the type is registered the property behaves
    // like any other.
    // For the built-in types prefer the typed PropertyGroup::add*() functions.
    static std::unique_ptr<Property> create(
        const TypeId& type, const QString& id, const QVariant& value = QVariant());

    virtual ~Property();

    Property(const Property&) = delete;
    Property& operator=(const Property&) = delete;

    // --- identity ----------------------------------------------------------

    // Identifier, unique among the siblings. Never contains '/'.
    QString id() const;
    // IDs from the root's child down to this property joined with '/', e.g.
    // "Transform/x". Empty for the root group.
    QString path() const;
    TypeId typeId() const;
    bool isGroup() const;

    // Parent group, or nullptr for a root or detached property.
    PropertyGroup* parent() const;
    // This property as a group, or nullptr if it is not a group.
    PropertyGroup* toGroup();
    const PropertyGroup* toGroup() const;

    // --- presentation --------------------------------------------------------

    // Text shown in views. Defaults to id().
    QString displayName() const;
    void setDisplayName(const QString& name);

    QString toolTip() const;
    void setToolTip(const QString& toolTip);

    // --- value -------------------------------------------------------------

    // Current value. Always invalid for groups.
    QVariant value() const;
    // Runs the value pipeline (see class comment). Returns false and keeps the
    // old value if the property is read-only or disabled, its type is not
    // registered, or conversion/validation fails.
    bool setValue(const QVariant& value);

    // Value restored by resetToDefault(). Defaults to the value at creation.
    QVariant defaultValue() const;
    void setDefaultValue(const QVariant& value);
    // True when value() differs from defaultValue(). Always false for groups.
    bool isModified() const;
    // Sets the value back to defaultValue() through the value pipeline. For a
    // group, resets all descendants. Returns false if any reset was rejected.
    bool resetToDefault();

    // Optional extra validation, run after the type's validation.
    Validator validator() const;
    void setValidator(Validator validator);

    // --- attributes (see Attr) -----------------------------------------------

    QVariantMap attributes() const;
    QVariant attribute(const QString& key, const QVariant& defaultValue = QVariant()) const;
    bool hasAttribute(const QString& key) const;
    void setAttribute(const QString& key, const QVariant& value);
    void removeAttribute(const QString& key);

    // --- state ---------------------------------------------------------------

    // Flags set on this property itself (ancestors not included).
    Flags flags() const;
    void setFlags(Flags flags);
    void setFlag(Flag flag, bool on = true);

    // Convenience setters for the corresponding own flag.
    void setReadOnly(bool readOnly);
    void setEnabled(bool enabled);
    void setVisible(bool visible);

    // Effective state: read-only if this property or any ancestor is read-only.
    bool isReadOnly() const;
    // Effective state: enabled only if this property and all ancestors are enabled.
    bool isEnabled() const;
    // Effective state: visible only if this property and all ancestors are visible.
    bool isVisible() const;

protected:
    explicit Property(std::unique_ptr<detail::PropertyPrivate> d);

    detail::PropertyPrivate* d_func()
    {
        return d.get();
    }
    const detail::PropertyPrivate* d_func() const
    {
        return d.get();
    }

private:
    std::unique_ptr<detail::PropertyPrivate> d;

    friend class detail::PropertyPrivate;
};

} // namespace qpb

Q_DECLARE_OPERATORS_FOR_FLAGS(qpb::Property::Flags)
Q_DECLARE_METATYPE(const qpb::Property*)

#endif // QPB_PROPERTY_H
