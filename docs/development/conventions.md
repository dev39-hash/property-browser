# Conventions

Match the file you are editing first. This page lists what is common to the whole code base.

## Language level and Qt versions

- **C++17, exactly.** No designated initializers and no C++20 library features. The code must also compile as C++20 in
  consumers; `tests/consumer` builds both. Configuration structs are filled by assigning fields.
- **Qt 6.5 or newer and Qt 5.15 from the same sources.** Differences between the two live only in the private headers
  `qpb/src/core/compat_p.h` and `qpb/src/widgets/compat_p.h`, or in documented `#if QT_VERSION` branches in the public
  headers. Never use a Qt 6-only API elsewhere. New code is built and tested in both configurations (see
  [building-and-testing.md](building-and-testing.md)).
- **Qt modules.** Core, Gui, Widgets and Test only, and `qpb::core` uses Core only (R2). No third-party libraries.
- **No code from other property browser libraries.** qpb is written from scratch; do not copy or adapt code from other
  libraries of the same kind.

## Naming and files

- Namespace `qpb`; internals in `qpb::detail`. Free functions added after 1.0 go in a nested namespace such as
  `qpb::serialization` (see [public-headers.md](../design/public-headers.md)).
- Types in PascalCase, functions in camelCase, enumerators in PascalCase.
- A class with a d-pointer keeps its data in `detail::<Class>Private`, declared in the public header and defined in a
  private header `src/<layer>/<Name>_p.h`. Sources and private headers are named after the class
  (`PropertyModel.cpp`); tests are `tst_<name>.cpp`.
- A new source file is added to the target in `qpb/CMakeLists.txt`. A new public header needs a user (R4) and is picked up
  by the per-header check in `tests/api` after CMake runs again.

## Formatting

- `.clang-format` at the root (WebKit based, 100 columns, `PointerAlignment: Left`). Keep the formatting of the file
  you edit and do not reformat code you did not change.
- Include order, in blocks: `<qpb/...>` headers first, then Qt (`<QtCore/qstring.h>`, `<QtWidgets/...>`), then the
  standard library, then local quoted headers (`"Property_p.h"`).
- Library and tests build without warnings (`/W4`, or `-Wall -Wextra -Wpedantic`), and public headers stay warning-free
  under a consumer's flags. Fix the cause of a warning; do not silence it.
- `QT_NO_CAST_FROM_ASCII` is defined for qpb's own targets: use `QStringLiteral` and `QLatin1String` /
  `QLatin1StringView` (through `compat_p.h` where the two Qt versions differ).

## Comments

- Comments are `//` in the style of the file. Public headers explain behaviour for the people who use qpb.
- A comment is self-contained. It states the behaviour, the constraint and the reason in a few words, in present tense.
  It does not point at a record outside the code: no issue numbers, review notes or task names. Where a reason is
  needed, write the reason itself ("signals carry the path, not a `Property` reference, so queued connections need no
  metatype").
- A comment may name source files, classes, functions and tests, and the documents in this repository by path or
  heading (never by line number), and the architecture constraints R1 to R6 defined in
  [architecture.md](../design/architecture.md). Inside `qpb/`, it only refers to other places in `qpb/`, because the
  folder travels without the rest of the repository.
- A comment says what the code does or why; it does not narrate a change.

## Encoding and language

- **ASCII only under `qpb/`** (R6): write `--`, `->`, `<=`, `x`, never an em dash, an arrow or a section sign. The
  exception is the translation catalogs `qpb/translations/qpb_<lang>.ts`. `tools/check_architecture.cmake` checks
  headers, sources, templates and CMake files.
- **English only** for code, comments, log text, commit messages and tracked documentation.
- Texts that qpb shows itself go through `tr()` or `QCoreApplication::translate()` with a stable context (the
  context is the class name with its namespace, such as `qpb::TypeRegistry`). Within 1.x existing contexts and source
  texts are never changed, because applications may override a wording with a translator of their own; new texts may
  be added. `qWarning()` messages are for developers, stay English and start with `qpb: `. See
  [building-and-testing.md](building-and-testing.md) for the maintenance of the catalogs.
- Do not use absolute paths of a machine in any tracked file; pass them on the command line (`CMAKE_PREFIX_PATH`) or
  through the environment.

## Behaviour that is easy to get wrong

- Properties are owned by the model: hold paths, not `Property*`, across structural changes (see
  [architecture.md](../design/architecture.md)).
- Duplicate or invalid ids are reported with `qWarning` and the tree stays consistent; they never assert.
- A user edit and an application write differ on purpose: read-only and disabled properties restrict the user only.
- The form view rebuilds once per event-loop pass; do not add synchronous rebuilds.
- qpb is a GUI-thread library; do not add threads.

## Tests

- One `tst_<name>.cpp` per class or feature, registered with `qpb_add_test(...)` in the folder's `CMakeLists.txt`.
- Model operations run under `QAbstractItemModelTester` in fatal mode, and signals are checked with `QSignalSpy`.
- No `QTest::qWait(n)` as synchronisation: use `QSignalSpy::wait()` or `QTRY_COMPARE` with a bounded timeout. No real file
  dialogs: use the test hook.
- Expected values are literals, not a round trip through qpb's own code (the serialization tests pin the JSON text).
- Never edit an existing test, an API snapshot or an `api_compat` file to make it pass; a failure there is an API break
  until shown otherwise. A new minor release adds new files (see [public-headers.md](../design/public-headers.md)).

## Commits

- New commits are written as `<type>: <summary>`, with an optional scope as `<type>(<scope>): <summary>`, in the style
  of Conventional Commits. The type is one of `feat`, `fix`, `docs`, `test`, `refactor`, `build` or `chore`. The summary
  is in the imperative and in English. Commits made before October 2026 do not follow this form.
- A new commit that touches `qpb/include` has a line in its body: `API change: none` or
  `API change: additive (<symbols>)`. A breaking change is not made in 1.x (see
  [public-headers.md](../design/public-headers.md)).
- A change that users can see is recorded in the user documentation and under `## Unreleased` in `qpb/CHANGELOG.md`.
- Releases and work travel between machines as git bundles; see [releasing.md](releasing.md).
