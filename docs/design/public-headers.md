# Public headers

The headers under `qpb/include/qpb/` are the API of qpb, and they are its normative reference: what a header declares is
what qpb promises. Within a major version (1.x) a project that builds against one release must build and behave the same
against a later one by only replacing the `qpb/` folder. What that means for users, and the table of allowed and
forbidden changes, is in [`qpb/doc/compatibility.md`](../../qpb/doc/compatibility.md); this page is about how the
headers are designed and how the promise is kept.

The promise is source compatibility, not binary compatibility: consumers always rebuild qpb from source. The d-pointer
(below) is used anyway, to keep headers small and stable and to leave the internals free to change.

## Header design rules

- **A d-pointer for stateful classes.** `Property`, `PropertyGroup`, `PropertyModel`, `TypeRegistry`, `EditorFactory`,
  the views and the other stateful classes hold their data in a `std::unique_ptr<detail::<Name>Private>`. The header
  declares the private class but never defines it, and has no public or protected data members besides that pointer.
  Public headers never include a `_p.h` file (R5). Reason: members can be added or changed in the `.cpp` side
  without touching what a consumer compiles against.
- **Aggregates grow at the end.** The configuration structs (`TypeHandler`, `EditorHandler`, `EnumOption`,
  `ValidationResult`) are aggregates. A new field is only ever added after the existing ones, and an empty field means
  the old behaviour. Users are told to assign fields one by one, never to use positional initialization (`{a, b, c}`),
  because that would change meaning when a field is added. Designated initializers are C++20 and so not used.
- **Only Qt and std types in the API.** No third-party types, so a consumer needs nothing beyond Qt.
- **No logic in inline functions or templates.** Builders and templates only forward to functions in `.cpp` files, so a
  fix is made in one place and does not depend on what the consumer's compiler inlined.
- **Self-contained headers.** Every public header compiles when included alone, and twice in a row (include guard).
  This is checked for each header by the compile-only library in `tests/api`, with strict warnings and warnings as
  errors. A new header is picked up automatically after CMake runs again.
- **Warning-free for consumers.** Public headers compile without warnings under a consumer's `-Wall -Wextra -Wpedantic`
  or `/W4 /WX`.
- **ASCII only (R6) and no internals (R5).**
- **Free functions added after 1.0 go into a nested namespace**, for example `qpb::serialization`, never directly
  into `qpb`. Reason: argument-dependent lookup (ADL) finds a function in `qpb` for any call whose arguments are qpb types.
  An application that has its own unqualified `save(group, settings)` would then get an ambiguous call after upgrading
  to a qpb that adds a function of the same name. A function in a nested namespace is never found by ADL, so the
  application's call stays unambiguous. `tests/api/future_sketches.cpp` and `tests/api_compat/v1_2.cpp` contain such
  application functions.
- **Qt 5 and Qt 6 from the same headers.** Where Qt 5 has no equivalent of a Qt 6 type, the public header has a
  documented `#if QT_VERSION` branch (for example `Types::*` and `Attr::*` are `QLatin1StringView` constants with Qt 6 and
  `QLatin1String` with Qt 5). Everything else is in the private `compat_p.h` headers; see
  [conventions.md](../development/conventions.md).

## Adding an API

1. **Classify the change** as none, additive or breaking against the header rules above and the table in
   [`qpb/doc/compatibility.md`](../../qpb/doc/compatibility.md). A breaking change is not made in 1.x. Typical traps:
   - adding a parameter to an existing function, even a defaulted one (it changes the function's type, which breaks
     code that takes its address or connects to it);
   - adding a pure virtual function to a class users subclass;
   - inserting an enum value in the middle, or changing the number of an existing role;
   - changing a documented default, the signature of a signal, or the context or source text of a translatable text.
2. **Give it a user** (R4): an example or a test. An API nobody uses is not added.
3. **Write the header** following the rules above. A new class or function that is not a member goes into a nested
   namespace.
4. **Test it.** Unit tests in `tests/core` or `tests/widgets`, and for every minor release that adds API a file
   `tests/api_compat/v<major>_<minor>.cpp` that exercises all the new public API (signals, builders and handlers
   included) the way an application would. It is listed in `tests/api_compat/CMakeLists.txt`.
5. **Take the API snapshot at release time**, for Qt 6 and for Qt 5 (see below).
6. **Document it** in the user documentation (`qpb/doc/`, `qpb/README.md`) and in `qpb/CHANGELOG.md` under *Added*.
7. **Say so in the commit.** A commit that touches `qpb/include` has the line `API change: none` or
   `API change: additive (<symbols>)` in its body; see [conventions.md](../development/conventions.md).

### Deprecation

To mark something deprecated, use `QPB_DEPRECATED_X("use X instead")` inside `#if !defined(QPB_DISABLE_DEPRECATED)`,
and name the replacement in `qpb/CHANGELOG.md` under *Deprecated* and in the upgrade notes. How deprecation works for
users, and the version macros (generated from `qpb/VERSION` at configure time), are in
[`qpb/doc/compatibility.md`](../../qpb/doc/compatibility.md#deprecation).

## What guards compatibility

Compatibility is checked by tests that run with every `ctest`, in both Qt configurations (see
[building-and-testing.md](../development/building-and-testing.md)):

- **Self-contained header check** (`tests/api`): each public header compiled alone.
- **`tests/api_compat/v<major>_<minor>.cpp`**: one file per minor release that added API. It exercises all the public
  API of that release as client code, and is built and run by every later release (deprecation warnings are allowed,
  other warnings are errors). **A file of a released version is never edited.** Qt 5 has `qt5_v1_7.cpp` for the forms
  of the API that differ there; `v1_0.cpp` uses `QMetaType` values and is built with Qt 6 only.
- **API snapshots** (`tests/api_compat/api-<major>.<minor>.txt` for Qt 6 and `api-qt5-<major>.<minor>.txt` for Qt 5):
  `tools/api_snapshot.py` lists the public declarations by parsing the headers. The CTest tests `api_snapshot_<version>` and
  `api_snapshot_qt5_<version>` run `--check` for every released version and fail when a line of a snapshot is missing
  from the current headers, that is when something was removed or changed. Additions pass. A snapshot is never edited
  after its release; each later release is checked against all earlier ones, so the newest header set is always a
  superset of every earlier one.

When a minor release is prepared, the snapshot of the new version is written once, from the release commit:

```sh
python3 tools/api_snapshot.py --include qpb/include --write tests/api_compat/api-<major>.<minor>.txt
python3 tools/api_snapshot.py --include qpb/include --qt-major 5 --write tests/api_compat/api-qt5-<major>.<minor>.txt
```

Both are listed in `tests/api_compat/CMakeLists.txt`. The release steps are in
[releasing.md](../development/releasing.md).

## What may change within 1.x

What may and may not change, the bug-fix exception and the Qt 5 differences are in the table and sections of
[`qpb/doc/compatibility.md`](../../qpb/doc/compatibility.md#what-may-and-may-not-change-within-1x).

Everything in `qpb::detail`, under `qpb/src/` and in `_p.h` files is internal and may change at any time (R5).
