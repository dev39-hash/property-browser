# Changelog

All notable changes to qpb are documented here. Each release has the sections
*Added*, *Changed*, *Deprecated*, *Fixed* and *Upgrade notes* (see `doc/compatibility.md`).

From 1.0.0 on, releases within a major version never require changes to consuming code or CMake.
Release candidates (`-rcN`) may still change the API before 1.0.0 if the RC trial shows a problem.

## Unreleased

### Added
- `PropertyFormView::showModelForeground` (a `Q_PROPERTY`, off by default; `qproperty-showModelForeground` in a style
  sheet): labels and section titles show the model's `Qt::ForegroundRole` colour, also while a section is collapsed.
  The colour is taken from the name cell, else the value cell; it follows `dataChanged()` and wins over a `color` of
  the application style sheet (`doc/guide/views.md`, `doc/guide/style-sheets.md`).

### Fixed
- `PropertyFormView` editors keep the text being typed when the model reports a change that leaves the
  value as it was: another role such as the tool tip, a `dataChanged()` without roles, or a whole range. An editor
  is written only when the model's value changed, or after its attributes were applied again. A value the user
  commits that the model normalises to the value it already held (no signal follows) is shown as the model holds it.
- `PropertyModel::setRoot()` and `PropertyGroup::remove()` can be called from a handler of `valueChanged` or
  `batchValueChanged`, also while a group or the whole tree is being reset, or while `qpb::serialization::load()` /
  `fromJson()` writes values: the changed property and the detached
  part of the tree stay alive until the outermost value change has been delivered, so handlers connected later (and
  `QObjectPropertySource`) receive the correct values instead of reading freed memory. `valueChanged` carries copies of
  the values. Outside such a handler both calls still destroy at once. "Reset to default" and "Reset group" of the views
  stop writing properties that a handler removed or replaced.
- Editors that `PropertyDelegate` opens (`PropertyTreeView`, or any view that uses the delegate, also through a proxy
  model) follow changes of the property's attributes, such as a range, `decimals` or a suffix, while they are open. They
  keep the text being typed when the model reports a change that leaves the value as it was (another role such as the
  tool tip, a `dataChanged()` without roles, or a whole range).

## 1.8.0 - 2026-10-21

Eighth feature release of 1.x: additions only; code written for 1.0-1.7 builds and behaves the same. Without a qpb
translation installed, every text is the English one of 1.7.

### Added
- Translations of qpb's own texts for English, Japanese and Vietnamese (`doc/guide/translations.md`):
  `qpb::loadTranslation(translator, locale)` in `<qpb/Translations.h>` loads them from `qpb::core`, where they are
  embedded; nothing to deploy, no Qt Linguist tools needed to build.
- The messages of the built-in validation and of failed conversions are translatable (context `qpb::TypeRegistry`
  and `qpb::Property`); "At most %n characters are allowed" has plural forms.
- Language changes at run time: `PropertyModel` emits `headerDataChanged()` for its column titles, and the file and
  directory editors kept open by `PropertyFormView` update their tool tips.
- The contexts and source texts of qpb's translatable texts are stable within 1.x, so applications can add languages
  or change a wording with a translator of their own.

### Upgrade notes
- Nothing to do. To show qpb in Japanese or Vietnamese, install the translator `qpb::loadTranslation()` fills.

## 1.7.0 - 2026-10-07

Seventh feature release of 1.x: additions only; code written for 1.0-1.6 builds and behaves the same with Qt 6.

### Added
- Qt 5.15 support (`doc/integration.md`, "Qt 5"; `doc/compatibility.md`): CMake uses the host's Qt 6 or Qt 5, otherwise finds Qt 6, then Qt 5.15.
  With Qt 5, `Types::*` and `Attr::*` are `QLatin1String` and `TypeHandler::storageType` is an `int` type id.

### Fixed
- Building the library with MSVC 2019 no longer reports warning C4267 in `src/core/PropertyGroup.cpp`.

