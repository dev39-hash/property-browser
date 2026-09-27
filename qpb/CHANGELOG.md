# Changelog

All notable changes to qpb are documented here. Each release has the sections
*Added*, *Changed*, *Deprecated*, *Fixed* and *Upgrade notes* (see `docs/SPEC.md` §9.4).

Versions below 1.0 are internal pre-releases; their API may change without notice.

## Unreleased

### Added
- Component folder skeleton: `qpb::core` and `qpb::widgets` targets, `add_subdirectory` integration.
- `qpb/qpbglobal.h`: export macros, version macros generated from `VERSION`, deprecation macros.
- `qpb::version()` runtime version query.
- 1.0 public API declared (not implemented yet): `Property`, `PropertyGroup`, builders, `TypeRegistry`,
  `PropertyModel`, `EditorFactory`, `EditorDialogScope`, `PropertyDelegate`, `PropertyTreeView`; umbrella headers
  `qpb/qpb.h` and `qpb/qpbcore.h`.
- `qpb::core` implemented: `Property`, `PropertyGroup`, builders, `TypeRegistry` with the seven built-in types
  (conversion, clamping/rounding, validation, display text) and `PropertyModel` (roles, flags, value pipeline,
  live structural changes, batches, reset to default).
- `qpb::widgets` implemented: `EditorFactory` with editors for the seven built-in types, `EditorDialogScope`,
  `PropertyDelegate` (Enter/Escape/Tab/focus-out, dialog focus protection, check boxes, validation tool tip,
  group/modified/read-only painting) and `PropertyTreeView` (Tree and List modes, hidden rows, reset context menu).

### Upgrade notes
- Not applicable (first pre-release).
