# Style sheets

Theming the views and editors with Qt style sheets (since 1.6). Back to the
[documentation index](../../README.md#documentation).

Standard selectors cover the views and every editor: `qpb--PropertyTreeView`, `qpb--PropertyFormView`, `::item`
(`:hover`, `:selected`, `:alternate`), `QHeaderView::section`, and the editors by their Qt classes
(`qpb--PropertyTreeView QLineEdit` for editors in cells, `qpb--PropertyFormView QLineEdit` in the form). For what qpb
paints or builds itself:

**Tree view**: colour properties, settable with `qproperty-`. Invalid values keep the defaults; a matching `::item`
rule with a `color` or `background` takes precedence. Fonts are fixed (group and modified names stay bold).

| Property | Default |
|---|---|
| `groupBackground` (brush) | the palette's Button |
| `groupForeground` | the item text colour |
| `modifiedForeground` | the item text colour |
| `readOnlyForeground` | the palette's PlaceholderText |

The same values are available from code: `setGroupBackground()` and the other setters of `PropertyTreeView`. An invalid
colour or `Qt::NoBrush` selects the default.

**Form view and path editors**: dynamic properties on their widgets.

| Widget | Selector |
|---|---|
| a group's section, title button, body | `QWidget[qpbPart="group"]`, `QToolButton[qpbPart="groupTitle"]`, `QWidget[qpbPart="groupBody"]` |
| a property's label | `QLabel[qpbPart="label"]`, and `[qpbModified="true"]` while modified |
| read-only text of a type without editor | `QLabel[qpbPart="value"]` |
| "..." button of file and directory editors | `QToolButton[qpbPart="browse"]` |

```css
qpb--PropertyTreeView { qproperty-groupBackground: #2c3038; qproperty-modifiedForeground: #e87c00; }
QToolButton[qpbPart="groupTitle"], QToolButton[qpbPart="groupTitle"]:checked {
    background: #2c3038; color: #e87c00; border: none; border-bottom: 1px solid #e87c00;
}
QLabel[qpbPart="label"][qpbModified="true"] { color: #e87c00; }
```

## Model colours in the form view

`PropertyFormView::showModelForeground` (since 1.9, off by default) makes labels and section titles show the
`Qt::ForegroundRole` colour of the model (the name cell's, else the value cell's). A style sheet can turn it on:

```css
qpb--PropertyFormView { qproperty-showModelForeground: true; }
```

The same value is available from code: `setShowModelForeground()`. Precedence differs from the tree view, where a
matching `::item` rule wins over the colour of the model:

- The model's colour wins over a `color` of the application's style sheet for `QLabel[qpbPart="label"]` and
  `QToolButton[qpbPart="groupTitle"]`, including the `[qpbModified="true"]` rule. The form sets the colour as the
  widget's own style sheet, which an application sheet cannot override. To get another colour, return another brush
  from the model or a proxy instead of writing a rule.
- Without a colour from the model (none given, or one the model stops giving) the widget follows the style and the
  application sheet as usual, so the `[qpbModified="true"]` rule applies again.
- Modified labels and titles stay bold. The form sets no colour on a disabled label (its rule is `QLabel:enabled`), so a
  disabled label keeps the colour it has without the property: the style's disabled colour, or the application sheet's.
- With the property off, the form sets nothing, whatever the model returns.

## Notes

- The section title is a checkable tool button (checked while expanded), so cover `:checked` if your sheet styles
  checked tool buttons.
- `qproperty-` values stay when a sheet is removed; set them back to invalid values when switching to a theme without
  them.
- A `::branch` rule replaces the tree's expand arrows and then needs `image:` for `:closed` and `:open`.
- Bold labels and titles stay bold when a sheet restyles `QLabel` or `QToolButton`: the form view sets the bold weight
  again after the sheet is applied.
- Only these property names and values are part of the API, not the class names of internal widgets.
