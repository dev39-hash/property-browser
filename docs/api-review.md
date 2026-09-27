# qpb — 1.0 API review (M1)

Review of the 1.0 public headers written in M1 (docs/PLAN.md M1.1–M1.5), against the stability rules of
docs/SPEC.md §9. The headers under `qpb/include/qpb/` are the normative API reference (D21).

Status: **M1 review done.** A final review against the implemented behaviour happens in M4.10 before the freeze.

## 1. Public headers

| Header                              | Library        | Contents                                                             |
|-------------------------------------|----------------|----------------------------------------------------------------------|
| `qpb/qpbglobal.h`                   | core           | export, version and deprecation macros; `qpb::version()`             |
| `qpb/Types.h`                       | core           | `TypeId`, built-in type IDs `Types::*`                               |
| `qpb/Attributes.h`                  | core           | attribute keys `Attr::*`, `EnumOption`, `FileMode`                   |
| `qpb/ValidationResult.h`            | core           | `ValidationResult`                                                   |
| `qpb/Property.h`                    | core           | `Property` (values, attributes, flags, validator)                    |
| `qpb/PropertyBuilders.h`            | core           | `PropertyBuilderBase<Derived>` and the seven builders                |
| `qpb/PropertyGroup.h`               | core           | `PropertyGroup` (children, typed `add*`)                             |
| `qpb/TypeRegistry.h`                | core           | `TypeHandler`, `TypeRegistry`                                        |
| `qpb/PropertyModel.h`               | core           | `PropertyModel` (roles, value API, batches, signals)                 |
| `qpb/qpbcore.h`                     | core           | umbrella for core-only users                                         |
| `qpb/widgets/EditorFactory.h`       | widgets        | `EditorHandler`, `EditorFactory`, `EditorDialogScope`                |
| `qpb/widgets/PropertyDelegate.h`    | widgets        | `PropertyDelegate`                                                   |
| `qpb/widgets/PropertyTreeView.h`    | widgets        | `PropertyTreeView` (Tree / List modes)                               |
| `qpb/qpb.h`                         | widgets        | umbrella for everything                                              |

Automated checks (built with the dev tree, see `tests/api/`):

- every header compiles on its own and twice in a row (include guard), with `-Wall -Wextra -Wpedantic -Werror`;
- `tests/consumer` includes `qpb/qpb.h` from a copied component folder with `-Werror`, as C++17 and C++20;
- the five examples compile against the headers (GCC 13 and Clang 18, Qt 6.5.3).

## 2. Checklist — SPEC §9.3 header rules

| Rule                                                        | Result |
|-------------------------------------------------------------|--------|
| Stateful classes use a d-pointer, no other data members      | ✅ `Property`, `PropertyGroup` (inherits), `TypeRegistry`, `PropertyModel`, `EditorFactory`, `PropertyDelegate`, `PropertyTreeView`. Exceptions by design: `PropertyBuilderBase` (a non-owning handle, one pointer) and `EditorDialogScope` (RAII guard, one pointer). Both are value helpers, never subclassed; source compatibility (D13) does not depend on their layout. |
| Configuration structs are aggregates that only grow at the end | ✅ `TypeHandler`, `EditorHandler`, `EnumOption`, `ValidationResult`; documented to assign fields by name |
| Only Qt and std types in the API                             | ✅ |
| No business logic inline in headers                          | ✅ Inline code is limited to forwarding (builders, `registerType<T>`), trivial factories (`ValidationResult`) and `EnumOption` comparison |
| Headers self-contained                                       | ✅ checked by `tests/api` |
| No `*_p.h` or `src/` includes from public headers            | ✅ private classes are only forward-declared in `qpb::detail` |

## 3. Checklist — SPEC §9.2 extension points

| Question                                                     | Answer |
|--------------------------------------------------------------|--------|
| Which classes may applications subclass?                     | `PropertyDelegate`, `PropertyTreeView` (Qt widgets). New virtuals must keep old behaviour by default. |
| Which classes must not be subclassed?                        | `Property`, `PropertyGroup` (protected constructors take an internal type), `TypeRegistry`, `EditorFactory` (private constructors). |
| Can enums grow?                                              | `PropertyModel::Role` (Qt::UserRole+1…+99 reserved), `Property::Flag`, `FileMode`, `PropertyTreeView::Mode`: append only. |
| Can handlers grow?                                           | Yes: new `std::function` fields, empty = old behaviour. |
| Are signal signatures final?                                 | `valueChanged(path, new, old)`, `validationFailed(path, rejected, message)`, `batchValueChanged(paths)`. New information → new signals. |

## 4. Future-proofing (M1.3)

Sketched in `tests/api/future_sketches.cpp` using only the 1.0 public API; the file is compiled in every build.

| Feature (release)                      | Needs from 1.0                                                                                   | Result |
|----------------------------------------|--------------------------------------------------------------------------------------------------|--------|
| `PropertyFormView` (1.1)               | Model walk via `QAbstractItemModel`, `PropertyRole`, `EditorFactory::createEditor/handlerFor`, `setData` | ✅ The editor→view commit notification (`notifyCommit`) is internal to the library, so a library-side form view can use it. |
| `PropertyFilterProxyModel` (1.1)       | `PropertyTreeView` accepting proxies; roles readable through a proxy                              | ✅ |
| `QObjectPropertySource` (1.2)          | `PropertyGroup::add(type, id, value)`, `TypeRegistry::types()` + `storageType`, `valueChanged`, `Property::setValue` | ✅ `QString` properties map to `Types::String` by default; path types need metadata (`Q_CLASSINFO`), which is additive. |
| Serialization JSON / `QSettings` (1.2) | `children()`, `toGroup()`, `child(id)`, `path()`, `value()`, `setValue()`                        | ✅ Custom types that need a JSON form get new optional `TypeHandler` fields (additive). |
| `Types::Int64` (1.2)                   | Registration like any custom type; `Attr::Minimum/Maximum`                                        | ✅ Adds `addInt64()` and an `Int64Builder` class (additive). |
| Multiline strings (1.1)                | New attribute key and editor behaviour                                                            | ✅ Additive. |

Conclusion: no planned 1.1/1.2 feature requires changing or removing 1.0 API.

## 5. Changes made during the review

- Added `EditorDialogScope`: custom editors that open dialogs need the focus protection the internal path editor uses (D24).
- Added `TypeRegistry::types()`/`contains()` and `EditorFactory::handlerFor()`/`editors()`: required by the QObject source and form view sketches.
- Added `qpb/qpbcore.h` so core-only consumers never include QtWidgets (D27).
- `Property::Flags` replaces separate own/effective getters (D23); validators take `(value, property)` (D28).
- Attribute keys renamed to full words; `Attr::DialogMode` + `qpb::FileMode` avoid a name clash (D26).
- Header includes checked against Qt 6.5 (a header that only exists in newer Qt was removed).

## 6. To revisit in M4.10 (before the 1.0 freeze)

- Behaviour of `Property::create()` / `PropertyGroup::add()` for types registered after the property was created.
- Whether `PropertyModel` should accept a non-global `TypeRegistry` (additive if needed later).
- Names returned by `PropertyModel::roleNames()` (become part of the QML-facing contract).
- Duplicate-id behaviour (assert in debug, return existing child) once real usage exists.
