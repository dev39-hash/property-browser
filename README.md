# property-browser (qpb)

A property browser library for Qt 6 Widgets (namespace `qpb`): declare properties once, display them as a tree, a
flat list or (since 1.1) a form on top of a single `QAbstractItemModel`, filter them with a search box, and add data types
and editors without modifying the library. Since 1.2 it can also edit the Q_PROPERTYs of QObjects directly and save values
to JSON or `QSettings`; 1.3 adds live values and titles taken from the objects; 1.4 adds conditions between properties
(`enabledWhen`, `visibleWhen`) and per-property change callbacks.

Status: 1.6.0 released. The public API is stable within a major version (see below).

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
plugin configuration, (1.1) a form view and a tree view sharing one search filter, (1.2) an editor for QObjects
with undo/redo and JSON files, and (1.6) a dark and a light theme from one style sheet template
([`examples/custom_theme`](examples/custom_theme); styling hooks in [docs/SPEC.md section 5.8](docs/SPEC.md)).

To build and run them all in Qt Creator, open [`examples/qtcreator/CMakeLists.txt`](examples/qtcreator/CMakeLists.txt)
(or [`qpb_examples.pro`](examples/qtcreator/qpb_examples.pro) with qmake), pick a Qt 6.5+ kit and choose the example in
the run target selector. The qmake project only serves the examples; projects integrate qpb with CMake (see below).

## Using qpb in a project

qpb is distributed as a self-contained component folder. Its own [`qpb/README.md`](qpb/README.md) travels with it
and documents features, API, a developer guide and the version history for projects that only have the folder.
Copy `qpb/` into your project (for example to `components/qpb/`) and add:

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)   # or qpb::core without widgets
```

```cpp
#include <qpb/qpb.h>       // everything; <qpb/qpbcore.h> for qpb::core only
```

Requirements: C++17 or newer, Qt 6.5 or newer, CMake 3.21 or newer. The component changes none of your project's
CMake settings. Static libraries are built by default; with `-DQPB_BUILD_SHARED=ON` you must deploy the qpb libraries
next to your executable.

### Getting and updating the folder

Three ways, all giving the same `components/qpb/`, so the CMake above does not depend on the choice. Each release
publishes `qpb-<version>.zip` on its GitHub Release and the tag `qpb-v<version>` on the branch `qpb-release`, which holds
the component folder alone.

- **Release zip** (default, no git needed). Delete the old folder first, so files removed by the new version do not
  linger:

  ```sh
  rm -rf components/qpb && unzip qpb-1.4.0.zip -d components
  ```

- **git subtree**: the folder is committed in your repository; pick the version with the tag.

  ```sh
  git subtree add  --prefix components/qpb <this repository> qpb-v1.4.0 --squash   # first time
  git subtree pull --prefix components/qpb <this repository> qpb-v1.5.0 --squash   # update
  ```

- **git submodule**: your repository records a commit of `qpb-release`.

  ```sh
  git submodule add -b qpb-release <this repository> components/qpb                # first time
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

## Documentation

- [Specification](docs/SPEC.md) ([Tiếng Việt](docs/SPEC-vi.md))
- [Implementation plan](docs/PLAN.md) ([Tiếng Việt](docs/PLAN-vi.md))
- [1.0 API review](docs/api-review.md)
- [Reference use cases](docs/use-cases.md)
- [Releasing](docs/RELEASING.md)
- [RC trial report](docs/rc-trial.md)
- [Original brainstorm (Vietnamese)](docs/brainstorm-vi.md)