### Upgrade notes
- Nothing to do with Qt 6. Qt 5.15 needs MSVC 2019 / 2022, GCC or Clang (not MSVC 2026).

## 1.6.1 - 2026-10-02

Documentation release: no change to the API or behaviour.

### Added
- `README.md` in the component folder: requirements, integration, features, API, developer guide, style sheets,
  compatibility and version history.

### Changed
- `CHANGELOG.md` and the comments of the public headers no longer refer to documents outside the component folder;
  they point to sections of `README.md`.

### Upgrade notes
- Nothing to do.

## 1.6.0 - 2026-09-30

Sixth feature release of 1.x: additions only; code written for 1.0-1.5 builds and behaves the same.

### Added
- Theming with style sheets (`doc/guide/style-sheets.md`):
  - `PropertyTreeView` colour properties `groupBackground`, `groupForeground`, `modifiedForeground` and
    `readOnlyForeground`, settable from a sheet with `qproperty-` (invalid values keep the defaults);
  - style sheet selectors on the widgets of `PropertyFormView` and of the file / directory editors: the dynamic
    properties `qpbPart` (`group`, `groupTitle`, `groupBody`, `label`, `value`, `browse`) and `qpbModified`.

### Fixed
- `PropertyFormView`: labels of modified properties lost their bold font under any application style sheet with a rule
  for `QLabel`.

### Upgrade notes
- Nothing to do.

## 1.5.0 - 2026-09-30

Fifth feature release of 1.x: additions only; code written for 1.0-1.4 builds and behaves the same.

### Added
- `PropertyModel::resetAllToDefault()`: resets the whole tree in one batch, like `root()->resetToDefault()`
  (read-only and disabled properties too, live ones not); connectable to a "Restore Defaults" button.
- `PropertyTreeView::setTabStopsOnCheckBoxes()` (also a Q_PROPERTY): Tab / Shift+Tab also stop on check boxes, where
  Space toggles them. Off by default.

### Fixed
- Building `qpb/` with MSVC no longer reports warning C4458 ("declaration hides class member") in
  `src/core/Property.cpp`.

### Upgrade notes
- Nothing to do. Tab keeps skipping check boxes unless `setTabStopsOnCheckBoxes(true)` is called.

## 1.4.0 - 2026-09-29

Fourth feature release of 1.x: additions only; code written for 1.0-1.3 builds and behaves the same.

### Added
- Conditions between properties: `Property::setEnabledWhen()` / `setVisibleWhen()` (and the builder methods
  `enabledWhen()` / `visibleWhen()`, `clearEnabledWhen()` / `clearVisibleWhen()`): enabled or visible while another
  property's value is true, equals a value, or passes a predicate. Evaluated by the model and combined with the
  property's own flags; views follow them without code.
- `PropertyModel::onValueChanged(path, context, handler)`: a callback for one path (or a group and its descendants),
  instead of comparing paths in `valueChanged`.
- `QObjectPropertySource` metadata keys `enabledWhen` and `visibleWhen`.

### Upgrade notes
- Nothing to do.

## 1.3.0 - 2026-09-27

Third feature release of 1.x: additions only; code written for 1.0-1.2 builds and behaves the same.

### Added
- `Property::Flag::Live` with `isLive()` / `setLive()` and the builder method `live()`: values maintained by the
  application are never modified, are left alone by group resets and are not saved by `qpb::serialization`.
- `QObjectPropertySource`: group titles from a Q_PROPERTY (`Q_CLASSINFO("qpb:title", "name")` or
  `setTitleProperty()`), the metadata key `live`, and `setLiveReadOnlyProperties()` for Q_PROPERTYs the object updates
  itself.

### Upgrade notes
- Nothing to do. Read-only Q_PROPERTYs keep their 1.2 behaviour unless `setLiveReadOnlyProperties(true)` is called.

## 1.2.0 - 2026-09-27

