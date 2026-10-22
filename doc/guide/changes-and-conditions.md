# Changes and conditions

Reacting to value changes, making one property depend on another, and marking values that your application maintains.
Back to the [documentation index](../../README.md#documentation).

## Reacting to changes

The model announces changes with signals. They carry the path of the property (a `QString`), not a reference to it, so a
path you store stays meaningful when the tree changes; use `model.find(path)` to get the property.

| Signal | When |
|---|---|
| `valueChanged(path, newValue, oldValue)` | A value changed, from any source: the user, `setValue()`, a reset, loading |
| `validationFailed(path, rejectedValue, message)` | A value was rejected; the old value is kept |
| `batchValueChanged(paths)` | The outermost `endBatch()`, once, with the paths of everything that changed in the batch |

Prefer `onValueChanged()` (since 1.4) to a `valueChanged` handler that compares paths:

```cpp
model.onValueChanged("Transform/x", this, [this](const QVariant& v) { m_object.x = v.toDouble(); });
model.onValueChanged("Output", this, [](const QString& path, const QVariant& v) { /* any child */ });
```

- The first form is called with the new value of one property. The second is given the path of a group (the empty path
  means the whole tree) and is called for the group's descendants, with the path of the one that changed.
- It works like `QObject::connect`: the connection ends when `context` is destroyed (pass `nullptr` to tie it to the model)
  and can be disconnected through the returned `QMetaObject::Connection`.
- It follows the path, not a `Property` object, so it keeps working after `setRoot()` and when a property is removed and
  added again.
- It is called after `valueChanged`, and after the conditions below have been evaluated, so the handler sees the new
  enabled and visible states.

**Batches.** `valueChanged` is emitted for every change. To react once to a bulk update, such as loading settings or a
group reset, wrap it in `beginBatch()` / `endBatch()`; the outermost `endBatch()` emits `batchValueChanged` with all
paths. Batches nest.

```cpp
model.beginBatch();
qpb::serialization::load(*model.root(), settings);
model.endBatch();
```

**Changing the tree while it is shown.** Properties can be added to and removed from groups while a view shows the
model; the views follow. Raw `Property*` pointers die with the removal of the property or with `setRoot()`; keep paths
instead if you need to refer to a property later.

## Dependencies between properties

A property can be enabled or shown depending on the value of another one, identified by its path from the root:

```cpp
general.addBool("autosave", true);
general.addInt("autosaveMinutes", 5).enabledWhen("General/autosave");              // true / non-empty
camera.addDouble("orthoScale", 1.0).visibleWhen("Camera/projection", 1);            // equals a value
limits.addInt64("quota", 0).enabledWhen("Limits/mode", [](const QVariant& v) { return v != "unlimited"; });
```

`enabledWhen()` and `visibleWhen()` (since 1.4) exist on every builder and as `Property::setEnabledWhen()` /
`setVisibleWhen()`, with three forms:

| Form | The condition is met when the source value |
|---|---|
| `enabledWhen(path)` | is "true": a true `bool`, a non-zero number, a non-empty string; for other types, valid and not null |
| `enabledWhen(path, value)` | equals `value` (an `int` overload keeps a literal `0`, such as an enum index, from being taken for the predicate form) |
| `enabledWhen(path, condition)` | makes the `Property::Condition` predicate return true |

A property has at most one condition of each kind (enabled, visible); setting another replaces it. Remove one with
`clearEnabledWhen()` / `clearVisibleWhen()`.

- The model evaluates conditions when a source value changes, when properties are added or removed, when a condition is
  set, and when a tree is attached with `setRoot()`. Views need no code: a property that is not enabled cannot be
  edited, and one that is not visible is hidden.
- The result is combined with the property's own flags, so a condition never overwrites `setEnabled()` or
  `setVisible()`: a property is enabled when it is not `Disabled`, its condition is met and its ancestors are enabled.
  `isEnabled()` and `isVisible()` give this effective state.
- A condition counts as met when the source path does not exist and for a property outside a model. When a tree is
  attached to a model and a source path is missing, a warning is logged once.
- Conditions of properties shown from a `QObject` are set with class info; see
  [QObject properties](qobject-source.md).

## Live values

A property maintained by the application (a counter, a status, the space used) is not a setting. Mark it `live()`
(since 1.3):

```cpp
status.addInt64("bytesUsed", 0).readOnly().live();
```

- It is never shown as modified, so the views never show it in bold.
- Resets of a group, of the whole tree and "Reset group" skip it. `resetToDefault()` called on the live property itself
  still resets it.
- `qpb::serialization` neither writes nor reads it.
- It stays editable unless it is also read-only; the flag is inherited by the children of a live group, and
  `isLive()` gives the effective state.
- Your program updates it with `setValue()`, which works on read-only properties too.
