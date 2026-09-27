# qpb — Property Browser for Qt 6: Technical Specification (1.0 → 1.2)

> Source: [`brainstorm-vi.md`](brainstorm-vi.md) (direction B — *explicit model + pluggable views/types*).
> Vietnamese translation: [`SPEC-vi.md`](SPEC-vi.md). This English document is authoritative.
> Status: **Draft 3** — C++17, Qt 6.5, namespace `qpb` and CMake-only are confirmed. Items still marked
> **[Assumption]** are unconfirmed; when one changes, update that item and [Appendix B](#appendix-b--decision-log).

---

## 1. Goals and non-goals

### 1.1 Goals

- **G1.** Developers *declare* a property tree once (builder API) and get a complete editing panel.
- **G2.** One data source (`PropertyModel`, a standard `QAbstractItemModel`), several presentations: **Tree**, **List**, **Form**.
- **G3.** Data types and editors are extended **from the user side**, without modifying the library.
- **G4.** Fast integration: a 10-property panel in ≤ 30 lines of code; ≤ 30 minutes including CMake.
- **G5. Stable API.** From 1.0 on, upgrading the library **never forces consuming projects to change code**
  (within the same major version). Detailed policy in §9.
- **G6. Distributed as a component folder.** A consuming project copies the `qpb/` folder in
  (e.g. `components/qpb/`) and adds two lines of CMake. Updating = replace the folder + rebuild. Details in §6.
- **G7. Written from scratch.** No wrapping, no forking, no code copied from QtPropertyBrowser/QtnProperty.

### 1.2 Non-goals (not in 1.x)

Qt Quick/QML views · Python bindings · multi-object editing · built-in undo/redo ·
compound types in core (color, font, vector, array) · Qt 5 · DSL/code generator ·
custom theming system (QSS is enough) · declarative conditional visibility expressions · qmake support.

Rationale: brainstorm section 11. The design **must not preclude** multi-object editing or QML later
(→ core does not depend on QtWidgets, `Property` does not assume exactly one source object).

### 1.3 Project-level acceptance criteria

| #  | Criterion                                                        | Verified by                                            |
|----|------------------------------------------------------------------|--------------------------------------------------------|
| S1 | 10-property panel in ≤ 30 lines                                  | `examples/quickstart/main.cpp` (non-blank lines, excluding comments and `#include`s) |
| S2 | `QColor` type in ≤ 100 lines, outside the library                 | `examples/custom_type/ColorType.{h,cpp}` (same counting rule as S1) |
| S3 | Switching Tree ↔ List ↔ Form does not change the model            | `examples/inspector` view switcher + test              |
| S4 | The three reference scenarios (`docs/use-cases.md`) are implementable with the public API only | Standalone RC application (PLAN RC.1); real projects tracked outside the repo when they exist |
| S5 | Upgrading 1.x → 1.y requires no consumer code changes             | API compatibility tests (§9.5) pass on every 1.y       |
| S6 | Updating the library = replace `components/qpb/` + rebuild        | `tests/consumer` rebuilds after the folder is replaced, with no change to the consumer's CMake |

---

## 2. Technical constraints

| Area            | Decision                                                                                     |
|-----------------|----------------------------------------------------------------------------------------------|
| Language        | **C++17** (confirmed). The public API does **not** rely on designated initializers (C++20)   |
| Qt              | Minimum **6.5** (confirmed); primary development/CI version **6.8 LTS**. Only Core, Gui, Widgets, Test |
| Build           | **CMake only** (confirmed; no qmake), CMake ≥ 3.21; targets `qpb::core`, `qpb::widgets`. **Primary** integration: `add_subdirectory(components/qpb)` (§6) |
| Namespace       | `qpb` (confirmed)                                                                             |
| Library type    | **Static by default** (`QPB_BUILD_SHARED=OFF`), independent of the consumer's `BUILD_SHARED_LIBS`; export macros `QPB_CORE_EXPORT`, `QPB_WIDGETS_EXPORT` still exist for the shared case |
| License         | MIT [Assumption]                                                                              |
| Platforms       | Linux, Windows (MSVC 2019+), macOS                                                            |
| Dependencies    | `qpb::core` → Qt6::Core **only**. `qpb::widgets` → `qpb::core`, Qt6::Widgets. No third-party libraries |
| Raising minimums| Raising the C++ standard, minimum Qt or minimum CMake is a **breaking change** (new major only) |
| Language of code/docs | English. Files with a `-vi` suffix are Vietnamese reference translations              |

---

## 3. Architecture

```text
┌─────────────────────────────── qpb::widgets (Qt6::Widgets) ───────────────────────────────┐
│  PropertyTreeView ──┐                                                                     │
│   (mode Tree/List)  ├── PropertyDelegate ──┐                                              │
│                     │                      ├── EditorFactory  (typeId → EditorHandler)    │
│  PropertyFormView ──┴──────────────────────┘                                              │
├─────────────────────────────── qpb::core (Qt6::Core only) ────────────────────────────────┤
│  PropertyModel : QAbstractItemModel                                                       │
│  Property / PropertyGroup (data tree)       TypeRegistry (typeId → TypeHandler)           │
│  Attributes (standard keys)   Validation                                                  │
└───────────────────────────────────────────────────────────────────────────────────────────┘
```

**Architecture rules (mandatory, checked in review):**

- **R1.** Views never `switch`/`if` on `typeId` or `QMetaType`. All type-dependent behaviour goes through `TypeRegistry` or `EditorFactory`.
- **R2.** `qpb::core` includes no QtWidgets/QtGui header (`Qt::ItemDataRole` lives in QtCore).
- **R3.** Every value change coming from the UI goes through `PropertyModel::setData` (→ validation → signal). Views never write to a `Property` directly.
- **R4.** No public API without at least one user (example or test).
- **R5.** Only headers under `qpb/include/qpb/` are public. Everything in namespace `qpb::detail` or under `src/`
  is internal and may change freely. Every change to a public header must follow §9.
- **R6.** Every file in `qpb/` is ASCII only. MSVC on a non-UTF-8 code page warns (C4819) about other characters,
  which breaks consumers building with `/WX`. (R1, R2, R5 and R6 are checked by `tools/check_architecture.cmake`.)

---

## 4. Core (`qpb::core`)

> Since M1 the public headers in `qpb/include/qpb/` are the **normative API reference**; the code in §4–§5 summarizes them.
> The M1 API review is recorded in [`api-review.md`](api-review.md) and Appendix B (D21–D28).

### 4.1 Type identifiers (`TypeId`)

Types are identified by a **logical ID** (string), not by `QMetaType`, because several logical types share one
storage type (`String`, `FilePath`, `DirPath` are all `QString`).

```cpp
namespace qpb {
using TypeId = QString;
namespace Types {
inline constexpr QLatin1StringView Bool{"bool"};
inline constexpr QLatin1StringView Int{"int"};
inline constexpr QLatin1StringView Double{"double"};
inline constexpr QLatin1StringView String{"string"};
inline constexpr QLatin1StringView Enum{"enum"};
inline constexpr QLatin1StringView FilePath{"filepath"};
inline constexpr QLatin1StringView DirPath{"dirpath"};
inline constexpr QLatin1StringView Group{"group"};
}
}
```

User-registered types use IDs of their choosing; a prefix is recommended (`"myapp.color"`) to avoid collisions.
IDs starting with `qpb.` or equal to one of the 8 IDs above are reserved.

### 4.2 `Property`

A node in the tree. Not a `QObject` (lightweight, no moc). Owned by its parent group
(`std::unique_ptr`); the root is owned by `PropertyModel` once attached.
Data lives behind a **d-pointer** (`std::unique_ptr<detail::PropertyPrivate>`); the header only has functions (see §9.3).
Non-copyable; no public constructor (created via `PropertyGroup::add*` or `Property::create`).

| Member            | Type                   | Notes                                                                   |
|-------------------|------------------------|-------------------------------------------------------------------------|
| `id()`            | `QString`              | Required, unique within its parent group; must not contain `/`          |
| `path()`          | `QString`              | `id`s joined from the root's child down: `"Transform/x"`; the root's path is empty |
| `displayName()`   | `QString`              | Defaults to `id`                                                        |
| `typeId()`        | `TypeId`               | Immutable after creation                                                |
| `value()`         | `QVariant`             | Groups: always invalid                                                  |
| `defaultValue()`  | `QVariant`             | Defaults to the value at creation                                       |
| `attributes()`    | `QVariantMap`          | See 4.4                                                                 |
| `toolTip()`       | `QString`              |                                                                         |
| `isReadOnly()`    | `bool`                 | Effective = itself **or** any ancestor is read-only                     |
| `isEnabled()`     | `bool`                 | Effective = itself **and** all ancestors are enabled                    |
| `isVisible()`     | `bool`                 | Hiding a group hides all its children                                   |
| `parent()`        | `PropertyGroup*`       | `nullptr` for the root                                                  |
| `isModified()`    | `bool`                 | `value() != defaultValue()` (`QVariant` comparison)                     |
| `validator()`     | `Property::Validator` = `std::function<ValidationResult(const QVariant&, const Property&)>` | Optional, runs after type validation |
| `flags()`         | `Property::Flags` (`ReadOnly`, `Disabled`, `Hidden`) | Own state; `isReadOnly/isEnabled/isVisible` above are effective |

The corresponding setters (`setDisplayName`, `setToolTip`, `setReadOnly`, `setEnabled`, `setVisible`,
`setAttribute`, `setValidator`, `setDefaultValue`) **notify the model** when the property is attached to one
(see 4.6), so views update.

`Property::setValue()` is the **application-code API** (e.g. loading data). It runs through the same
conversion, validation and signal pipeline as `PropertyModel::setData` (R3) and returns `bool`. Read-only and disabled
restrict **the user only** (edits through views, i.e. `setData`): application code can still set, and reset, the value of
such properties, e.g. a read-only status field it maintains. [D34]

### 4.3 `PropertyGroup` and the builder API

`PropertyGroup : Property` with `typeId() == Types::Group`, holding an ordered list of children.

```cpp
class PropertyGroup : public Property {
public:
    static std::unique_ptr<PropertyGroup> create(const QString& id);

    PropertyGroup&  addGroup(const QString& id);
    BoolBuilder     addBool  (const QString& id, bool value);
    IntBuilder      addInt   (const QString& id, int value);
    DoubleBuilder   addDouble(const QString& id, double value);
    StringBuilder   addString(const QString& id, const QString& value);
    EnumBuilder     addEnum  (const QString& id, const QStringList& labels, int currentIndex);
    EnumBuilder     addEnum  (const QString& id, const QList<EnumOption>& options, const QVariant& value);
    FilePathBuilder addFilePath(const QString& id, const QString& path);
    DirPathBuilder  addDirPath (const QString& id, const QString& path);
    Property&       add(const TypeId& type, const QString& id, const QVariant& value); // custom type
    Property&       add(std::unique_ptr<Property> property);

    bool remove(const QString& id);
    int  childCount() const;
    Property* child(int index) const;
    Property* child(const QString& id) const;
    QList<Property*> children() const;
    int  indexOf(const Property* child) const;
    Property* find(const QString& path) const;   // "Transform/x"
};
```

- Adding a duplicate `id` to the same group logs a warning and returns the existing property (nothing new is created); `addGroup()` over a
  non-group child adds the group under the first free id (`<id>_2`, ...). Empty ids and ids containing `/` are sanitized with a warning. [D29]
- `PropertyBuilder<T>` is a thin wrapper around `Property&` whose setters return `*this`:
  `displayName`, `toolTip`, `readOnly`, `enabled`, `visible`, `validator`, plus type-specific setters
  (`range`, `minimum`, `maximum`, `step`, `decimals`, `prefix`, `suffix`, `maxLength`, `placeholder`, `regularExpression`, `filter`, `dialogMode`, `defaultDir`, `mustExist`), plus `attribute` and `editor` for any type.
  Builders are concrete classes (`BoolBuilder`, `IntBuilder`, `DoubleBuilder`, `StringBuilder`, `EnumBuilder`, `FilePathBuilder`, `DirPathBuilder`)
  sharing `PropertyBuilderBase<Derived>`; a setter that does not apply to the type (e.g. `regularExpression` on an int) **does not compile**.
- A builder converts implicitly to `Property&`.

### 4.4 Standard attributes

Keys are the constants `qpb::Attr::*` (`QString`). Unknown attributes are ignored (no error), so custom types can have their own.

| Type       | Attributes (value type)                                                                  | Default               |
|------------|-------------------------------------------------------------------------------------------|-----------------------|
| Int        | `minimum`(int) `maximum`(int) `step`(int) `prefix` `suffix`                                | INT_MIN / INT_MAX / 1 |
| Double     | `minimum` `maximum` `step`(double) `decimals`(int) `prefix` `suffix`                       | −∞ / +∞ / 1.0 / 2     |
| String     | `maxLength`(int) `placeholder` `regularExpression`(QString) `multiline`(bool, **1.1**)    | unlimited |
| Enum       | `options`(`QList<EnumOption>` — `{QString label; QVariant value;}`)                         | —                     |
| FilePath   | `filter`(QString) `dialogMode`(int, `qpb::FileMode::Open`/`Save`) `defaultDir` `mustExist`(bool) | `Open`, false |
| DirPath    | `defaultDir` `mustExist`(bool)                                                             | false                 |
| (any type) | `editorId`(TypeId) — overrides the editor for this property only (see 5.2)                  | —                     |

**Enum:** `value()` is the `value` of the selected option (int **or** QString, chosen by overload).
The overload `addEnum(id, QStringList labels, int index)` creates options with `value = index`.

**64-bit integers:** 1.0 supports only `int` (`QSpinBox` is `int`). `qint64` arrives in 1.2 as a **new type**
`Types::Int64` (the behaviour of `Int` does not change → not breaking).

### 4.5 `TypeRegistry` (UI-free part)

```cpp
struct TypeHandler {
    QMetaType storageType;                                                // invalid → stored unconverted
    std::function<QString(const QVariant&, const Property&)> displayText; // empty → QVariant::toString()
    std::function<QVariant(const QVariant&, const Property&)> normalize;  // empty → unchanged (e.g. clamp)
    std::function<ValidationResult(const QVariant&, const Property&)> validate; // empty → always valid
};

class TypeRegistry {
public:
    static TypeRegistry& global();
    bool registerType(const TypeId& id, const TypeHandler& h); // false if id exists or is reserved
    template <class T> bool registerType(const TypeId& id, TypeHandler h); // sets storageType to T
    bool replaceType (const TypeId& id, const TypeHandler& h); // false if id is not registered
    bool contains(const TypeId& id) const;
    const TypeHandler* handler(const TypeId& id) const;        // nullptr if not registered
    QList<TypeId> types() const;                               // registration order
};
```

- The 7 basic types are registered the first time `TypeRegistry::global()` is called.
- Handlers are configured by assigning fields (C++17), not with designated initializers.
- `TypeRegistry` is **not thread-safe**; register on the main thread before creating models.
- A property whose `typeId` is not registered is still shown (text = `QVariant::toString()`), **read-only**, with a single `qWarning`.

### 4.6 `PropertyModel`

```cpp
class PropertyModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column { NameColumn = 0, ValueColumn = 1 };
    enum Role {
        PropertyRole = Qt::UserRole + 1, // Property* (const)
        TypeIdRole,                      // QString
        PathRole,                        // QString
        IsGroupRole,                     // bool
        IsModifiedRole,                  // bool
        AttributesRole,                  // QVariantMap
        IsVisibleRole,                   // bool (effective, ancestors included)
        // Qt::UserRole+1 … Qt::UserRole+99 are reserved for qpb (new roles are appended, numbers never change)
        UserRole = Qt::UserRole + 100    // application roles start here
    };

    explicit PropertyModel(QObject* parent = nullptr);
    explicit PropertyModel(std::unique_ptr<PropertyGroup> root, QObject* parent = nullptr);

    void setRoot(std::unique_ptr<PropertyGroup> root);   // beginResetModel/endResetModel
    PropertyGroup* root() const;
    Property* propertyAt(const QModelIndex& idx) const;
    QModelIndex indexOf(const Property* p, int column = NameColumn) const;
    Property* find(const QString& path) const;

    bool setValue(const QString& path, const QVariant& v);
    bool resetToDefault(const QModelIndex& idx);          // group ⇒ recursive reset

    void beginBatch();                                    // nestable
    void endBatch();

signals:
    void valueChanged(const QString& path, const QVariant& newValue, const QVariant& oldValue);
    void validationFailed(const QString& path, const QVariant& rejected, const QString& message);
    void batchValueChanged(const QStringList& paths);     // emitted only by the outermost endBatch()
};
```

**Data → role mapping:**

| Column | Role                   | Value                                                                    |
|--------|------------------------|--------------------------------------------------------------------------|
| Name   | `DisplayRole`          | `displayName()`                                                          |
| Name   | `ToolTipRole`          | `toolTip()`                                                              |
| Name   | `FontRole`             | Not set by core (the view renders bold based on `IsModifiedRole`, from 1.0) |
| Value  | `DisplayRole`          | `TypeHandler::displayText` (Bool: empty, uses `CheckStateRole`)          |
| Value  | `EditRole`             | `value()`                                                                |
| Value  | `CheckStateRole`       | Bool only: `Qt::Checked`/`Qt::Unchecked`                                 |
| Both   | custom roles above     | as in the enum                                                           |

**`flags()`:** `ItemIsEnabled` per effective `isEnabled()`; `ItemIsSelectable` always;
Value column: `ItemIsEditable` (non-Bool) or `ItemIsUserCheckable` (Bool) when not read-only and the type is registered.
Groups are never editable.

**Hidden properties (`visible = false`):** the model **still contains** the row; views hide it (`setRowHidden`), keeping
`QSortFilterProxyModel` and indexes stable. Visibility changes are announced via `dataChanged` with `IsVisibleRole`.

**Value write pipeline** (`setData(ValueColumn, EditRole|CheckStateRole)`, `setValue`, `Property::setValue`):

1. Group or unregistered type → return `false`, no signal. For `setData` (user edits) also: read-only or disabled property → `false`. [D34]
2. Convert the `QVariant` to `storageType` (`QVariant::convert`); on failure → `validationFailed`, return `false`.
3. `normalize` (e.g. Int/Double clamped to `min`/`max`, rounded to `decimals`).
4. `TypeHandler::validate` → `Property::validator`. Error → `validationFailed(path, v, message)`, return `false`, old value kept.
5. Value equals the old value → return `true`, **no** signal.
6. Store the value; `dataChanged(nameIdx, valueIdx)`; `valueChanged(path, new, old)`.
   Inside a batch: `valueChanged` is still emitted per change; paths are collected and `batchValueChanged` is emitted when the batch ends.

**Structural changes while the model is live:** each node holds an internal pointer to a `detail::TreeObserver`
(an interface in core, implemented by `PropertyModel`). `PropertyGroup::add*` / `remove` call
`observer->aboutToInsert/inserted/aboutToRemove/removed`, which the model turns into `beginInsertRows`/`endInsertRows`/...
Metadata setters call `observer->changed(node, roles)` → `dataChanged`. Detached nodes have a null observer and pay nothing.
Removing a property that is being edited: the view closes the editor first (Qt handles this via `rowsAboutToBeRemoved`).

**Ownership:** `PropertyModel` owns the root. Returned `Property*` pointers stay valid until the node is removed or `setRoot` is called.

### 4.7 Validation

```cpp
struct ValidationResult {
    bool ok = true;
    QString message;
    static ValidationResult valid();
    static ValidationResult error(QString msg);
};
```

Default validation of the basic types:

| Type       | Check                                                                     |
|------------|---------------------------------------------------------------------------|
| Int/Double | Always in range after normalize → always valid (clamp instead of reject)  |
| String     | `maxLength`; `regularExpression` (full match, `QRegularExpression::anchoredPattern`) |
| Enum       | Value is one of `options`                                                 |
| FilePath   | `mustExist` + `FileMode::Open` → `QFileInfo::isFile()`; empty string is always valid |
| DirPath    | `mustExist` → `QFileInfo::isDir()`; empty string is always valid          |

---

## 5. Widgets (`qpb::widgets`)

### 5.1 `EditorFactory`

```cpp
struct EditorHandler {
    std::function<QWidget*(QWidget* parent, const Property&)> createEditor;        // required
    std::function<void(QWidget*, const QVariant&, const Property&)> setEditorData; // required
    std::function<QVariant(QWidget*, const Property&)> editorData;                 // required
    std::function<void(QPainter*, const QStyleOptionViewItem&, const QVariant&, const Property&)> paint; // optional
    std::function<void(QWidget*, const Property&)> applyAttributes;                // optional
};

class EditorFactory {
public:
    static EditorFactory& global();
    bool registerEditor(const TypeId& id, const EditorHandler& h);
    bool replaceEditor (const TypeId& id, const EditorHandler& h);
    bool contains(const TypeId& id) const;
    const EditorHandler* handler(const TypeId& id) const;
    const EditorHandler* handlerFor(const Property& p) const;  // editorId attribute, then typeId
    QList<TypeId> editors() const;
    QWidget* createEditor(QWidget* parent, const Property& p) const;
    static void notifyCommit(QWidget* editor);                // commit now, don't wait for focus-out
};

class EditorDialogScope {                                     // RAII: editor shows a modal dialog
public:
    explicit EditorDialogScope(QWidget* editor);
    ~EditorDialogScope();
};
```

- Handler lookup order: `attributes["editorId"]` → `typeId()`. Not found → no editor (display-only cell).
- A custom editor that wants to commit immediately (without waiting for focus-out) calls `EditorFactory::notifyCommit(this)`; the delegate and form view listen for it.

### 5.2 Overriding the editor of a single property

Register an editor under its own ID (e.g. `"myapp.slider"`) and set the attribute `editorId = "myapp.slider"` on
that property. Storage type and validation still follow the original `typeId`.

### 5.3 Default editors

| Type     | Widget                                                        | Behaviour                                                               |
|----------|---------------------------------------------------------------|-------------------------------------------------------------------------|
| Bool     | *no editor* in Tree/List (checkbox painted by the delegate via `CheckStateRole`); `QCheckBox` in Form | Toggle with click or Space |
| Int      | `QSpinBox`                                                    | Applies `min/max/step/prefix/suffix`; `keyboardTracking = false`        |
| Double   | `QDoubleSpinBox`                                              | Like Int + `decimals`; when not editing, shows at most `decimals` fractional digits via `QLocale`, trailing zeros removed |
| String   | `QLineEdit`                                                   | `maxLength`, `placeholder`, `QRegularExpressionValidator` from `regularExpression` |
| Enum     | `QComboBox` (not editable)                                    | Commits on selection (`notifyCommit` on `activated`)                    |
| FilePath | internal `PathEdit` = `QLineEdit` + `QToolButton "…"`         | Button opens `QFileDialog::getOpenFileName`/`getSaveFileName` per `dialogMode`; on selection → `notifyCommit` |
| DirPath  | internal `PathEdit` (directory mode)                          | `QFileDialog::getExistingDirectory`                                     |

Long paths in a (non-editing) cell are elided in the middle (`Qt::ElideMiddle`); the tooltip shows the full path.

### 5.4 `PropertyDelegate : QStyledItemDelegate`

- `createEditor/setEditorData/setModelData` delegate to `EditorFactory`. `setModelData` calls `model->setData`;
  if it returns `false` (validation error) the editor still closes, the model keeps the old value, and the view shows the error
  (tooltip at the cell, `QToolTip::showText`) — 1.0 behaviour. [Decision D5]
- `paint`: uses `EditorHandler::paint` when present; check boxes come from `CheckStateRole`; groups paint a `QPalette::Button` background
  with bold text; the name of a modified property is bold; read-only values (enabled, neither editable nor checkable) use
  `QPalette::PlaceholderText`; value text is elided in the middle. [D30]
- **Focus while a dialog is open:** an editor showing a modal dialog holds a `qpb::EditorDialogScope` (public, so custom editors such as
  a color button can use it; the internal `PathEdit` does too). `PropertyDelegate::eventFilter` ignores `FocusOut` while a scope is
  active, so the editor is not committed/closed half-way. [Decisions D6, D24]
- Keys: **Enter** commits + closes; **Esc** cancels; **Tab/Shift+Tab** commit and open the editor on the next/previous editable Value cell
  (skipping groups and read-only properties); focus-out commits.

### 5.5 `PropertyTreeView : QTreeView`

```cpp
class PropertyTreeView : public QTreeView {
public:
    enum class Mode { Tree, List };
    explicit PropertyTreeView(QWidget* parent = nullptr);
    void setModel(QAbstractItemModel* model) override; // accepts a PropertyModel or a proxy of one
    void setMode(Mode m);  Mode mode() const;
    int nameColumnWidth() const;
    void setNameColumnWidth(int px);
    PropertyDelegate* propertyDelegate() const;
};
```

- The name column fits its contents (capped at 60% of the view) until its width is set with `setNameColumnWidth()` or by
  dragging the header. [D35]
- Defaults: `editTriggers = CurrentChanged | SelectedClicked | EditKeyPressed`; 2 columns; resizable header;
  `alternatingRowColors = true`; `setUniformRowHeights(true)`; rows hidden per `IsVisibleRole`, updated on `dataChanged`/insert.
- **Mode::Tree:** groups are collapsible, expanded by default; group rows use `setFirstColumnSpanned(true)`.
- **Mode::List:** *no flattening proxy*. Same model, `rootIsDecorated = false`, `indentation = 0`,
  `itemsExpandable = false`, `expandAll()` kept on new rows; groups render as section headers (spanned, not collapsible). [Decision D4]
- Context menu on a property: **Reset to default** (disabled when not modified or read-only); on a group: **Reset group**.
- Switching mode does not recreate the model and loses neither values nor the current selection.
- `moveCursor()` is overridden so `MoveNext`/`MovePrevious` (Tab / Shift+Tab while editing) land on the next editable value,
  skipping groups, read-only rows, check boxes and hidden rows.

### 5.6 `PropertyFormView : QScrollArea` (1.1)

- Builds a `QFormLayout` per group; nested groups become collapsible `QGroupBox`es (not checkable, ▸ button in the title).
- Every property has a **persistent** editor created by `EditorFactory` (Bool uses `QCheckBox`).
- Commit: editor change signal → `model->setData` (spin box: `editingFinished`; line edit: `editingFinished`;
  combo/checkbox/path: immediately or via `notifyCommit`). Validation error → restore the old value in the editor + error tooltip.
- Reverse sync: `dataChanged` → `setEditorData` (loops blocked with `QSignalBlocker`);
  `rowsInserted/rowsRemoved/modelReset/layoutChanged` → rebuild the affected part (1.1 may rebuild the whole containing group).
- Honours `visible` (hides label + editor), `enabled`, `readOnly`.
- Does not use `QDataWidgetMapper` (no tree support).

### 5.7 Search / filter (1.1)

`PropertyFilterProxyModel : QSortFilterProxyModel` with `recursiveFilteringEnabled = true`, filtering on `displayName`
(case-insensitive). A group is shown when a child matches. `PropertyTreeView` and `PropertyFormView` work with the proxy.

---

## 6. Distribution as a component folder

### 6.1 Repository layout

The `qpb/` folder is the **unit of distribution**: self-contained, usable once copied into another project.
Everything outside `qpb/` exists only to develop the library.

```text
property-browser/                  (development repo)
├── qpb/                           ← COMPONENT FOLDER: copy this folder as-is
│   ├── CMakeLists.txt             # project(qpb VERSION x.y.z), builds the two libraries only
│   ├── VERSION                    # "1.0.0" — single source of the version
│   ├── LICENSE
│   ├── CHANGELOG.md               # includes upgrade notes for every release
│   ├── cmake/                     # internal CMake helpers and templates
│   ├── include/qpb/               # public headers (only these) — §9
│   │   ├── qpb.h                  # umbrella header: everything (needs qpb::widgets)
│   │   ├── qpbcore.h              # umbrella header for qpb::core only
│   │   ├── qpbglobal.h            # export macros, QPB_VERSION*, QPB_DEPRECATED*
│   │   ├── Property.h  PropertyGroup.h  PropertyModel.h  TypeRegistry.h ...
│   │   └── widgets/PropertyTreeView.h  EditorFactory.h ...
│   └── src/                       # internal: *.cpp, *_p.h
│       ├── core/
│       └── widgets/
├── CMakeLists.txt                 # dev: add_subdirectory(qpb) + tests + examples
├── tests/   (core, widgets, api_compat, consumer)
├── examples/ (quickstart, custom_type, inspector)
├── tools/
└── docs/
```

`tests/` and `examples/` use `qpb` **exactly like a consumer** (link `qpb::core` / `qpb::widgets` only),
so integration problems show up inside the repo.

### 6.2 Using it from another project

```text
my-app/
├── CMakeLists.txt
├── components/
│   └── qpb/        ← copied from property-browser/qpb (or git subtree/submodule)
└── src/
```

```cmake
# my-app/CMakeLists.txt — two lines are enough
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)   # pulls in qpb::core
```

```cpp
#include <qpb/qpb.h>   // or include individual headers
```

**Updating the library:** delete `components/qpb/`, copy the new version in, rebuild. Read the new release's section in `CHANGELOG.md`.
No change to the consumer's CMake or code within the same major version (G5, S6).

Three supported ways to keep the folder in sync (the consumer chooses):

| Method           | Update command                                                   | Notes                                    |
|------------------|------------------------------------------------------------------|------------------------------------------|
| Manual copy      | Download `qpb-x.y.z.zip` from the GitHub Release, extract over it | Simplest                                 |
| `git subtree`    | `git subtree pull --prefix components/qpb <remote> qpb-release --squash` | Needs a `qpb-release` branch containing only `qpb/` (created with `git subtree split`) |
| `git submodule`  | Point to the repo and use `add_subdirectory(components/property-browser/qpb)` | Pulls the whole dev repo; tests/examples are not built as a subproject |

### 6.3 Requirements for `qpb/CMakeLists.txt` (do not pollute the host project)

- Use `CMAKE_CURRENT_SOURCE_DIR`/`CMAKE_CURRENT_BINARY_DIR`, **never** `CMAKE_SOURCE_DIR`.
- Do not change global variables (`CMAKE_CXX_STANDARD`, `CMAKE_CXX_FLAGS`, `CMAKE_AUTOMOC`, output dirs, ...).
  Everything goes through `target_*` and `set_target_properties(... AUTOMOC ON)` on qpb's own targets.
- `target_compile_features(qpb_core PUBLIC cxx_std_17)` — a *minimum* only; consumers on C++20 still work.
- Warning flags and `QT_NO_CAST_FROM_ASCII` are **PRIVATE**; public headers must be warning-free under the consumer's
  `-Wall -Wextra -Wpedantic` / `/W4`.
- Call `find_package(Qt6 6.5 ... Core Widgets)` only if the `Qt6::Widgets` target does not exist yet; otherwise verify the Qt version.
- Target names use the `qpb_` prefix; aliases `qpb::core`, `qpb::widgets`. Options use the `QPB_` prefix.
- No tests/examples inside `qpb/`; no `install()` by default (option `QPB_INSTALL`, OFF).
- **No Qt resources (`.qrc`)** in 1.x: a static library would need `Q_INIT_RESOURCE` on the consumer side → breaks "two lines of CMake".
  Icons come from `QStyle::standardIcon` or are painted in code.
- **No reliance on static initializers** to register types (the linker may drop them when linking statically).
  Basic types are registered lazily in `TypeRegistry::global()` / `EditorFactory::global()`.
- Shared builds (`QPB_BUILD_SHARED=ON`) are supported; consumers must then deploy the DLL/.so — documented in the README.
- `VERSION` holds `MAJOR.MINOR.PATCH` with an optional pre-release suffix (`1.0.0-rc1`); `QPB_VERSION_STR` carries the full
  string. `project()` is called **without** `VERSION`: in a subdirectory it would set the host's `CMAKE_PROJECT_VERSION`
  when the host project has none. Editing `VERSION` re-runs CMake (`CMAKE_CONFIGURE_DEPENDS`). [D31, D32]

## 7. Testing

| Layer         | Tool                           | Required content                                                                  |
|---------------|--------------------------------|-----------------------------------------------------------------------------------|
| Core          | Qt Test (no GUI)               | builder, path/find, inherited effective state, registry, validation pipeline      |
| Model         | Qt Test + `QAbstractItemModelTester` (Fatal mode) | every structural/value operation; signals (`QSignalSpy`); batches |
| Widgets       | Qt Test, `QT_QPA_PLATFORM=offscreen` | create/commit/cancel editors for all 7 types; Enter/Esc/Tab; focus-out; Tree↔List; reset menu; PathEdit does not close while a dialog is open (dialog replaced by a test hook) |
| Examples      | built in CI                    | S1 and S2 measured automatically by a line-count script                            |
| API compat    | compile + run only             | §9.5: client code of every released 1.x still builds and behaves correctly        |
| Consumer      | sample CMake project `tests/consumer` | copy `qpb/` into `components/`, build; no leaked global variables; C++17 and C++20; static and shared |

The internal `PathEdit` has a test hook (under `src/`, not public) so tests can replace `QFileDialog` with a function returning a fixed value.

CI: GitHub Actions, matrix Ubuntu/Windows/macOS × Qt 6.5 / 6.8, via `jurplel/install-qt-action`.

---

## 8. Versions and scope

Because the goal is a stable API across many projects, **the first release for real projects is 1.0**.
0.x releases are only used inside this repo (examples) to try out the API; the 0.x API may change.
After 1.0, new features arrive as **additions** (minor releases); existing API is never altered.

| Version | Content                                                                                                  | Change type   |
|---------|----------------------------------------------------------------------------------------------------------|---------------|
| 0.1     | Internal prototype: core + model + tree view, 7 types. API not frozen                                    | —             |
| **1.0** | All of §4; §5.1–5.5; 7 basic types; Tree + List; reset; bold when modified; tooltips; component folder (§6); API policy (§9); examples quickstart, custom_type, inspector | **API freeze** |
| 1.1     | `PropertyFormView` (§5.6); `PropertyFilterProxyModel` (§5.7); `multiline` attribute                      | Additive      |
| 1.2     | `QObjectPropertySource` (reads `Q_PROPERTY`, metadata via `Q_CLASSINFO("qpb:<prop>", "min=0;max=10")`, two-way sync); serialization `toJson/fromJson`, `save/load(QSettings&)`; `Types::Int64`; `QUndoStack` example | Additive |
| 2.0     | Only if breaking the API is truly necessary; removes everything deprecated                               | Breaking      |

**The 1.0 design must leave room for 1.1/1.2** without API changes: the form view and filter are new classes on top of the
existing `PropertyModel`; `QObjectPropertySource` needs only the public API of `PropertyGroup`/`PropertyModel`
(`add`, `setValue`, `valueChanged`); `Int64` is a new `TypeId`; serialization is a set of new free functions.
Checking this is a dedicated task in the plan (M1.3).

---

## 9. API stability policy

### 9.1 Commitment

Within a major version (1.x): consumer code that builds against 1.a **builds and behaves the same** against 1.b (b > a)
by only replacing the `qpb/` folder. The single exception: fixing a bug where the old behaviour contradicted the documentation —
this must be called out in the CHANGELOG.

Because consumers always **rebuild from source**, the commitment is **source (API) compatibility**, not binary (ABI) compatibility.
D-pointers are still used (§9.3) to keep headers stable and internals free to change.

### 9.2 Allowed / forbidden changes within 1.x

| Allowed (minor/patch)                                                    | Forbidden (major only)                                            |
|--------------------------------------------------------------------------|-------------------------------------------------------------------|
| Add classes, free functions, new headers                                 | Remove or rename anything public                                  |
| Add new **non-virtual** member functions; add non-ambiguous overloads    | Change parameter/return types, add parameters (even defaulted)    |
| Append enum values **at the end**                                        | Change the numeric value of an existing enum/role                 |
| Add `std::function` fields to `TypeHandler`/`EditorHandler` (empty = old behaviour) | Add a **pure virtual** function to a class users may subclass |
| Add virtual functions **whose default implementation keeps the old behaviour** | Change documented default behaviour                         |
| Add attributes, `TypeId`s, new signals                                   | Change the signature of an existing signal/slot                   |
| Add CMake options (defaults keep the old behaviour)                      | Rename targets, options, include paths; raise the C++/Qt/CMake minimum |
| Deprecate (with a replacement)                                           | Remove deprecated items                                           |

### 9.3 Public header design rules

- Stateful classes (`Property`, `PropertyGroup`, `PropertyModel`, `TypeRegistry`, `EditorFactory`, views) use a d-pointer;
  no public/protected data members besides the d-pointer. Headers never include `*_p.h`.
- Configuration structs (`TypeHandler`, `EditorHandler`, `EnumOption`, `ValidationResult`) are aggregates that only **grow at the end**;
  documentation recommends assigning fields one by one, never positional initialization (`{a, b, c}`).
- No third-party types in the API; only Qt and std types.
- No business logic in `inline` functions/templates in headers (builders only forward to functions in `.cpp` files).
- Every public header is **self-contained** (includable on its own), verified by a per-header compile test.

### 9.4 Versioning and deprecation

- `qpbglobal.h` provides `QPB_VERSION_MAJOR/MINOR/PATCH`, `QPB_VERSION_STR`, `QPB_VERSION`, `QPB_VERSION_CHECK(maj, min, pat)`
  (generated from the `VERSION` file) and the runtime function `qpb::version()`.
- Superseded API is marked `QPB_DEPRECATED_X("use X instead")` (maps to `[[deprecated]]`) and kept **until the end of 1.x**.
  Deprecated declarations are wrapped in `#if !defined(QPB_DISABLE_DEPRECATED)`, so a consumer who wants to clean up early
  can define `QPB_DISABLE_DEPRECATED` and get compile errors wherever deprecated API is still used.
- Every `CHANGELOG.md` entry has *Added / Changed / Deprecated / Fixed* and *Upgrade notes* (usually "nothing to do").

### 9.5 Automated verification

- `tests/api_compat/v1_0.cpp`, `v1_1.cpp`, …: each release adds one file exercising **all** public API of that release
  (signals, builders and handlers included). Old files are **never edited**; every later release must build them
  (deprecation warnings allowed) and pass.
- "Self-contained header" test: every public header is included alone in its own `.cpp`.
- `tools/api_snapshot`: exports the list of public symbols (by parsing headers, or with `abi-dumper` when available) and diffs it
  against the previous release's snapshot → CI flags removals/changes. (Used from 1.1; 1.0 only creates the baseline.)

---

## Appendix A — Final API example (1.0)

```cpp
#include <qpb/qpb.h>

using namespace qpb;

auto root = PropertyGroup::create("Camera");
auto& transform = root->addGroup("Transform");
transform.addDouble("x", 0.0).range(-100.0, 100.0).step(0.5).suffix(" m");
transform.addDouble("y", 0.0).range(-100.0, 100.0).step(0.5).suffix(" m");
root->addBool("visible", true);
root->addInt("fov", 60).range(10, 170);
root->addString("name", "Main camera");
root->addEnum("projection", {"Perspective", "Orthographic"}, 0);
root->addFilePath("lut", {}).filter("LUT files (*.cube)");
root->addDirPath("cacheDir", {});

PropertyModel model(std::move(root));
PropertyTreeView view;                       // view.setMode(PropertyTreeView::Mode::List);
view.setModel(&model);

QObject::connect(&model, &PropertyModel::valueChanged,
                 [](const QString& path, const QVariant& v, const QVariant&) { qDebug() << path << "=" << v; });
```

Registering `QColor` (C++17, outside the library):

```cpp
TypeHandler t;
t.displayText = [](const QVariant& v, const Property&) { return v.value<QColor>().name(); };
TypeRegistry::global().registerType<QColor>("app.color", t);

EditorHandler e;
e.createEditor  = [](QWidget* parent, const Property&) { return new ColorButton(parent); };
e.setEditorData = [](QWidget* w, const QVariant& v, const Property&) { static_cast<ColorButton*>(w)->setColor(v.value<QColor>()); };
e.editorData    = [](QWidget* w, const Property&) { return QVariant::fromValue(static_cast<ColorButton*>(w)->color()); };
e.paint         = &paintColorSwatch;
EditorFactory::global().registerEditor("app.color", e);

root->add("app.color", "tint", QColor(Qt::white));
```

## Appendix B — Decision log

| #  | Decision                                                                   | Rationale                                                                              |
|----|----------------------------------------------------------------------------|----------------------------------------------------------------------------------------|
| D1 | `TypeId` is a logical string, not a `QMetaType`                            | String/FilePath/DirPath all store `QString`; Enum may be int or string                 |
| D2 | Split `TypeRegistry` (core) from `EditorFactory` (widgets)                 | Keeps core free of QtWidgets (room for QML, headless tests)                            |
| D3 | C++17, handlers configured by field assignment                             | Designated initializers are C++20; MSVC `/std:c++17` rejects them                      |
| D4 | List mode = same `QTreeView`, no flattening proxy                          | Much cheaper; indexes and editors survive a mode switch                                |
| D5 | Validation error from the delegate: close editor, keep old value, show error tooltip | The standard delegate closes the editor before `setData` returns; simple for 1.0 |
| D6 | `PathEdit` suppresses `FocusOut` while its dialog is open                  | Prevents commit/close when the modal `QFileDialog` takes focus                         |
| D7 | The model owns the tree; nodes report structural changes via an internal `TreeObserver` | Allows adding/removing properties on a live model while keeping the builder API |
| D8 | Int is `int` only in 1.0; `qint64` is a new `Int64` type in 1.2             | `QSpinBox` only supports `int`; adding a new type does not break the API               |
| D9 | Int/Double clamp instead of rejecting                                      | Matches `QSpinBox`; values set from application code behave the same                  |
| D10| Hidden properties stay in the model; views hide the rows                   | Stable indexes; filter proxies work normally                                           |
| D11| Signals carry the `path` (QString), not `Property&`                        | Safe with queued connections, no metatype registration needed                          |
| D12| The first release for real projects is 1.0 (API freeze); 0.x is internal  | Goal G5: consuming projects never have to change code                                  |
| D13| Commitment is **source** compatibility, not ABI                            | Consumers always rebuild from the component folder                                     |
| D14| Distribution as a self-contained `qpb/` folder + `add_subdirectory`         | Goal G6; `find_package`/install become optional                                       |
| D15| Static library by default, no `.qrc`, no static initializers                | Avoids DLL deployment, `Q_INIT_RESOURCE`, and the linker dropping objects in static builds |
| D16| Written from scratch, no wrap/fork                                          | Goal G7; full control of the API makes the stability commitment possible              |
| D17| C++17, Qt ≥ 6.5, namespace `qpb`, CMake only — confirmed                   | Settled before M1 because they are frozen until 2.0                                    |
| D18| Code and docs in English; `-vi` files are reference translations only      | Project convention                                                                     |
| D19| "Custom property table" = users build property tables with the public API (G1, G3) | Clarified by the maintainer; no separate feature                                  |
| D20| API design and the RC trial use reference scenarios instead of real projects | No real consuming project is available yet                                          |
| D21| The public headers are the normative API reference; SPEC code summarizes them | Avoids two sources of truth after M1                                              |
| D22| Type IDs and attribute keys are `constexpr QLatin1StringView` constants     | No static-initialization order issues; convert implicitly to `QString`                 |
| D23| Own state as `Property::Flags` (ReadOnly/Disabled/Hidden) + effective getters | One extensible enum instead of separate own/effective getter pairs                 |
| D24| `EditorDialogScope` is public; `PathEdit` stays internal                    | Custom editors that open dialogs (e.g. colors) need the same focus protection as paths |
| D25| Concrete builder classes over a CRTP `PropertyBuilderBase`                  | Type-specific setters only where they apply; exported non-template classes          |
| D26| Attribute keys use full words (`minimum`, `regularExpression`, `dialogMode`) | Readability; the enum `qpb::FileMode` stores the dialog mode                        |
| D27| `qpb/qpbcore.h` umbrella for core-only users; `qpb/qpb.h` includes widgets  | Core-only consumers must not need QtWidgets                                          |
| D28| Validators receive the property (`(value, property)`) like `TypeHandler::validate` | One signature; validators can read attributes                                  |
| D29| Duplicate or invalid ids log a warning instead of asserting                 | An assert would abort debug builds of consuming apps; the tree stays consistent either way |
| D30| Group rows use `QPalette::Button`; read-only values are dimmed             | `AlternateBase` was indistinguishable from alternating rows; read-only needs a visual cue |
| D31| `project()` without `VERSION` in the component; version from `qpb/VERSION`  | `project(VERSION)` leaked into the host's `CMAKE_PROJECT_VERSION` (found by the consumer leak check) |
| D32| `VERSION` may carry a pre-release suffix; CMake re-runs when it changes      | Release candidates (`1.0.0-rc1`); replacing the folder must refresh the version header |
| D33| `qpb/` is ASCII only (rule R6)                                               | MSVC C4819 on non-UTF-8 code pages would break consumers using `/WX`                   |
| D34| Read-only/disabled block user edits (`setData`) only, not application writes | RC trial: an application could not update its own read-only status field, and group resets depended on property order |
| D35| The name column fits its contents until a width is set explicitly            | RC trial screenshots: names were cut off at the default width                         |
