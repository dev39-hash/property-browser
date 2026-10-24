# Architecture

qpb has two layers, built as two libraries. The lower layer knows nothing about widgets; the upper layer is a set of
views and editors that only talk to the lower one through the model and two registries.

```text
+------------------------------ qpb::widgets (Qt Widgets) ------------------------------+
|  PropertyTreeView ---+                                                                |
|   (mode Tree / List) +-- PropertyDelegate --+                                          |
|                      |                      +-- EditorFactory (type ID -> EditorHandler)
|  PropertyFormView ---+----------------------+                                         |
+------------------------------- qpb::core (Qt Core only) ------------------------------+
|  PropertyModel : QAbstractItemModel                                                   |
|  Property / PropertyGroup (the data tree)     TypeRegistry (type ID -> TypeHandler)   |
|  Attributes (standard keys)   ValidationResult   PropertyFilterProxyModel             |
|  QObjectPropertySource   serialization (JSON, QSettings)   loadTranslation()          |
+---------------------------------------------------------------------------------------+
```

- `qpb::core` links Qt Core only. It has everything needed to describe, validate, store, serialize and observe
  properties, and it can be used (and tested) without a GUI.
- `qpb::widgets` links `qpb::core` and Qt Widgets. It has the views, the delegate and the editors.

## Main classes

Core (headers in `qpb/include/qpb/`):

| Class | Responsibility |
|---|---|
| `Property` | One node of the tree: id, path, display name, type ID, value, default value, attributes, flags (read-only, enabled, visible, live), conditions, validator. Not a `QObject`. Its data lives behind a d-pointer. |
| `PropertyGroup` | A `Property` that holds children, and the builder API (`addInt()`, `addString()`, ...) that creates them. |
| Builders (`PropertyBuilders.h`) | Thin wrappers that set type-specific attributes (`range()`, `suffix()`, ...) and forward to the property; a setter that does not apply to the type does not compile. |
| `PropertyModel` | A `QAbstractItemModel` over the tree. Owns the root, maps properties to roles, runs the value write pipeline, evaluates conditions, emits `valueChanged`, `validationFailed` and `batchValueChanged`. |
| `TypeRegistry` | Maps a type ID to a `TypeHandler`: storage type, display text, normalization, validation, JSON conversion. All type-dependent behaviour that needs no widget lives here. |
| `PropertyFilterProxyModel` | A `QSortFilterProxyModel` that filters by display name for search boxes. |
| `QObjectPropertySource` | Builds properties from the `Q_PROPERTY`s of QObjects and keeps both sides in sync through the model. |
| `qpb::serialization` | Free functions that write and read values as JSON or `QSettings`. |
| `loadTranslation()` | Loads qpb's own translations, which are embedded in `qpb::core`. |

Widgets (headers in `qpb/include/qpb/widgets/`):

