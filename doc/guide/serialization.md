# Saving and loading

Writing property values to JSON or `QSettings` and reading them back (since 1.2). Back to the
[documentation index](../../README.md#documentation).

```cpp
#include <qpb/Serialization.h>

qpb::serialization::save(*model.root(), settings);   // keys are paths: "General/language"
model.beginBatch();
qpb::serialization::load(*model.root(), settings);   // values go through validation
model.endBatch();
```

The four functions work on a group and its descendants:

| Function | Does |
|---|---|
| `QJsonObject toJson(const PropertyGroup&)` | Every group becomes a nested object keyed by its id, every other property its value |
| `bool fromJson(PropertyGroup&, const QJsonObject&)` | Sets the values found in the object |
| `void save(const PropertyGroup&, QSettings&)` | Writes one key per value under the settings' current group; keys are paths relative to the group (`"General/language"`) |
| `bool load(PropertyGroup&, const QSettings&)` | Sets the values stored by `save()` |

## What is stored

- Only values are stored, never the structure. The application builds the tree first, then loads values into it.
- Read-only properties and [live properties](changes-and-conditions.md#live-values) are neither written nor read: the
  application maintains them, and loading would overwrite, say, the running version with a stored one. Hidden and
  disabled properties are stored.
- Groups with nothing to store (empty, or holding only read-only properties) are left out.
- Values are restored with `Property::setValue()`, so they are converted, clamped and validated like any other value
  your code sets (see [Values and validation](values-and-validation.md)).
- Keys with no matching property, and properties with no key, are ignored: the property keeps its value.
- `fromJson()` and `load()` return `false` if a value was rejected. The rejected property keeps its value and the
  other values are still applied.
- Wrap loading in `beginBatch()` / `endBatch()` to get one `batchValueChanged` instead of many signals.

## JSON form of a value

By default a value is written with `QJsonValue::fromVariant()` and read with `QJsonValue::toVariant()`. `Int64` writes
numbers up to 2^53 as JSON numbers and larger values as strings, so no precision is lost. A type that needs another form
sets `TypeHandler::toJson` and `TypeHandler::fromJson`; see
[Custom types and editors](custom-types-and-editors.md). For example, a `QColor` type can be stored as `"#ff0000"`.

## Call the functions qualified

The functions live in the namespace `qpb::serialization`, so write `qpb::serialization::save(...)`. They are in a
nested namespace on purpose: argument-dependent lookup does not search it, so qpb can never make an unqualified call to
your own `save(group, settings)` ambiguous, now or after an upgrade.
