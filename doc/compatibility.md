# Compatibility

What the 1.x versions promise, what may and may not change in a minor or patch release, how deprecation works, how Qt 5
and Qt 6 differ, and how to check the version. Back to the [documentation index](../README.md#documentation).

## The 1.x promise

Within a major version (1.x), code that builds against 1.a builds and behaves the same against any later 1.b, by only
replacing the `qpb/` folder. Your code and your CMake do not change; this holds for each Qt major (see
[Qt 5 and Qt 6](#qt-5-and-qt-6)).

- The promise is **source** compatibility, not binary compatibility. You always rebuild from the folder, so the
  headers are free to keep internals out of sight, and a shared library of one version is not a drop-in replacement for
  another (see [Integration](integration.md#static-or-shared)).
- The one exception is a bug fix: where the old behaviour contradicted the documentation, the behaviour is corrected and
  the entry in [CHANGELOG.md](../CHANGELOG.md) says so.
- The minimum C++, Qt and CMake versions only rise in a new major version.

## What may and may not change within 1.x

| May change in a minor or patch release | Does not change before 2.0 |
|---|---|
| New classes, free functions and headers | Removing or renaming anything public |
| New non-virtual member functions and non-ambiguous overloads | Changing parameter or return types of existing functions, adding parameters (even defaulted ones) |
| New enum values, appended at the end; new model roles, appended | The numeric value of an existing enum value or model role |
| New fields at the end of `TypeHandler`, `EditorHandler`, `EnumOption`, `ValidationResult`; an empty field keeps the old behaviour | The meaning of existing fields; positional initialization is not supported (see below) |
| New virtual functions whose default implementation keeps the old behaviour | A new pure virtual function in a class you may subclass |
| New attributes, type ids and signals | The signature of an existing signal or slot |
| New CMake options whose defaults keep the old behaviour | Names of targets, options and include paths; the C++, Qt and CMake minimums |
| Deprecating API that has a replacement | Removing deprecated API |
| New translatable texts and translations | The context or source text of an existing translatable text |
| Fixing a bug where the behaviour contradicted the documentation (called out in the changelog) | Documented behaviour and defaults |

New behaviour that would change what an application does after an upgrade, such as a keyboard behaviour, is opt-in
(for example `setTabStopsOnCheckBoxes()`, `setLiveReadOnlyProperties()`).

## What you should do in return

- Assign the fields of `TypeHandler`, `EditorHandler`, `EnumOption` and `ValidationResult` by name. Never initialize them
  positionally with `{a, b, c}`: new fields are added at the end, which would change the meaning. `EnumOption` has the
  two-argument form shown in the examples; create `ValidationResult` with `valid()` and `error()`.
- Call the functions of `qpb::serialization` qualified. Free functions added after 1.0 live in nested namespaces, so
  they can never clash with your own unqualified calls; qualification is how you reach them.
- Check `ValidationResult::ok`, never the wording of a message. The wording of built-in messages is translated and may
  be improved.
- Do not subclass `Property` or `PropertyGroup`; they are not meant to be extended. Add behaviour through validators,
  conditions, type handlers and editor handlers instead.
- Start your own model roles at `PropertyModel::UserRole`. The range from `Qt::UserRole + 1` to `Qt::UserRole + 99` is
  reserved for qpb.
- Use type ids with your own prefix (`"myapp.color"`); ids that start with `qpb.` are reserved.
- Do not rely on anything internal: the namespace `qpb::detail`, the `src/` folder, and the class names of qpb's
  internal widgets (use the style sheet properties described in [Style sheets](guide/style-sheets.md) instead).

## Deprecation

Superseded API is marked `QPB_DEPRECATED_X("use X instead")` (which maps to `[[deprecated]]`, so the compiler warns
where you still use it) and is kept until the end of 1.x; it is removed only in 2.0. Every deprecated declaration is
wrapped in `#if !defined(QPB_DISABLE_DEPRECATED)`. To find the remaining uses before an upgrade, define
`QPB_DISABLE_DEPRECATED` for your build: each use then fails to compile.

```cmake
target_compile_definitions(my_app PRIVATE QPB_DISABLE_DEPRECATED)
```

Each version in [CHANGELOG.md](../CHANGELOG.md) lists what was *Added*, *Changed*, *Deprecated* and *Fixed*, and has
*Upgrade notes*, usually "nothing to do".

## Translatable texts

The contexts and source texts of qpb's translatable texts are stable within 1.x, so a translation you made for another
language, or a different wording you installed, keeps working. The list is in
[Translations](guide/translations.md).

## Qt 5 and Qt 6

Since 1.7 qpb builds with Qt 5.15 and with Qt 6.5 or newer from the same sources, with the same behaviour. The
compatibility promise holds for each Qt major: an application built with Qt 5 keeps building with Qt 5 when the folder
is updated, and the same for Qt 6. Moving an application from Qt 5 to Qt 6 is Qt's own migration and may need the two
changes below.

Where Qt 5 has no equivalent of a Qt 6 type, the public headers differ:

- `Types::*` and `Attr::*` are `QLatin1String` constants with Qt 5 and `QLatin1StringView` constants with Qt 6. Both
  convert to `QString`, so code that uses them as type ids or attribute keys is the same.
- `TypeHandler::storageType` is an `int` type id with Qt 5, `QMetaType::UnknownType` by default, and a `QMetaType` with
  Qt 6. `registerType<T>(id, handler)` sets it in both. With Qt 5 you can also write
  `handler.storageType = qMetaTypeId<T>();`.

If you want code that compiles with both, use `registerType<T>()` and use the constants only where a `QString` is
accepted. Setting up CMake for Qt 5 and its compiler limits are in [Integration](integration.md#qt-5).

## Checking the version

`qpbglobal.h` provides the version of the headers you compile against, generated from the `VERSION` file:

| Name | Meaning |
|---|---|
| `QPB_VERSION_MAJOR`, `QPB_VERSION_MINOR`, `QPB_VERSION_PATCH` | The three numbers |
| `QPB_VERSION_STR` | The full string, including any pre-release suffix such as `"1.0.0-rc1"` |
| `QPB_VERSION` | The version packed into one integer, comparable in `#if` |
| `QPB_VERSION_CHECK(major, minor, patch)` | Packs a version the same way |
| `qpb::version()` | The version of the library linked at run time, for example `"1.8.0"` |

```cpp
#if QPB_VERSION >= QPB_VERSION_CHECK(1, 4, 0)
    model.onValueChanged("Transform/fov", this, [this](const QVariant& v) { apply(v); });
#else
    connect(&model, &qpb::PropertyModel::valueChanged, this, &Window::onValueChanged);
#endif
```

Compare `qpb::version()` with `QPB_VERSION_STR` to detect a header and library mismatch in a shared build.
