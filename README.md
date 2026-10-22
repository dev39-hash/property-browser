# property-browser (qpb)

A property browser library for Qt 6 and Qt 5.15 Widgets (namespace `qpb`): declare properties once, display them as a
tree, a flat list or a form on top of a single `QAbstractItemModel`, filter them with a search box, and add data types
and editors without modifying the library. It can also edit the Q_PROPERTYs of QObjects directly, save values to JSON
or `QSettings`, and switch its own texts between languages at run time.

Status: 1.8.0 released. The public API is stable within a major version (see below). What each version added:
[Version history](#version-history).

```cpp
auto root = qpb::PropertyGroup::create("Camera");
root->addString("name", "Main camera");
root->addDouble("x", 0.0).range(-100.0, 100.0).suffix(" m");
root->addEnum("projection", {"Perspective", "Orthographic"}, 0);
root->addFilePath("lut", {}).filter("LUT files (*.cube)");

qpb::PropertyModel model(std::move(root));
qpb::PropertyTreeView view;                 // Tree or List mode
view.setModel(&model);
QObject::connect(&model, &qpb::PropertyModel::valueChanged,
    [](const QString& path, const QVariant& value) { qDebug() << path << value; });
```

More in [`examples/`](examples): quickstart, a custom `QColor` type, an inspector, a settings dialog, a runtime
plugin configuration, a form view and a tree view sharing one search filter, an editor for QObjects with undo/redo and
JSON files, a dark and a light theme from one style sheet template
([`examples/custom_theme`](examples/custom_theme); styling hooks in [docs/SPEC.md section 5.8](docs/SPEC.md)), and
English, Japanese and Vietnamese switched at run time ([`examples/translations`](examples/translations);
[docs/SPEC.md section 4.10](docs/SPEC.md)).

To build and run them all in Qt Creator, open [`examples/qtcreator/CMakeLists.txt`](examples/qtcreator/CMakeLists.txt)
(or [`qpb_examples.pro`](examples/qtcreator/qpb_examples.pro) with qmake), pick a Qt 6.5+ or Qt 5.15 kit and choose the example in
the run target selector. The qmake project only serves the examples; projects integrate qpb with CMake (see below).

## Using qpb in a project

qpb is distributed as a self-contained component folder. Its own [`qpb/README.md`](qpb/README.md) travels with it
and documents features, API, a developer guide and the version history for projects that only have the folder.
Copy `qpb/` into your project (for example to `components/qpb/`) and add:

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)   # or find_package(Qt5 5.15 ...) for Qt 5
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)   # or qpb::core without widgets
```

```cpp
#include <qpb/qpb.h>       // everything; <qpb/qpbcore.h> for qpb::core only
```

Requirements: C++17 or newer, Qt 6.5 or newer or (since 1.7) Qt 5.15, CMake 3.21 or newer. The component changes none of your project's
CMake settings. Static libraries are built by default; with `-DQPB_BUILD_SHARED=ON` you must deploy the qpb libraries
next to your executable.

### Getting and updating the folder

Three ways, all giving the same `components/qpb/`, so the CMake above does not depend on the choice. Each release
produces `qpb-<version>.zip` with `tools/make_release.sh` (docs/RELEASING.md) and the tag `qpb-v<version>` on the branch
`qpb-release`, which holds the component folder alone.

- **Release zip** (default, no git needed). Delete the old folder first, so files removed by the new version do not
  linger:

  ```sh
  rm -rf components/qpb && unzip qpb-1.4.0.zip -d components
  ```

- **git subtree** (from a clone of this repository): the folder is committed in your repository; pick the version with
  the tag.

  ```sh
  git subtree add  --prefix components/qpb <a clone of this repository> qpb-v1.4.0 --squash   # first time
  git subtree pull --prefix components/qpb <a clone of this repository> qpb-v1.5.0 --squash   # update
  ```

- **git submodule** (from a clone of this repository): your repository records a commit of `qpb-release`.

  ```sh
  git submodule add -b qpb-release <a clone of this repository> components/qpb                # first time
  git -C components/qpb fetch --tags && git -C components/qpb checkout qpb-v1.4.0  # first time and every update
  git add components/qpb && git commit -m "Use qpb 1.4.0"
  ```

  `git submodule update --remote components/qpb` moves to the newest release instead of a chosen one.

Do not edit files inside `components/qpb/`: the next update replaces them.

## Upgrading

Replace `components/qpb/` with the new version (as above) and rebuild; read the version's *Upgrade notes* in
`qpb/CHANGELOG.md`. Within a major version (1.x) this never requires changes to your code or CMake:

- nothing public is removed or changed, only added;
- superseded API is marked deprecated (`QPB_DEPRECATED_X`) and kept until the next major version; define
  `QPB_DISABLE_DEPRECATED` to find remaining uses at compile time;
- raising the C++, Qt or CMake minimum only happens in a new major version.

The rules and how they are enforced (API compatibility tests, API snapshot) are in
[docs/SPEC.md section 9](docs/SPEC.md#9-api-stability-policy).

## Developing qpb

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.5.3/gcc_64
cmake --build build
ctest --test-dir build --output-on-failure
```

The test suite includes unit and widget tests, the API compatibility test, the API snapshot check
(needs Python 3), architecture-rule and size checks, and `tests/consumer`, which builds throwaway applications that
embed a copy of `qpb/` exactly like a consuming project (static, shared, C++17/C++20, in-place upgrade).
Releases: [docs/RELEASING.md](docs/RELEASING.md).

Platforms: Windows (MSVC) is verified for every release; Linux (GCC, Clang) is supported. macOS is not supported.

## Version history

Each version has a section in [`qpb/CHANGELOG.md`](qpb/CHANGELOG.md) with what was added, changed, deprecated and
fixed, and its upgrade notes.

- **1.8.0** - translations of qpb's own texts (English, Japanese, Vietnamese).
- **1.7.0** - Qt 5.15 as a second configuration.
- **1.6.1** - documentation only: `qpb/README.md` in the component folder.
- **1.6.0** - style sheet hooks for the tree and form views.
- **1.5.0** - `resetAllToDefault()`, check boxes in the Tab chain.
- **1.4.0** - `onValueChanged()` callbacks, `enabledWhen()` / `visibleWhen()` conditions.
- **1.3.0** - live values, group titles from `QObjectPropertySource`.
- **1.2.0** - `QObjectPropertySource`, JSON and `QSettings` serialization, `Int64`.
- **1.1.0** - `PropertyFormView`, `PropertyFilterProxyModel`, multi-line strings.
- **1.0.0** - first stable release: model, tree view, editors for the basic types.

## Documentation

- [Specification](docs/SPEC.md)
- [Implementation plan](docs/PLAN.md)
- [1.0 API review](docs/api-review.md)
- [Reference use cases](docs/use-cases.md)
- [Releasing](docs/RELEASING.md)
- [RC trial report](docs/rc-trial.md)
