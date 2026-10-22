# qpb - property browser for Qt 6 and Qt 5.15 Widgets

qpb shows and edits a tree of typed properties ("name | value") in Qt Widgets applications: object
inspectors, settings dialogs, plugin and device configuration. You describe the properties in code; qpb provides the
model, the views and the editors, validation, reset to default, change notification, saving and loading.

This folder is the complete library. Copy it into your project, add two lines of CMake, and build. Updating means
replacing the folder: within a major version (1.x) your code and CMake never need to change (see
[Compatibility](#compatibility)).

Contents: [Requirements](#requirements) - [Integration](#integration) - [Features](#features) -
[Quick start](#quick-start) - [Documentation](#documentation) - [Compatibility](#compatibility) -
[Version history](#version-history)

## Requirements

- C++17 or newer, Qt 6.5 or newer or (since 1.7) Qt 5.15 (Core, Widgets), CMake 3.21 or newer.
- Windows (MSVC) and Linux (GCC, Clang) with a Qt that supports them. Windows is verified for every release, Linux is
  supported; see [Platforms](doc/integration.md#platforms) and [Qt 5](doc/integration.md#qt-5) for the details and the
  compilers Qt 5.15 accepts.

## Integration

Put this folder in your project, for example as `components/qpb/`, and add:

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
add_subdirectory(components/qpb)
target_link_libraries(my_app PRIVATE qpb::widgets)   # or qpb::core without widgets
```

```cpp
#include <qpb/qpb.h>       // everything; <qpb/qpbcore.h> for qpb::core only
```

The folder changes none of your project's CMake settings. Getting the folder (release zip, git subtree, git submodule),
the CMake options, static and shared builds, Qt 5 and upgrading are in [doc/integration.md](doc/integration.md).

## Features

- **Property tree**: groups and properties with an id, a path (`"Transform/x"`), a display name, a tool tip, a value
  and a default value. Built in code with fluent builders.
- **Types**: bool, int, 64-bit int, double, string (single or multiple lines), enum, file path, directory path, and
  your own types (e.g. `QColor`) through a type registry.
- **Attributes**: ranges, steps, decimals, prefix and suffix, maximum length, placeholder, regular expression, file
  filter, dialog mode, default directory, "must exist", enum options.
- **Validation**: conversion, normalization (clamping, rounding) and type checks, plus a validator per property.
  Rejected values keep the old value and report a message.
- **State**: read-only, disabled, hidden and live (a value maintained by the application), inherited by children.
  Conditions enable or show a property depending on another one's value.
- **Model**: a two-column `QAbstractItemModel` for any Qt view; change signals per value and per batch, a separate
  signal for edits of the user, callbacks per path; properties can be added and removed while the model is shown; reset
  of one property, a group or everything; normalizing values and making them the defaults.
- **Views**: `PropertyTreeView` (collapsible tree or flat list with section headers) and `PropertyFormView` (a form
  with persistent editors and collapsible sections). Bold names for modified values, "Reset to default" context menu,
  full keyboard use (Enter, Esc, Tab), validation messages as tool tips. The form view can show the model's
  `Qt::ForegroundRole` colour on labels and section titles (`showModelForeground`, off by default).
- **Editors**: one per type, replaceable per type or per property; custom editors may open dialogs.
- **Search**: a filter proxy model by display name for both views.
- **QObject properties**: show the `Q_PROPERTY`s of your objects and keep both sides in sync.
- **Saving**: values to and from JSON and `QSettings`.
- **Theming**: Qt style sheets, with hooks for what qpb paints itself.
- **Translations**: qpb's own texts in English, Japanese and Vietnamese, switchable at run time.

## Quick start

```cpp
#include <qpb/qpb.h>
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto root = qpb::PropertyGroup::create("Camera");
    root->addString("name", "Main camera");
    root->addBool("visible", true);
    auto& transform = root->addGroup("Transform");
    transform.addDouble("x", 0.0).range(-100.0, 100.0).suffix(" m");
    transform.addInt("fov", 60).range(10, 170).displayName("Field of view");
    root->addEnum("projection", {"Perspective", "Orthographic"}, 0);
    root->addFilePath("lut", {}).filter("LUT files (*.cube)");

    qpb::PropertyModel model(std::move(root));
    model.onValueChanged("Transform/fov", &app, [](const QVariant& v) { qDebug() << "fov" << v; });

    qpb::PropertyTreeView view;   // or qpb::PropertyFormView
    view.setModel(&model);
    view.show();
    return app.exec();
}
```

## Documentation

| Page | What it covers |
|---|---|
| [Integration](doc/integration.md) | Getting the folder, CMake, options, static and shared builds, Qt 5, platforms, upgrading |
| [API overview](doc/api-overview.md) | The public classes by header |
| [Values and validation](doc/guide/values-and-validation.md) | The value pipeline, built-in checks, validators, defaults and reset |
| [Changes and conditions](doc/guide/changes-and-conditions.md) | Reacting to changes, batches, dependencies between properties, live values |
| [Saving and loading](doc/guide/serialization.md) | JSON and `QSettings` |
| [Custom types and editors](doc/guide/custom-types-and-editors.md) | Your own property types, replacing or writing editors |
| [QObject properties](doc/guide/qobject-source.md) | Showing `Q_PROPERTY`s of your objects with `Q_CLASSINFO` metadata |
| [Views](doc/guide/views.md) | Tree view, form view, filter proxy, keyboard, threads |
| [Style sheets](doc/guide/style-sheets.md) | Theming the views and editors |
| [Translations](doc/guide/translations.md) | qpb's own languages, your texts, switching at run time |
| [Compatibility](doc/compatibility.md) | The 1.x promise, deprecation, Qt 5 and Qt 6 differences, version checks |

## Compatibility

Within 1.x, updating the folder never requires changes to your code or CMake (with the same Qt major): nothing public is
removed or changed, only added. What may and may not change, and what you should do in return, is in
[doc/compatibility.md](doc/compatibility.md).

## Version history

Each version has a section in `CHANGELOG.md` (next to this file) with what was added, changed, deprecated and fixed, and
its upgrade notes.

- **1.8.0** - translations of qpb's own texts (English, Japanese, Vietnamese).
- **1.7.0** - Qt 5.15 as a second configuration.
- **1.6.1** - documentation only: this README in the component folder.
- **1.6.0** - style sheet hooks for the tree and form views.
- **1.5.0** - `resetAllToDefault()`, check boxes in the Tab chain.
- **1.4.0** - `onValueChanged()` callbacks, `enabledWhen()` / `visibleWhen()` conditions.
- **1.3.0** - live values, group titles from `QObjectPropertySource`.
- **1.2.0** - `QObjectPropertySource`, JSON and `QSettings` serialization, `Int64`.
- **1.1.0** - `PropertyFormView`, `PropertyFilterProxyModel`, multi-line strings.
- **1.0.0** - first stable release: model, tree view, editors for the basic types.
