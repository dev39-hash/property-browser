# QObject properties

Showing the `Q_PROPERTY`s of your own objects in a model and keeping both sides in sync (since 1.2). Back to the
[documentation index](../../README.md#documentation).

`QObjectPropertySource` adds one group per object to a `PropertyModel`, with one property per `Q_PROPERTY`. Edits in the
model are written to the object; changes of the object appear in the model. You do not change the class you show: the
optional metadata is `Q_CLASSINFO`, which a class can add without touching its logic.

```cpp
class Light : public QObject {
    Q_OBJECT
    Q_CLASSINFO("qpb:title", "name")                         // group title follows this Q_PROPERTY
    Q_CLASSINFO("qpb:intensity", "min=0;max=100;suffix= %")
    Q_CLASSINFO("qpb:used", "live")
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    ...
};
qpb::QObjectPropertySource source(&model);
source.addObject(&light);
```

The model is not owned by the source and must outlive it.

## Which properties are shown

- By default the properties declared by the object's class and its base classes, except those of `QObject` itself
  (`objectName`). `Q_CLASSINFO("qpb:properties", "name,fov,lut")` selects them and sets their order.
- Supported types: `bool` (Bool), `int` (Int), `qint64` (Int64), `double` and `float` (Double), `QString` (String),
  enumerations declared with `Q_ENUM` (Enum, with the keys as labels and the integer values), and any other type that is
  registered in `TypeRegistry` for the property's `QMetaType`. Other types, flags included, are skipped.
- A `Q_PROPERTY` without a `WRITE` accessor becomes read-only.
- The values the object has when it is added are the default values.

## Metadata

`Q_CLASSINFO("qpb:<property>", "key=value;key=value")` describes one property. A key without `=value` means true.

| Key | Effect |
|---|---|
| `type` | The type id to use instead of the one derived from the C++ type, for example `type=filepath` |
| `displayName`, `toolTip` | Texts |
| `readOnly`, `hidden`, `disabled` | State flags |
| `exclude` | Do not show this property |
| `live` | A live value (since 1.3); see [Changes and conditions](changes-and-conditions.md#live-values) |
| `enabledWhen`, `visibleWhen` | The name of another `Q_PROPERTY` of the same object; this property is enabled / shown while that one is "true" (since 1.4); see [Changes and conditions](changes-and-conditions.md#dependencies-between-properties) |
| any attribute key | An attribute such as `suffix`, `step`, `decimals`, `filter` (`min` and `max` stand for `minimum` and `maximum`) |

```cpp
Q_CLASSINFO("qpb:fov", "min=10;max=170;suffix= deg;displayName=Field of view")
Q_CLASSINFO("qpb:lut", "type=filepath;filter=LUT (*.cube)")
```

Class info texts such as `displayName=` and `toolTip=`, and the keys of `Q_ENUM`s, are used as written. To translate
them, set them again with `setDisplayName()` / `setToolTip()` after `addObject()`; see
[Translations](translations.md).

## Titles of groups

The group's id, and so every path, is the object name; this keeps paths stable for saved files and application code.
The group's display name can follow a `Q_PROPERTY` instead (since 1.3):

- `Q_CLASSINFO("qpb:title", "name")` in the class, or
- `source.setTitleProperty("name")` for classes you cannot change, for objects added afterwards. The class info wins.

The title is updated through the property's `NOTIFY` signal; an empty value keeps the previous title.

## Live read-only properties

`setLiveReadOnlyProperties(true)` (since 1.3) makes every `Q_PROPERTY` of the objects added afterwards that has a `NOTIFY`
signal but no `WRITE` accessor a live property: a value the object maintains, never shown as modified and not saved. It
is off by default, which keeps such properties as plain read-only ones.

## Adding, removing and keeping in sync

- `addObject(object, parentGroup, id)` adds a group and returns it. The parent defaults to the model's root (a model
  without a root gets an empty one); a given parent group must belong to the model. The group's id is `id`, else the
  object name, else the class name, made unique with a suffix `_2`, `_3`, ... Adding an object again returns its
  existing group. It returns `nullptr` for a null object.
- `removeObject(object)` removes the group and stops synchronizing it; it returns `false` if the object was not added.
  `objects()` lists the objects and `groupOf(object)` finds a group.
- Edits in the model are written with `QMetaProperty::write`. If the object adjusts or refuses the value, the model
  shows the object's value.
- Changes of the object are read back through its `NOTIFY` signal. For a `Q_PROPERTY` without one, call `refresh()`,
  which reads every property of every object again.
- When an object is destroyed its group is removed. When the source is destroyed the groups stay in the model and the
  synchronization stops.
