# Changelog

All notable changes to qpb are documented here. Each release has the sections
*Added*, *Changed*, *Deprecated*, *Fixed* and *Upgrade notes* (see `docs/SPEC.md` section 9.4).

From 1.0.0 on, releases within a major version never require changes to consuming code or CMake.
Release candidates (`-rcN`) may still change the API before 1.0.0 if the RC trial shows a problem.

## 1.0.0-rc2 - 2026-09-27

Second release candidate, after the first RC trial round (docs/rc-trial.md).

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
