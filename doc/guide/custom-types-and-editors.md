# Custom types and editors

Adding property types of your own (a colour, a vector, an enumeration of your domain), choosing another editor for one
property, and writing editors. Back to the [documentation index](../../README.md#documentation).

A type has two halves. The **type handler** (`TypeRegistry`, in `qpb::core`) says how a value is converted, shown,
validated and saved. It knows nothing about widgets. The **editor handler** (`EditorFactory`, in `qpb::widgets`) says
which widget edits the value. The views never look at a type id themselves; everything type-dependent goes through these
two registries.

## Your own type

```cpp
qpb::TypeHandler color;
color.displayText = [](const QVariant& v, const qpb::Property&) { return v.value<QColor>().name(); };
color.toJson = [](const QVariant& v, const qpb::Property&) { return QJsonValue(v.value<QColor>().name()); };
qpb::TypeRegistry::global().registerType<QColor>("color", color);
root->add("color", "tint", QColor(Qt::red));
```

`TypeHandler` is configured by assigning fields; all of them are optional, and an empty field means the default
behaviour:

| Field | Purpose | When empty |
|---|---|---|
| `storageType` | The type every value is converted to before it is stored. `registerType<T>()` sets it to `T` | Values are stored as given |
| `displayText(value, property)` | The text shown for a value that is not being edited | `QVariant::toString()` |
| `normalize(value, property)` | Adjusts a converted value before validation, for example clamping | The value is kept |
| `validate(value, property)` | Accepts or rejects a normalized value, returning a `ValidationResult` | Every value is accepted |
| `toJson(value, property)` | The JSON form of a value for `qpb::serialization::toJson()` (since 1.2) | `QJsonValue::fromVariant()` |
| `fromJson(json, property)` | The value for JSON read by `qpb::serialization::fromJson()`, then set with `setValue()` (since 1.2) | `QJsonValue::toVariant()` |

Notes:

- Use a prefix in your ids (`"myapp.color"`). Ids starting with `qpb.` and the ids of the built-in types are reserved.
- `registerType()` returns `false`, changing nothing, if the id is already registered or reserved. `replaceType()`
  replaces the handler of a type that is registered, built-in types included, and returns `false` otherwise.
  `contains()`, `handler()` and `types()` look types up.
- Register types on the main thread before you create properties that use them. The registry is not thread-safe.
- A property whose type is not registered is shown read-only as plain text, with one warning in the log. It starts to
  behave normally once the type is registered. `Property::create(type, id, value)` makes such a property without
  needing the type to exist yet.
- Custom attributes (`attribute("myapp.something", value)`) are ignored by qpb and passed to your handlers through
  the property.
- With Qt 5 `TypeHandler::storageType` is an `int` type id instead of a `QMetaType`; `registerType<T>()` works the same
  in both. See [Compatibility](../compatibility.md#qt-5-and-qt-6).

## An editor for your type

An `EditorHandler` has five functions:

| Field | Purpose | Required |
|---|---|---|
| `createEditor(parent, property)` | Builds the widget as a child of `parent` | yes |
| `setEditorData(editor, value, property)` | Shows a value in the widget | yes |
| `editorData(editor, property)` | Returns the value the user entered | yes |
| `applyAttributes(editor, property)` | Applies ranges and the like; called after `createEditor` and whenever the attributes change | no |
| `paint(painter, option, value, property)` | Draws the value cell when it is not being edited (a colour swatch, say); the display text is drawn when empty | no |

```cpp
qpb::EditorHandler slider;
slider.createEditor = [](QWidget* parent, const qpb::Property&) -> QWidget* {
    return new QSlider(Qt::Horizontal, parent);
};
slider.setEditorData = [](QWidget* editor, const QVariant& v, const qpb::Property&) {
    static_cast<QSlider*>(editor)->setValue(v.toInt());
};
slider.editorData = [](QWidget* editor, const qpb::Property&) {
    return QVariant(static_cast<QSlider*>(editor)->value());
};
slider.applyAttributes = [](QWidget* editor, const qpb::Property& p) {
    static_cast<QSlider*>(editor)->setRange(p.attribute(qpb::Attr::Minimum, 0).toInt(),
                                            p.attribute(qpb::Attr::Maximum, 100).toInt());
};
qpb::EditorFactory::global().registerEditor("myapp.slider", slider);
```

`registerEditor()` returns `false`, changing nothing, if the id is already registered or a required field is empty;
`replaceEditor()` replaces a registered editor, built-in ones included. `contains()`, `handler()` and `editors()` look
editors up. Use the factory from the GUI thread.

**An editor for a type.** Register the editor under the type's id: `registerEditor("color", handler)`.

**Another editor for one property.** Register the editor under an id of its own and name it in the builder:

```cpp
root->addInt("volume", 50).range(0, 100).editor("myapp.slider");
```

This sets the attribute `Attr::EditorId`. The property keeps its type, so storage and validation still follow its type
id. The factory looks for the editor named by `EditorId` first and then for the one registered under the type id
(`handlerFor(property)`); a property for which neither exists has no editor and shows its text.

### Committing and dialogs

- The views commit an editor when it loses focus. For editors where a choice is final (a list item, a picked file,
  a colour), call `qpb::EditorFactory::notifyCommit(editor)` when the user has chosen, so the value is committed at once.
- `qpb::EditorFactory::notifyCommitKeepOpen(editor)` (since 1.9) commits at once too, but the editor stays open, for
  editors where the user keeps working after each change (a slider, a spin box step). The tree view then writes the
  value the model holds back into the editor: the model may have clamped or normalised the input, or refused it (the
  message shows as a tool tip). The form view commits as for `notifyCommit()`. Call it from the editor's own slot, for
  example `connect(slider, &QSlider::valueChanged, this, [this] { qpb::EditorFactory::notifyCommitKeepOpen(this); })`.
- While an editor shows a dialog, keep a `qpb::EditorDialogScope` alive. The dialog takes the focus; without the scope
  the view would commit and close the editor half-way:

```cpp
void ColorButton::pickColor()
{
    qpb::EditorDialogScope scope(this);
    const QColor color = QColorDialog::getColor(m_color, this);
    // ...
}
```

## Built-in editors

| Type | Widget | Notes |
|---|---|---|
| Bool | check box | In the tree and list views the check indicator of the cell is used and there is no editor widget; Space or a click toggles it. The form view uses a `QCheckBox` |
| Int | `QSpinBox` | Applies minimum, maximum, step, prefix and suffix; commits when editing finishes, not on every key. With `Attr::CommitOnStep` (since 1.9) every step (arrow buttons, arrow keys, wheel, Page Up / Down) is committed at once and the editor stays open; typed text still waits for Enter or focus out |
| Int64 | a spin box for 64-bit values | Like Int; steps stop at the limits |
| Double | `QDoubleSpinBox` | Like Int, with decimals; a cell that is not edited shows at most that many fractional digits, without trailing zeros. With `Attr::RoundToDecimals` false the stored value keeps its digits and an editor the user did not change writes it back as it is |
| String | `QLineEdit`; `QPlainTextEdit` with `multiline` | The single-line editor limits typing live to the max length and the regular expression. The multi-line editor checks both when the value is committed; it is at least four lines tall, Tab moves on, Enter inserts a line break and Ctrl+Enter commits (see [Views](views.md#keyboard)) |
| Enum | non-editable `QComboBox` | Commits when an item is chosen |
| FilePath | line edit with a "..." button | The button opens an open or save dialog according to `DialogMode`; choosing a file commits |
| DirPath | line edit with a "..." button | The button opens a directory dialog |

Long values in a cell that is not edited are elided in the middle. The tool tip of the value cell is the property's tool
tip when it has one, and otherwise the display text, so a long path shows in full.
