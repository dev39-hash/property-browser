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
| S1 | 10-property panel in ≤ 30 lines                                  | `examples/quickstart/main.cpp` (line count, excluding includes) |
| S2 | `QColor` type in ≤ 100 lines, outside the library                 | `examples/custom_type/`                                |
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

---

## 4. Core (`qpb::core`)

### 4.1 Type identifiers (`TypeId`)

Types are identified by a **logical ID** (string), not by `QMetaType`, because several logical types share one
storage type (`String`, `FilePath`, `DirPath` are all `QString`).

```cpp
namespace qpb {
using TypeId = QString;
namespace Types {
inline const TypeId Bool     = QStringLiteral("bool");
inline const TypeId Int      = QStringLiteral("int");
inline const TypeId Double   = QStringLiteral("double");
inline const TypeId String   = QStringLiteral("string");
inline const TypeId Enum     = QStringLiteral("enum");
inline const TypeId FilePath = QStringLiteral("filepath");
inline const TypeId DirPath  = QStringLiteral("dirpath");
inline const TypeId Group    = QStringLiteral("group");
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
| `validator`       | `std::function<ValidationResult(const QVariant&)>` | Optional, runs after type validation         |

The corresponding setters (`setDisplayName`, `setToolTip`, `setReadOnly`, `setEnabled`, `setVisible`,
`setAttribute`, `setValidator`, `setDefaultValue`) **notify the model** when the property is attached to one
(see 4.6), so views update.

`Property::setValue()` is the **application-code API** (e.g. loading data). It runs through the same
validation + signal pipeline as `PropertyModel::setData` (R3) and returns `bool`.

### 4.3 `PropertyGroup` and the builder API

`PropertyGroup : Property` with `typeId() == Types::Group`, holding an ordered list of children.

```cpp
class PropertyGroup : public Property {
public:
    static std::unique_ptr<PropertyGroup> create(const QString& id);

    PropertyGroup& addGroup(const QString& id);
    PropertyBuilder<bool>    addBool  (const QString& id, bool v);
    PropertyBuilder<int>     addInt   (const QString& id, int v);
    PropertyBuilder<double>  addDouble(const QString& id, double v);
    PropertyBuilder<QString> addString(const QString& id, const QString& v);
    EnumBuilder              addEnum  (const QString& id, const QStringList& labels, int index);
    EnumBuilder              addEnum  (const QString& id, const QList<EnumOption>& options, const QVariant& v);
    PathBuilder              addFilePath(const QString& id, const QString& v);
    PathBuilder              addDirPath (const QString& id, const QString& v);
    Property&                add(const TypeId& type, const QString& id, const QVariant& v); // custom type
    Property&                add(std::unique_ptr<Property> p);

    bool remove(const QString& id);
    int  childCount() const;
    Property* child(int i) const;
    Property* find(const QString& path) const;   // "Transform/x"
};
```

- Adding a duplicate `id` to the same group: **asserts in debug**; in release returns the existing property (nothing new is created).
- `PropertyBuilder<T>` is a thin wrapper around `Property&` whose setters return `*this`:
  `displayName`, `toolTip`, `readOnly`, `enabled`, `visible`, `validator`, plus type-specific setters
  (`range`, `step`, `decimals`, `prefix`, `suffix`, `maxLength`, `placeholder`, `regex`, `filter`, `mode`, `defaultDir`, `mustExist`).
  A setter that does not apply to the type (e.g. `regex` on `int`) **does not compile** (declared only on the matching specialization).
- A builder converts implicitly to `Property&`.

### 4.4 Standard attributes

Keys are the constants `qpb::Attr::*` (`QString`). Unknown attributes are ignored (no error), so custom types can have their own.

| Type       | Attributes (value type)                                                                  | Default               |
|------------|-------------------------------------------------------------------------------------------|-----------------------|
| Int        | `min`(int) `max`(int) `step`(int) `prefix` `suffix`                                        | INT_MIN / INT_MAX / 1 |
| Double     | `min` `max` `step`(double) `decimals`(int) `prefix` `suffix`                               | −∞ / +∞ / 1.0 / 2     |
| String     | `maxLength`(int) `placeholder` `regex`(QString) `multiline`(bool, **1.1**)                | unlimited             |
| Enum       | `options`(`QList<EnumOption>` — `{QString label; QVariant value;}`)                         | —                     |
| FilePath   | `filter`(QString) `mode`(`"open"`/`"save"`) `defaultDir` `mustExist`(bool)                  | `"open"`, false       |
| DirPath    | `defaultDir` `mustExist`(bool)                                                             | false                 |
| (any type) | `editorId`(TypeId) — overrides the editor for this property only (see 5.2)                  | —                     |

**Enum:** `value()` is the `value` of the selected option (int **or** QString, chosen by overload).
The overload `addEnum(id, QStringList labels, int index)` creates options with `value = index`.

**64-bit integers:** 1.0 supports only `int` (`QSpinBox` is `int`). `qint64` arrives in 1.2 as a **new type**
`Types::Int64` (the behaviour of `Int` does not change → not breaking).

### 4.5 `TypeRegistry` (UI-free part)

```cpp
struct TypeHandler {
    int storageType = QMetaType::UnknownType;                           // expected QVariant type
    std::function<QString(const QVariant&, const Property&)> displayText; // null → QVariant::toString()
    std::function<ValidationResult(const QVariant&, const Property&)> validate; // null → always valid
    std::function<QVariant(const QVariant&, const Property&)> normalize;  // null → unchanged (e.g. clamp)
};

