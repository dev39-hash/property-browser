# Building and testing

## Requirements

- CMake 3.21 or newer, and a build tool (the commands below use Ninja).
- A C++17 compiler: MSVC, GCC or Clang.
- Qt 6.5 or newer, or Qt 5.15, with the Core, Gui, Widgets and Test modules.
- Python 3 for the API snapshot tests and the translations check. Without it those tests are not registered, and CMake
  says so when it configures.
- Qt Linguist's `lupdate` and `lrelease` only to maintain translations (see below). Building qpb never needs them.

## Configure, build, test

```sh
cmake -S <source dir> -B <build dir> -G Ninja -DCMAKE_BUILD_TYPE=<Debug|Release> -DCMAKE_PREFIX_PATH=<Qt prefix>
cmake --build <build dir>
ctest --test-dir <build dir> --output-on-failure
```

- `<Qt prefix>` is the installation directory of one Qt, the one that contains `lib/cmake/Qt6` or `lib/cmake/Qt5`.
  The root `CMakeLists.txt` takes Qt 6 when it can find it and Qt 5 otherwise, so give each Qt its own build directory
  and point `CMAKE_PREFIX_PATH` at that Qt only.
- With a multi-configuration generator (Visual Studio), leave out `CMAKE_BUILD_TYPE` and pass
  `--config <Debug|Release>` to `cmake --build` and `-C <Debug|Release>` to `ctest`.
- With MSVC and Ninja, run the commands in a prompt where the compiler is set up (for example a Visual Studio native
  tools prompt).
- Tests need the Qt libraries on `PATH` (Windows) or in the library path. A test that dies with exit code `0xC0000135`
  did not find a Qt DLL; that is an environment problem, not a test failure.
- Options of the root project: `QPB_BUILD_TESTS` (default `ON`) and `QPB_BUILD_EXAMPLES` (default `ON`). The examples are
  built with every build and use qpb exactly as a consumer would. `QPB_BUILD_SHARED` (default `OFF`) is an option of the
  component itself and builds shared libraries.

## The two configurations

qpb is built and tested with two Qt configurations from the same sources:

| Configuration | Qt |
|---|---|
| Qt 6 | 6.5 or newer |
| Qt 5.15 | 5.15 |