| Class | Responsibility |
|---|---|
| `EditorFactory` | Maps a type ID (or a property's `editorId` attribute) to an `EditorHandler`: how to create an editor widget, put the value in, read it back, and optionally paint the cell. |
| `PropertyDelegate` | A `QStyledItemDelegate` that creates editors through `EditorFactory` and writes the result with `setData`; paints groups, modified and read-only values. |
| `PropertyTreeView` | A `QTreeView` in Tree or List mode, with the editing keys, the reset menu and style sheet hooks. |
| `PropertyFormView` | A `QScrollArea` that builds a form (label and persistent editor per property, collapsible sections for groups) from the same model. |
| `EditorDialogScope` | Marks an editor that shows a modal dialog, so the delegate does not commit it when focus moves to the dialog. |

Internal helpers live in `qpb::detail` and under `qpb/src/` (for example the path editor, the 64-bit spin box and the
`<Name>Private` d-pointer classes). They are not API and change freely.

## Data flow

```text
  Showing and editing (the user)

  builder API --> tree of Property nodes (owned by PropertyModel)
                                |
                                | data(), flags(), dataChanged, row signals
                                v
                  views: PropertyTreeView / PropertyFormView
                                |
                                | type ID (or the editorId attribute)
                                v
                  EditorFactory --> editor widget
                                |
                                | setData(index, value)         <-- R3
                                v
                  PropertyModel --> write pipeline of the Property
                                      1. convert to the storage type   (TypeRegistry)
                                      2. normalize                     (TypeRegistry)
                                      3. validate                      (TypeRegistry, then the property's validator)
                                      4. store, then notify
                                |
                                v
                  signals carrying the path: valueChanged / validationFailed / batchValueChanged

  Writing by the application

  Property::setValue() or PropertyModel::setValue() --> the same write pipeline and signals
```

1. The application builds a tree with the builder API (or `QObjectPropertySource`, or deserialization into an existing
   tree) and hands it to a `PropertyModel`. The model owns the tree from then on.
2. Views show the model. They look up how to display and edit a property through `TypeRegistry` and `EditorFactory`,
   by type ID; they never decide by type themselves (R1).
3. A user edit ends in `PropertyModel::setData` (R3). The write pipeline, implemented once for all writers, converts the
   value to the storage type, normalizes it (for example clamps to the range), runs the type's validation and then
   the property's own validator. A rejected value leaves the old one in place and emits `validationFailed`; an
   unchanged value emits nothing; an accepted one is stored and the model emits `dataChanged` and `valueChanged`.
4. Application code writes with `Property::setValue()` or `PropertyModel::setValue()`. These use the same
   pipeline, so validation and signals are identical; the only difference is that read-only and disabled
   properties restrict the user, not the application.
5. Signals carry the property's path (a `QString`), not a `Property` reference. That keeps them safe with queued
   connections and needs no metatype registration. Within a batch (`beginBatch()` / `endBatch()`) every change
   still emits `valueChanged`, and one `batchValueChanged` follows when the outermost batch ends.

Properties that are added or removed while a model shows them tell the model through an internal observer
interface (`TreeObserver`, declared in a private header and implemented by `PropertyModel`). The model turns these
reports into the matching `beginInsertRows()` / `endInsertRows()` calls, `dataChanged` for metadata changes, and
conditions re-evaluation. A property that is not attached to a model has no observer and costs nothing.

## Ownership

- `PropertyModel` owns the root `PropertyGroup`; each group owns its children (`std::unique_ptr`).
- A `Property*` returned by the model or a group is an observer. It stays valid until that node is removed or the model's
  root is replaced; do not keep one across a structural change. Paths (`"Transform/x"`) identify properties across
  such changes.
- Views, proxies and `QObjectPropertySource` do not own the model. The registries (`TypeRegistry::global()`,
  `EditorFactory::global()`) are process-wide and register their basic types lazily on first use; they are not
  thread-safe, so register on the main thread before creating models.
- qpb is a GUI-thread library: it creates no threads.

## Architecture constraints

These six constraints keep the layers separate and the public surface small. They are checked automatically where a
check is possible, and in code review for every change.

| ID | Constraint | Why | Checked by |
|---|---|---|---|
| R1 | Views never branch on a type ID or `QMetaType`. All type-dependent behaviour goes through `TypeRegistry` or `EditorFactory`. | A type registered by an application must work in every view without changing the library. | `architecture_rules` (searches the delegate and the two views for `Types::` and `typeId()`), review |
| R2 | `qpb::core` includes no QtWidgets or QtGui header. | Core stays usable without a GUI, and a non-widget front end remains possible. | `architecture_rules`, review |
| R3 | Every value change that comes from the UI goes through `PropertyModel::setData`, so through validation and the signal. Views never write to a `Property` directly. | One place decides what a valid value is, and every change is announced. | Review, the view tests |
| R4 | No public API without at least one user, an example or a test. | Untried API is where mistakes in a stable interface hide. | Review |
| R5 | Only headers under `qpb/include/qpb/` are public. `qpb::detail` and everything under `qpb/src/` is internal and may change freely; public headers never include `_p.h` files or anything under `src/`. | A public header that reaches into internals would freeze them. | `architecture_rules` (includes), review |
| R6 | Text files under `qpb/` are ASCII only (the translation catalogs `qpb_<lang>.ts` and the translated user documentation under `qpb/doc/<lang>/` are the exceptions). | MSVC on a non-UTF-8 code page warns (C4819) about other characters, which breaks consumers that build with `/WX`. | `architecture_rules` (headers, sources, templates, CMake files), review |

The check is the CTest test `architecture_rules`, which runs `tools/check_architecture.cmake`; see
[building-and-testing.md](../development/building-and-testing.md). The rules for public headers are in
[public-headers.md](public-headers.md), those for the component's CMake files in
[component-cmake.md](component-cmake.md).