class TypeRegistry {
public:
    static TypeRegistry& global();
    bool registerType(const TypeId& id, TypeHandler h);   // false if id already exists (no overwrite)
    void replaceType (const TypeId& id, TypeHandler h);   // deliberate overwrite
    const TypeHandler* handler(const TypeId& id) const;   // nullptr if not registered
    template <class T> bool registerType(const TypeId& id, TypeHandler h); // sets storageType
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
    void resetToDefault(const QModelIndex& idx);          // group ⇒ recursive reset

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

1. Read-only / disabled property or unregistered type → return `false`, no signal.
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
| String     | `maxLength`; `regex` (full match, `QRegularExpression::anchoredPattern`)  |
| Enum       | Value is one of `options`                                                 |
| FilePath   | `mustExist` + mode `open` → `QFileInfo::isFile()`; empty string is always valid |
| DirPath    | `mustExist` → `QFileInfo::isDir()`; empty string is always valid          |

---

## 5. Widgets (`qpb::widgets`)

### 5.1 `EditorFactory`

```cpp
struct EditorHandler {
    std::function<QWidget*(QWidget* parent, const Property&)> createEditor;   // required
    std::function<void(QWidget*, const QVariant&, const Property&)> setEditorData; // required
    std::function<QVariant(QWidget*, const Property&)> editorData;            // required
    std::function<void(QPainter*, const QStyleOptionViewItem&, const QVariant&, const Property&)> paint; // optional
    std::function<void(QWidget*, const Property&)> applyAttributes;          // optional, called after createEditor and when attributes change
};

class EditorFactory {
public:
    static EditorFactory& global();
    bool registerEditor(const TypeId& id, EditorHandler h);
    void replaceEditor (const TypeId& id, EditorHandler h);
    const EditorHandler* handler(const TypeId& id) const;

    QWidget* createEditor(QWidget* parent, const Property& p) const; // honours the editorId attribute
    // An editor calls this when the user "commits" a value (e.g. finished picking a file) so the view commits immediately.
    static void notifyCommit(QWidget* editor);
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
| String   | `QLineEdit`                                                   | `maxLength`, `placeholder`, `QRegularExpressionValidator` from `regex`  |
| Enum     | `QComboBox` (not editable)                                    | Commits on selection (`notifyCommit` on `activated`)                    |
| FilePath | `qpb::PathEdit` = `QLineEdit` + `QToolButton "…"`             | Button opens `QFileDialog::getOpenFileName`/`getSaveFileName` per `mode`; on selection → `notifyCommit` |
| DirPath  | `qpb::PathEdit` (directory mode)                              | `QFileDialog::getExistingDirectory`                                     |

Long paths in a (non-editing) cell are elided in the middle (`Qt::ElideMiddle`); the tooltip shows the full path.

### 5.4 `PropertyDelegate : QStyledItemDelegate`

- `createEditor/setEditorData/setModelData` delegate to `EditorFactory`. `setModelData` calls `model->setData`;
  if it returns `false` (validation error) the editor still closes, the model keeps the old value, and the view shows the error
  (tooltip at the cell, `QToolTip::showText`) — 1.0 behaviour. [Decision D5]
- `paint`: uses `EditorHandler::paint` when present; Bool paints a left-aligned checkbox; groups paint a `QPalette::AlternateBase` background with bold text.
- **Focus while a dialog is open:** `PathEdit` sets a `dialogOpen` flag while the modal dialog is shown. `PropertyDelegate::eventFilter`
  ignores `FocusOut` for editors with that flag, so the editor is not committed/closed half-way. [Decision D6]
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
    void setNameColumnWidth(int px);
};
```

- Defaults: `editTriggers = CurrentChanged | SelectedClicked | EditKeyPressed`; 2 columns; resizable header;
  `alternatingRowColors = true`; `setUniformRowHeights(true)`; rows hidden per `IsVisibleRole`, updated on `dataChanged`/insert.
- **Mode::Tree:** groups are collapsible, expanded by default; group rows use `setFirstColumnSpanned(true)`.
- **Mode::List:** *no flattening proxy*. Same model, `rootIsDecorated = false`, `indentation = 0`,
  `itemsExpandable = false`, `expandAll()` kept on new rows; groups render as section headers (spanned, not collapsible). [Decision D4]
- Context menu on a property: **Reset to default** (disabled when not modified or read-only); on a group: **Reset group**.
- Switching mode does not recreate the model and loses neither values nor the current selection.

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
│   │   ├── qpb.h                  # umbrella header: includes everything
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

## 7. Testing

| Layer         | Tool                           | Required content                                                                  |
|---------------|--------------------------------|-----------------------------------------------------------------------------------|
| Core          | Qt Test (no GUI)               | builder, path/find, inherited effective state, registry, validation pipeline      |
| Model         | Qt Test + `QAbstractItemModelTester` (Fatal mode) | every structural/value operation; signals (`QSignalSpy`); batches |
| Widgets       | Qt Test, `QT_QPA_PLATFORM=offscreen` | create/commit/cancel editors for all 7 types; Enter/Esc/Tab; focus-out; Tree↔List; reset menu; PathEdit does not close while a dialog is open (dialog replaced by a test hook) |
| Examples      | built in CI                    | S1 and S2 measured automatically by a line-count script                            |
| API compat    | compile + run only             | §9.5: client code of every released 1.x still builds and behaves correctly        |
| Consumer      | sample CMake project `tests/consumer` | copy `qpb/` into `components/`, build; no leaked global variables; C++17 and C++20; static and shared |

`PathEdit` exposes a static hook (`setDialogProviderForTesting`) so tests can replace `QFileDialog` with a function returning a fixed value.

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