Second feature release of 1.x: additions only; code written for 1.0 or 1.1 builds and behaves the same (see the
upgrade note on `"int64"`).

### Added
- `QObjectPropertySource` (`qpb/QObjectPropertySource.h`): shows the Q_PROPERTYs of QObjects in a `PropertyModel` and keeps
  both in sync; ranges, display names and path types come from `Q_CLASSINFO("qpb:<property>", "min=0;max=10")`.
- `qpb::serialization::toJson/fromJson/save/load` (`qpb/Serialization.h`): property values to and from JSON and
  `QSettings`; `TypeHandler::toJson` / `fromJson` for types with their own JSON form.
- `Types::Int64` with `PropertyGroup::addInt64()` / `Int64Builder` and a 64-bit spin box editor.

### Upgrade notes
- Nothing to do, unless the application registered its own type with the ID `"int64"`: `registerType()` now returns
  false for it and the built-in type is used.

## 1.1.0 - 2026-09-27

First feature release of 1.x: additions only; code written for 1.0 builds and behaves the same.

### Added
- `PropertyFormView` (`qpb/widgets/PropertyFormView.h`): the properties of a `PropertyModel` (or a proxy of one) as a
  form with persistent editors and collapsible group sections, kept in sync with the model in both directions.
- `PropertyFilterProxyModel` (`qpb/PropertyFilterProxyModel.h`, in `qpb::core`): filters by display name for a search
  box; works with `PropertyTreeView` and `PropertyFormView`.
- `Attr::Multiline` / `StringBuilder::multiline()`: multi-line strings, edited with a `QPlainTextEdit`
  (Enter adds a line, Ctrl+Enter commits) and shown on one line in cells.

### Upgrade notes
- Nothing to do.

## 1.0.0 - 2026-09-27

First stable release: the same content as `1.0.0-rc2`, which passed a trial in an application without API or
behaviour changes. The public API is frozen for 1.x.

### Upgrade notes
- From `1.0.0-rc2`: nothing to do.
- From `1.0.0-rc1`: see the `1.0.0-rc2` upgrade notes below.

## 1.0.0-rc2 - 2026-09-27

Second release candidate, after its first trial in an application.

### Changed
- Read-only and disabled now only block edits by the user through views (`PropertyModel::setData`).
  Application code (`Property::setValue`, `PropertyModel::setValue`, `resetToDefault`) can set and reset
  such properties, still with conversion and validation.
- The "Reset to default" / "Reset group" context menu leaves read-only and disabled properties unchanged.
- `PropertyTreeView` fits the name column to its contents until a width is set with `setNameColumnWidth()`
  or by dragging the header.

### Upgrade notes
- Code that relied on `setValue()` failing for read-only or disabled properties must check those flags itself.

## 1.0.0-rc1 - 2026-09-27

First release candidate of the 1.0 API.

### Added
- Component folder `qpb/`: `add_subdirectory(components/qpb)` plus `qpb::core` / `qpb::widgets`; static libraries
  by default, `QPB_BUILD_SHARED=ON` for shared ones. Requires C++17, Qt 6.5, CMake 3.21; changes none of the host
  project's CMake settings.
- `qpb::core`: `Property`, `PropertyGroup` and typed builders; `TypeRegistry` with seven built-in types (bool, int,
  double, string, enum, file path, directory path) and custom types; `PropertyModel`, a two-column
  `QAbstractItemModel` with validation, change signals, batches and reset to default.
- `qpb::widgets`: `EditorFactory` with editors for the built-in types and custom editors (`EditorDialogScope`,
  `notifyCommit`); `PropertyDelegate`; `PropertyTreeView` with Tree and List modes and a reset context menu.
- Headers `qpb/qpb.h` (everything) and `qpb/qpbcore.h` (core only); version macros and `qpb::version()`;
  deprecation macros for future releases.

### Upgrade notes
- Not applicable (first release).
