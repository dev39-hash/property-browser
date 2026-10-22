# Translations

qpb's own texts in English, Japanese and Vietnamese, your own texts, and changing the language at run time (since 1.8).
Back to the [documentation index](../../README.md#documentation).

Since 1.8 qpb ships translations of its own texts for English, Japanese and Vietnamese. They are embedded in
`qpb::core`: there is nothing to deploy, and building needs no Qt Linguist tools. Load and install them like Qt's own:

```cpp
#include <qpb/Translations.h>

QTranslator qpbTranslator;                          // keep it as long as it is installed
if (qpb::loadTranslation(qpbTranslator, QLocale())) // the locale's UI languages: "ja-JP" finds Japanese
    app.installTranslator(&qpbTranslator);
```

`loadTranslation()` returns false when qpb has no translation for the locale; qpb's texts then stay English. Without a
qpb translation installed, every text is the English one of earlier versions. Your application owns the translator, so
you control the order of translators and when it is removed; qpb keeps none of its own.

qpb translates what it shows by itself: the column titles, the context menus, the tool tips of the file and directory
editors, and the messages of the built-in validation. Everything else is text you give qpb (display names, tool tips,
enum labels, placeholders, messages of your validators): pass it translated, e.g. with `tr()`. Compare
`ValidationResult::ok`, never the wording of a message. Texts that `QObjectPropertySource` takes from class info
(`displayName=`, `toolTip=`) and from `Q_ENUM` keys are used as written; to translate them, set them again with
`setDisplayName()` / `setToolTip()` / the `options` attribute after `addObject()`.

**Changing the language at run time**: install and remove translators as usual. qpb updates the column titles and the
editors it keeps open; context menus are built when they open; messages already shown keep their language. Set your
own texts again when your window gets `QEvent::LanguageChange`:

```cpp
void Window::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        model.find("general/name")->setDisplayName(tr("Name"));
        // ... tool tips, group titles, enum labels (setAttribute(qpb::Attr::Options, ...))
    }
    QWidget::changeEvent(event);
}
```

Texts that come from Qt itself inside qpb's editors (the context menu of a line edit or spin box, a non-native file
dialog) are translated by Qt's `qtbase_<lang>.qm`, which your application loads. Qt has Japanese, but not Vietnamese.
Messages written with `qWarning()` are for developers and stay English.

**Other languages, other wording**: the contexts and source texts below are stable within 1.x. Translate them in a
`.ts` file of your own (`lupdate components/qpb/src -ts myapp_qpb_de.ts` lists them) and install that translator after
qpb's: the translator installed last wins. New texts may be added in later 1.x versions; existing contexts and source
texts are not changed.

| Context | Source texts |
|---|---|
| `qpb::PropertyModel` | `Property`, `Value` |
| `qpb::PropertyTreeView`, `qpb::PropertyFormView` | `Reset to default`, `Reset group` |
| `qpb::detail::PathEdit` | `Choose a file`, `Choose a directory` |
| `qpb::TypeRegistry` | `The value is not a number`, `At most %n characters are allowed` (plural), `The value does not match the pattern %1`, `"%1" is not one of the options`, `The file %1 does not exist`, `The directory %1 does not exist` |
| `qpb::Property` | `Cannot convert the value to %1` |

qpb's own translations are in `translations/qpb_<lang>.ts` (the embedded `.qm` files are generated from them).
