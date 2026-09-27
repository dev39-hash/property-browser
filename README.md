# property-browser (qpb)

A property browser library for Qt 6 Widgets (namespace `qpb`): declare properties once, display them as a tree, a
flat list or (since 1.1) a form on top of a single `QAbstractItemModel`, filter them with a search box, and add data types
and editors without modifying the library. Since 1.2 it can also edit the Q_PROPERTYs of QObjects directly and save values
to JSON or `QSettings`; 1.3 adds live values and titles taken from the objects.

Status: 1.3.0 released. The public API is stable within a major version (see below).

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
plugin configuration, (1.1) a form view and a tree view sharing one search filter, and (1.2) an editor for QObjects
with undo/redo and JSON files.

## Using qpb in a project

qpb is distributed as a self-contained component folder. Copy `qpb/` into your project (for example to
`components/qpb/`) and add:

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

Get the folder from a release (`qpb-<version>.zip`), or keep it in sync with
`git subtree pull --prefix components/qpb <this repository> qpb-release --squash`.

## Upgrading

Replace `components/qpb/` with the new version and rebuild; read the version's *Upgrade notes* in
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
