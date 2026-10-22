# API overview

The public classes by header. The headers in `include/qpb/` are the complete and exact reference (every declaration has a
comment); this page tells you where to look and how the parts fit together. Back to the
[documentation index](../README.md#documentation).

All names are in namespace `qpb`. Include `<qpb/qpb.h>` for everything (needs `qpb::widgets`) or `<qpb/qpbcore.h>` for
`qpb::core` only; or include single headers: `<qpb/Name.h>` per class, widgets in `<qpb/widgets/Name.h>`. Only these
headers are public; everything in `qpb::detail` and under `src/` is internal.

| Header | Declares | Library |
|---|---|---|
| `Types.h` | `TypeId`, `Types::` constants | core |
| `Attributes.h` | `Attr::` keys, `EnumOption`, `FileMode` | core |
| `Property.h` | `Property` | core |
| `PropertyGroup.h` | `PropertyGroup` | core |
| `PropertyBuilders.h` | `BoolBuilder`, `IntBuilder`, `Int64Builder`, `DoubleBuilder`, `StringBuilder`, `EnumBuilder`, `FilePathBuilder`, `DirPathBuilder` | core |
| `PropertyModel.h` | `PropertyModel` | core |
| `ValidationResult.h` | `ValidationResult` | core |
| `TypeRegistry.h` | `TypeRegistry`, `TypeHandler` | core |
| `PropertyFilterProxyModel.h` | `PropertyFilterProxyModel` | core |
| `QObjectPropertySource.h` | `QObjectPropertySource` | core |
| `Serialization.h` | `qpb::serialization::toJson`, `fromJson`, `save`, `load` | core |
| `Translations.h` | `loadTranslation()` | core |
| `qpbglobal.h` | version macros, `version()`, deprecation macros | core |
| `widgets/PropertyTreeView.h` | `PropertyTreeView` | widgets |
| `widgets/PropertyFormView.h` | `PropertyFormView` | widgets |
| `widgets/PropertyDelegate.h` | `PropertyDelegate` | widgets |
| `widgets/EditorFactory.h` | `EditorFactory`, `EditorHandler`, `EditorDialogScope` | widgets |

## Types and attributes (`Types.h`, `Attributes.h`)

| Type id (`Types::...`) | Stored as | Builder | Default editor |
|---|---|---|---|
| `Bool` | `bool` | `addBool` | check box (in the tree: the cell's check indicator) |
| `Int` | `int` | `addInt` -> `IntBuilder` | spin box |
| `Int64` | `qint64` | `addInt64` -> `Int64Builder` | 64-bit spin box |
| `Double` | `double` | `addDouble` -> `DoubleBuilder` | double spin box |
| `String` | `QString` | `addString` -> `StringBuilder` | line edit; multi-line text edit with `multiline()` |
| `Enum` | `int` index, or the option's value | `addEnum` -> `EnumBuilder` | combo box |
| `FilePath`, `DirPath` | `QString` | `addFilePath`, `addDirPath` | line edit with a "..." button opening a dialog |
| `Group` | - | `addGroup` | - |

`TypeId` is a `QString`. The `Types::` constants convert to it. Your own types use their own ids; a prefix such as
`"myapp.color"` avoids collisions. Ids that start with `qpb.` and the nine ids above are reserved.

Attributes are stored per property and tune validation, display and editors. Keys a type does not know are ignored, so
your own types can define keys of their own (use a prefix such as `"myapp."`). The standard keys are in `Attr::`:

| Key (`Attr::...`) | Types | Value | Default |
|---|---|---|---|
| `Minimum`, `Maximum` | Int, Int64, Double | same type as the value; values are clamped | the whole range of the type; none for Double |
| `Step` | Int, Int64, Double | spin box step | 1 (1.0) |
| `Decimals` | Double | number of decimals (0 to 15); values are rounded to it | 2 |
| `Prefix`, `Suffix` | Int, Int64, Double | `QString` shown before / after the number | none |
| `MaxLength` | String | maximum number of characters | unlimited |
| `Placeholder` | String | placeholder text of the editor | none |
| `RegularExpression` | String | pattern the whole value must match (`QRegularExpression` syntax) | none |
| `Multiline` | String | `bool`; several lines of text (since 1.1) | `false` |
| `Options` | Enum | `QList<EnumOption>` | - |
| `Filter` | FilePath | file dialog name filter, e.g. `"Images (*.png *.jpg)"` | none |
| `DialogMode` | FilePath | an `int` holding a `FileMode` (`Open`, `Save`) | `Open` |
| `DefaultDir` | FilePath, DirPath | directory the dialog starts in when the value is empty | none |
| `MustExist` | FilePath, DirPath | `bool`; a non-empty value must name an existing file / directory | `false` |
| `EditorId` | any | id of the editor to use for this property instead of its type's | none |

Enum options are `EnumOption { label, value }`; the value is an `int` or a `QString`. `addEnum(id, QStringList labels,
index)` makes options whose values are their indexes and stores the index; `addEnum(id, QList<EnumOption>, value)`
stores the selected option's value.

## Properties (`Property.h`, `PropertyGroup.h`, `PropertyBuilders.h`)

`PropertyGroup::create(id)` makes a detached root group; hand it to a `PropertyModel`. A property is owned by its parent
group, the root by the model. `Property` is not a `QObject` and not meant to be subclassed. Pointers to properties stay
valid until the property is removed or the model's tree is replaced.

**Building a tree** (`PropertyGroup`):

- `addGroup(id)` returns the new `PropertyGroup&`. `addBool`, `addInt`, `addInt64`, `addDouble`, `addString`, `addEnum`,
  `addFilePath` and `addDirPath` return a builder. `add(typeId, id, value)` adds a property of any type, and
  `add(std::unique_ptr<Property>)` takes one made with `Property::create(typeId, id, value)`.
- The initial value is also the default value.
- Ids are non-empty and never contain `/`; an empty id becomes `property` and each `/` becomes `_`, with a warning.
  Adding an id that already exists in the group adds nothing, logs a warning and returns the existing child (for
  `addGroup`, when that child is not a group, the new group gets the first free id `<id>_2`, `<id>_3`, ...). The tree
  stays consistent; nothing asserts.
- Access: `remove(id)` (returns false if there is no such child), `child(id)`, `child(index)`, `children()`,
  `childCount()`, `indexOf(child)`, `find("Transform/x")` (a path relative to the group).

**Builders** are lightweight handles; every setter returns the builder, and a builder converts to `Property&`. Keep the
`Property&`, not the builder. Setters that do not apply to a type are not available on its builder, so a mistake does not
compile.

| Builder | Typed setters |
|---|---|
| all | `displayName`, `toolTip`, `readOnly`, `enabled`, `visible`, `live`, `enabledWhen`, `visibleWhen`, `validator`, `attribute(key, value)`, `editor(editorId)` |
| `IntBuilder`, `Int64Builder` | `range`, `minimum`, `maximum`, `step`, `prefix`, `suffix` |
| `DoubleBuilder` | the same, plus `decimals` |
| `StringBuilder` | `maxLength`, `placeholder`, `regularExpression`, `multiline` |
| `FilePathBuilder` | `filter`, `dialogMode`, `defaultDir`, `mustExist` |
| `DirPathBuilder` | `defaultDir`, `mustExist` |
| `BoolBuilder`, `EnumBuilder` | only the common ones |

**`Property`:**

- Identity: `id()`, `path()` (ids joined with `/`, empty for the root), `typeId()`, `isGroup()`, `parent()` (null for a
  root or detached property), `toGroup()` (null when not a group).
- Presentation: `displayName()` (defaults to the id), `toolTip()` and their setters.
- Value: `value()` (invalid for groups), `setValue()`, `defaultValue()`, `setDefaultValue()`, `isModified()`,
  `resetToDefault()`, `validator()` / `setValidator()`. See [Values and validation](guide/values-and-validation.md).
- Attributes: `attributes()`, `attribute(key, default)`, `hasAttribute()`, `setAttribute()`, `removeAttribute()`.
- State: `flags()`, `setFlags()`, `setFlag(flag, on)` with `Property::Flag::ReadOnly`, `Disabled`, `Hidden`, `Live`; the
  setters `setReadOnly`, `setEnabled`, `setVisible`, `setLive`. These are the property's own flags. The effective state
  also depends on ancestors (and conditions): `isReadOnly()`, `isEnabled()`, `isVisible()`, `isLive()`.
- Conditions: `setEnabledWhen()` / `setVisibleWhen()` in four forms, `clearEnabledWhen()`, `clearVisibleWhen()`;
  `Property::Condition` is a `std::function<bool(const QVariant&)>`. See
  [Changes and conditions](guide/changes-and-conditions.md).
- A setter on a property that belongs to a model updates the model and its views.

## Model (`PropertyModel.h`)

A `QAbstractItemModel` with two columns, `NameColumn` and `ValueColumn`. Any Qt view or proxy model can show it; qpb's
own views add editors and property-specific painting. The model owns the root group. Hidden properties stay in the model
(views hide their rows), so indexes stay stable and proxies work normally.

- Construction and tree: `PropertyModel(std::unique_ptr<PropertyGroup> root, parent)` (or without a root);
  `setRoot()` replaces the tree (pointers into the old tree become invalid); `root()`, `find(path)`,
  `propertyAt(index)`, `indexOf(property, column)`.
- Values: `setValue(path, value)` for application code; `setData()` for views, which also refuses read-only and
  disabled properties; `resetToDefault(index)`; `resetAllToDefault()` (since 1.5).
- Batches: `beginBatch()` / `endBatch()`, nestable.
- Signals: `valueChanged(path, newValue, oldValue)`, `validationFailed(path, rejectedValue, message)`,
  `batchValueChanged(paths)`. They carry paths, not `Property` references.
- `onValueChanged(path, context, handler)` (since 1.4): a callback for one path, or for a group and its descendants (the
  handler then also receives the path).
- Roles: `PropertyRole` (`const Property*`), `TypeIdRole`, `PathRole`, `IsGroupRole`, `IsModifiedRole`,
  `AttributesRole`, `IsVisibleRole`. They work on both columns. Roles from `Qt::UserRole + 1` to `Qt::UserRole + 99` are
  reserved for qpb; your own roles start at `PropertyModel::UserRole`.

| Column | Role | Value |
|---|---|---|
| Name | `Qt::DisplayRole`, `Qt::ToolTipRole` | the display name, the tool tip |
| Value | `Qt::DisplayRole` | the type's display text (a Bool shows its check state instead) |
| Value | `Qt::EditRole` | the value |
| Value | `Qt::CheckStateRole` | Bool only: checked or unchecked |

Item flags: an item is enabled according to `isEnabled()`; the value cell is editable (or checkable for Bool) when the
property is enabled and not read-only and its type is registered. Groups are never editable. The column titles are
"Property" and "Value" (translated).

## Other core classes

- `ValidationResult` (`ValidationResult.h`): `ValidationResult::valid()`, `ValidationResult::error(message)`, `ok`,
  `message`. Converts to `bool` explicitly.
- `TypeRegistry::global()` and `TypeHandler` (`TypeRegistry.h`): `registerType(id, handler)`,
  `registerType<T>(id, handler)` (storage type `T`), `replaceType()`, `handler()`, `contains()`, `types()`. `TypeHandler`
  fields, all optional: `storageType`, `displayText`, `normalize`, `validate`, `toJson`, `fromJson`. See
  [Custom types and editors](guide/custom-types-and-editors.md).
- `PropertyFilterProxyModel` (`PropertyFilterProxyModel.h`, since 1.1): filters by display name for a search box. See
  [Views](guide/views.md).
- `QObjectPropertySource(model)` (`QObjectPropertySource.h`, since 1.2): `addObject(object, parentGroup, id)`,
  `removeObject()`, `groupOf()`, `objects()`, `refresh()`, `model()`, `setTitleProperty()` / `titleProperty()`,
  `setLiveReadOnlyProperties()` / `liveReadOnlyProperties()`. See [QObject properties](guide/qobject-source.md).
- `qpb::serialization` (`Serialization.h`, since 1.2): `toJson(group)`, `fromJson(group, json)`, `save(group, settings)`,
  `load(group, settings)`. See [Saving and loading](guide/serialization.md).
- `loadTranslation(translator, locale)` (`Translations.h`, since 1.8). See [Translations](guide/translations.md).
- `qpbglobal.h`: `QPB_VERSION`, `QPB_VERSION_CHECK(major, minor, patch)`, `QPB_VERSION_STR`, `QPB_VERSION_MAJOR` /
  `_MINOR` / `_PATCH`, `qpb::version()`, `QPB_DEPRECATED` and `QPB_DEPRECATED_X(text)`. See
  [Compatibility](compatibility.md).

## Widgets

- `PropertyTreeView` (a `QTreeView`): `setModel()` (a `PropertyModel` or a proxy of one), `mode()` / `setMode(Mode::Tree |
  Mode::List)`, `nameColumnWidth()` / `setNameColumnWidth()`, `propertyDelegate()`, `tabStopsOnCheckBoxes()` /
  `setTabStopsOnCheckBoxes()` (since 1.5), and the colour properties for style sheets (`groupBackground`,
  `groupForeground`, `modifiedForeground`, `readOnlyForeground`; since 1.6). `mode`, `tabStopsOnCheckBoxes` and the
  colours are `Q_PROPERTY`s.
- `PropertyFormView` (a `QScrollArea`, since 1.1): `setModel()`, `model()`, `editor(path)`, `isExpanded(groupPath)`,
  `setExpanded()`.
- `PropertyDelegate`: the tree view's delegate (Tree and List modes); set it on any `QAbstractItemView` showing a
  `PropertyModel`. `PropertyFormView` has editors of its own.
- `EditorFactory::global()`: `registerEditor(id, handler)`, `replaceEditor()`, `handler()`, `handlerFor(property)`,
  `createEditor(parent, property)`, `contains()`, `editors()`, and the static `notifyCommit(editor)`. `EditorHandler`
  fields: `createEditor`, `setEditorData`, `editorData` (required), `applyAttributes`, `paint` (optional).
- `EditorDialogScope`: keeps an editor open while it shows a dialog.

See [Views](guide/views.md) for how the views behave and [Custom types and editors](guide/custom-types-and-editors.md)
for the factory.
