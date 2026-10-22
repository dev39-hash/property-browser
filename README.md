# property-browser (qpb)

qpb is a property browser library for Qt 6 and Qt 5.15 Widgets (namespace `qpb`). You declare a tree of properties
once, and qpb shows it as a tree, a flat list or a form on top of a single `QAbstractItemModel`, with a search filter
and an editor for every data type. You add your own data types and editors without modifying the library.

- One model, three presentations: tree, list and form, switchable without losing values.
- A builder API for the basic types (bool, integers, double, string, enum, file and directory paths), with ranges,
  validators and conditions between properties.
- Custom types and editors registered from application code.
- Editing the `Q_PROPERTY`s of `QObject`s directly, and saving values to JSON or `QSettings`.
- Themes through Qt style sheets, and its own texts in English, Japanese and Vietnamese, switchable at run time.
- Distributed as one self-contained folder, `qpb/`, that you copy into your project; two lines of CMake use it.

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

## Quick start

Copy the `qpb/` folder into your project (for example to `components/qpb/`) and add it with `add_subdirectory`. The
CMake lines, getting and updating the folder (release zip, git subtree, git submodule), the options, shared libraries,
Qt 5 and upgrading are in [`qpb/doc/integration.md`](qpb/doc/integration.md); a short quick start is in
[`qpb/README.md`](qpb/README.md#quick-start).

Requirements: C++17, Qt 6.5 or newer or Qt 5.15, CMake 3.21 or newer; see
[`qpb/README.md`](qpb/README.md#requirements).

## Examples

More in [`examples/`](examples): quickstart, a custom `QColor` type, an inspector, a settings dialog, a runtime
plugin configuration, a form view and a tree view sharing one search filter, an editor for QObjects with undo/redo and
JSON files, a dark and a light theme from one style sheet template
([`examples/custom_theme`](examples/custom_theme); see [style sheets](qpb/doc/guide/style-sheets.md)), and
English, Japanese and Vietnamese switched at run time ([`examples/translations`](examples/translations); see
[translations](qpb/doc/guide/translations.md)).

To build and run them all in Qt Creator, open [`examples/qtcreator/CMakeLists.txt`](examples/qtcreator/CMakeLists.txt)
(or [`qpb_examples.pro`](examples/qtcreator/qpb_examples.pro) with qmake), pick a Qt 6.5+ or Qt 5.15 kit and choose
the example in the run target selector. The qmake project only serves the examples; projects integrate qpb with CMake
(see above).

## Developing qpb

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<Qt prefix>
cmake --build build
ctest --test-dir build --output-on-failure
```

Configurations, the test layers, the repository checks and the Qt Creator projects are described in
[docs/development/building-and-testing.md](docs/development/building-and-testing.md).

## Documentation

For users of qpb (this documentation travels with the `qpb/` folder):

- [`qpb/README.md`](qpb/README.md): overview, requirements, quick start and the version history
- [`qpb/doc/integration.md`](qpb/doc/integration.md): getting the folder, CMake, options, upgrading
- [`qpb/doc/api-overview.md`](qpb/doc/api-overview.md): the public classes by header
- Guides in [`qpb/doc/guide/`](qpb/doc/guide): [values and validation](qpb/doc/guide/values-and-validation.md),
  [changes and conditions](qpb/doc/guide/changes-and-conditions.md),
  [serialization](qpb/doc/guide/serialization.md),
  [custom types and editors](qpb/doc/guide/custom-types-and-editors.md),
  [QObject source](qpb/doc/guide/qobject-source.md), [views](qpb/doc/guide/views.md),
  [style sheets](qpb/doc/guide/style-sheets.md), [translations](qpb/doc/guide/translations.md)
- [`qpb/doc/compatibility.md`](qpb/doc/compatibility.md): the 1.x promise, allowed changes, deprecation, Qt 5 differences
- [`qpb/CHANGELOG.md`](qpb/CHANGELOG.md): what each version added, changed, deprecated and fixed, with upgrade notes

For developers of qpb: the index is [docs/README.md](docs/README.md).

## Version

The current version is in [`qpb/VERSION`](qpb/VERSION). The version history, one line per release, is in the
"Version history" section of [`qpb/README.md`](qpb/README.md); the full notes of each release, including upgrade
notes, are in [`qpb/CHANGELOG.md`](qpb/CHANGELOG.md).

## License

MIT; see [`qpb/LICENSE`](qpb/LICENSE).