The supported platforms and compilers for each, including the compilers Qt 5.15 accepts, are in
[qpb/doc/integration.md](../../qpb/doc/integration.md#platforms).

The API and behaviour are the same. The differences are the `#if QT_VERSION` branches in a few public headers and the
private `compat_p.h` files (see [conventions.md](conventions.md)). The Qt 5 configuration has an extra compatibility test
(`qt5_v1_7`) and its own API snapshots; `v1_0` is built with Qt 6 only (see
[public-headers.md](../design/public-headers.md)).

CTest runs every test with `QT_QPA_PLATFORM=offscreen` in its environment (`qpb_add_test` in `tests/CMakeLists.txt`), so
no display is needed; on Windows with Qt 5 it also sets `QT_QPA_FONTDIR` to the system fonts, because Qt 5's offscreen
platform otherwise finds no fonts. A test executable started directly does not get these variables: set them by hand
first.

A change to anything under `qpb/`, to `tests/api_compat/`, or to a test of a Qt-version branch is tested in both
configurations; other changes in Qt 6.

## Test layers

All tests are registered with CTest. The ones named `tst_*` are Qt Test executables; run one with
`ctest -R <name>`.

| Layer | Where | What it checks |
|---|---|---|
| Core | `tests/core` | The builder, paths and `find()`, effective state inherited from ancestors, the type registry, the validation pipeline, conditions, serialization, `QObjectPropertySource`, translations and the filter proxy. No GUI. |
| Model | `tests/core` (`tst_propertymodel` and the proxy and source tests) | Every structural and value operation runs under `QAbstractItemModelTester` in fatal mode; signals are checked with `QSignalSpy`; batches. |
| Widgets | `tests/widgets` | Editors for all types: create, commit, cancel; Enter, Esc, Tab; focus-out; Tree and List modes; the form view; the reset menu; style sheet hooks; a path editor that must not close while a dialog is open (the file dialog is replaced by a test hook). Run on the offscreen platform. |
| API compile checks | `tests/api` | Every public header compiles alone and twice (strict warnings, warnings as errors), plus compile-only sketches of how an application could use future API. Failing means a build error. |
| API compatibility | `tests/api_compat` | `tst_api_compat_<version>`: client code of each released minor version builds and runs against the current headers. `api_snapshot_<version>` and `api_snapshot_qt5_<version>`: the public declarations of each released version are still present. See [public-headers.md](../design/public-headers.md). |
| Consumer | `tests/consumer` | `consumer_cxx17`, `consumer_cxx20`, `consumer_shared`, `consumer_upgrade`: a throwaway application embeds a copy of `qpb/` and must build, run and leave the host's CMake settings untouched. These tests are slow (label `consumer`, timeout 15 minutes each). See [component-cmake.md](../design/component-cmake.md). |
| Repository checks | `tests/CMakeLists.txt` | Described below. |

Widget tests include `qpb/src` so they can reach test hooks such as `PathEdit::setDialogProviderForTesting()`.

### Repository checks

| Test | What it checks |
|---|---|
| `architecture_rules` | `tools/check_architecture.cmake`: R1, R2, R5 and R6 (see [architecture.md](../design/architecture.md)). |
| `qtcreator_examples_listed` | `tools/check_qtcreator_examples.cmake`: every folder in `examples/` is built by `examples/CMakeLists.txt`, listed in `QPB_EXAMPLES` of `examples/qtcreator/qpb_examples.pro` and has `examples/qtcreator/qmake/<name>/<name>.pro`; and the qmake project lists nothing else. |
| `translations_up_to_date` | `tools/update_translations.py --check`: the `.ts` files match the translatable texts in the sources and have no unfinished translation. Registered only when `lupdate` and Python 3 are found. |
| `size_quickstart_s1` | `tools/count_lines.cmake`: `examples/quickstart/main.cpp` has at most 30 lines, counting lines that are not blank, not `//` comments and not `#include`. |
| `size_custom_type_s2` | The same count for `examples/custom_type/ColorType.h` and `ColorType.cpp` together: at most 100 lines. |

The two size limits express the promise that a ten-property panel takes about thirty lines and a custom `QColor` type
about a hundred, outside the library.

`tst_translations` (in `tests/core`) checks that the committed `.qm` files match their `.ts` files.

## Before a release

What a release must pass, and why the widget suites are repeated on a Release build, is in
[releasing.md](releasing.md#2-verify). To run the repeat loop for one suite:

```sh
ctest --test-dir <build dir> -R tst_propertytreeview --repeat until-fail:25
```

## The trial application

`rc-trial/` is a standalone application that uses qpb the way a project outside this repository does, through a release
zip. It is not part of the root build. Nothing from the development repository is used: the zip is extracted to
`rc-trial/components/qpb/` and added with `add_subdirectory`, and the code uses the public API only. It has four tabs
(an inspector, a settings page, plugin configuration and a device list) with a custom type and a custom editor, and a
Qt Test (`tst_trial`) that drives them on the offscreen platform and saves screenshots when `QPB_TRIAL_SCREENSHOTS`
names a directory.

Its purpose is to prove that a release candidate can be used with the public API alone before the release is made. Run
it against a zip made by `tools/make_release.sh` (see [releasing.md](releasing.md)):

```sh
rc-trial/run_trial.sh <path to qpb-<version>.zip> [<Qt prefix> [<extra CMake arguments>...]]
```

The script (bash; needs `unzip` and Ninja) replaces `rc-trial/components/`, configures a Debug build in `rc-trial/build`
(set `TRIAL_BUILD_DIR` to use another directory), builds it, and runs `ctest`. Screenshots of the pages are written to
`<build>/screenshots/`. Extra arguments go to CMake, for example `-DQPB_BUILD_SHARED=ON` or
`-DCMAKE_CXX_COMPILER=clang++`. `components/` and `build/` in `rc-trial/` are ignored by git.

## Translations

qpb's own texts are translated into Japanese and Vietnamese; English is the source language. The catalogs are
`qpb/translations/qpb_<lang>.ts` (`qpb_en.ts` holds the plural forms only) and the compiled `.qm` files next to them,
which are committed and embedded in `qpb::core`. `examples/translations` has a catalog of its own.

When translatable texts change:

```sh
python3 tools/update_translations.py --qt-bin <Qt prefix>/bin
```

This runs `lupdate` for every catalog (new texts are added as unfinished, texts that left the sources are dropped), and
`lrelease` to write the `.qm` files. Translate the unfinished texts in the `.ts` files (Qt Linguist or an editor), run
the script again, and commit the `.ts` and `.qm` files. Existing contexts and source texts are not changed within 1.x,
because applications may override them (see [conventions.md](conventions.md)). `--check` changes nothing and is what the
`translations_up_to_date` test runs.

## Qt Creator projects

`examples/qtcreator/` has two projects that build the library and all examples, for opening in Qt Creator:

- `CMakeLists.txt`: builds `../../qpb` and reuses `../CMakeLists.txt`, so a new example shows up without further work.
  Open it, pick a Qt 6.5+ or Qt 5.15 kit and choose the example in the run target selector. From a terminal:
  `cmake -S examples/qtcreator -B <build dir> -DCMAKE_PREFIX_PATH=<Qt prefix>`.
- `qpb_examples.pro`: a qmake project for those who work with qmake. It builds the same sources as one static library
  (`qmake/qpb`) and one executable per example (`qmake/<example>/<example>.pro`). It only serves the examples; projects
  integrate qpb with CMake.

When an example is added under `examples/`, it is added to `examples/CMakeLists.txt`, to `QPB_EXAMPLES` in
`qpb_examples.pro` and as `qmake/<example>/<example>.pro`; `qtcreator_examples_listed` fails otherwise.
