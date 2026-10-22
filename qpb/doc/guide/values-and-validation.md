# Values and validation

What happens when a value changes, which checks qpb makes, how to add your own, and how defaults and reset work. Back to
the [documentation index](../../README.md#documentation).

## The value pipeline

Every change, from a view or from code, goes through the same steps:

1. The value is converted to the type's storage type (for example a string typed into a spin box becomes an `int`). If
   the conversion fails, the value is rejected.
2. It is normalized: `Int`, `Int64` and `Double` values are clamped to `Attr::Minimum` / `Attr::Maximum`, and `Double`
   values are rounded to `Attr::Decimals`.
3. The type's own validation runs (see the table below).
4. The property's validator runs, if it has one.
5. A value equal to the current one changes nothing and emits nothing.
6. Otherwise the value is stored and the model emits `valueChanged(path, newValue, oldValue)`.

A rejected value (step 1, 3 or 4) keeps the old value and makes the model emit `validationFailed(path, rejectedValue,
message)`; the views show the message as a tool tip. Numbers are clamped rather than rejected, as a spin box does, so
that a value set from code behaves like one typed by the user.

Setting a value on a group, or on a property whose type is not registered, returns `false` and emits nothing.

**Changing an attribute never moves a value.** After `setAttribute()` lowers a maximum or a number of decimals, the
stored value stays as it was, even when it is now outside the range. To bring values inside the current attributes, run
them through the pipeline again (since 1.9):

- `Property::normalizeValue()` does for the current value what `setValue(value())` does: conversion, normalization,
  type validation and the validator, as application code (read-only and disabled properties included). It emits
  `valueChanged` (and no `userValueChanged`) only if the value moved, and returns `false`, keeping the value and emitting
  `validationFailed`, if a check now refuses it; it also returns `false`, silently, for a type that is not registered.
  On a group it normalizes all descendants except [live ones](changes-and-conditions.md#live-values) in one batch and
  returns `false` if any was refused.
- `PropertyModel::normalizeAllValues()` does the same for the whole tree, like `root()->normalizeValue()`; it returns
  `true` for a model without a root.

```cpp
property->setAttribute(qpb::Attr::Maximum, 100);
property->normalizeValue();     // 150 becomes 100
model.normalizeAllValues();     // after rebuilding all the ranges
```

## Who may change what

`readOnly` and `disabled` stop the user only: edits made through a view are refused. Application code can still change
such properties with `PropertyModel::setValue()`, `Property::setValue()` and `resetToDefault()`, for example a read-only
status field that your program maintains. Both functions return `bool`: `true` when the value was accepted (or was
already the current one), `false` when it was rejected.

## Built-in checks

| Type | Check |
|---|---|
| Int, Int64 | None needed: the value is clamped into the range, so it is always valid. |
| Double | The value is not NaN (the message is "The value is not a number"); any other number is clamped into the range. |
| String | At most `Attr::MaxLength` characters; the whole value matches `Attr::RegularExpression`. |
| Enum | The value is one of the options (`Attr::Options`). |
| FilePath | With `Attr::MustExist` and `FileMode::Open`, a non-empty value must name an existing file. An empty string is always valid. |
| DirPath | With `Attr::MustExist`, a non-empty value must name an existing directory. An empty string is always valid. |
| Bool | Conversion only. |

The messages of these checks, and of a value that cannot be converted, are translated (see
[Translations](translations.md)). Their wording is not part of the API: compare `ValidationResult::ok`, never the text
of a message.

## Your own validator

A validator runs after the type's checks. It receives the candidate value and the property, so it can read attributes
or look at siblings. Return `ValidationResult::valid()` or `ValidationResult::error(message)`:

```cpp
root->addString("name", "Main").validator([&scene](const QVariant& v, const qpb::Property&) {
    return scene.contains(v.toString()) ? qpb::ValidationResult::error("Name already used")
                                        : qpb::ValidationResult::valid();
});
```

Pass the message already translated (for example with `tr()`). A validator can also be set later with
`Property::setValidator()`. To check values of a whole type, register a type with a `validate` function instead; see
[Custom types and editors](custom-types-and-editors.md).

## Defaults and reset

The default value is the value the property had when it was created; change it with `setDefaultValue()`.
`isModified()` is true while the value differs from the default; the views show the names of modified properties in
bold. Groups are never modified, and neither are [live properties](changes-and-conditions.md#live-values).

- `Property::resetToDefault()` sets the value back through the pipeline above and returns `false` if the reset was
  rejected. On a group it resets all descendants except live ones.
- `PropertyModel::resetToDefault(index)` does the same for the property or group at an index and emits one
  `batchValueChanged`. An invalid index, including the one of the root, returns `false`.
- `PropertyModel::resetAllToDefault()` (since 1.5) resets the whole tree like `root()->resetToDefault()`: read-only and
  disabled properties too, live ones not, one `batchValueChanged`. It returns `false` if a reset was rejected, and
  `true` for a model without a root.
- The views offer "Reset to default" for a property and "Reset group" for a group in their context menus.
- `Property::makeValueDefault()` (since 1.9) makes the current value the default: `defaultValue()` becomes `value()`
  and the property is no longer modified, for example after loading settings, to show later edits as modified relative
  to that load. It changes no value, so it emits no `valueChanged`; the views only repaint the rows whose default
  changed. On a group it applies to all descendants except live ones.
- `PropertyModel::makeAllValuesDefault()` (since 1.9) does this for the whole tree, like `root()->makeValueDefault()`.
