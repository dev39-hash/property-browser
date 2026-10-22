# Views

The tree view, the form view, the filter proxy for a search box, the delegate for views of your own, the keyboard, and
threads. Back to the [documentation index](../../README.md#documentation).

Both views show a `PropertyModel`, or a proxy model whose source is one. Neither owns the model. Edits go through the
model (`setData`), so validation and signals are the same whichever view the user works in. Both look the same to your
code: you build the tree, create a model, and call `setModel()`.

## Tree view

`PropertyTreeView` is a `QTreeView` with two columns, name and value.

- **Modes.** `Mode::Tree` (the default) shows collapsible groups, expanded at first. `Mode::List` shows the same model
  flat, with groups as section headers that do not collapse. `setMode()` switches at any time and keeps the model, the
  values and the current selection.
- **Looks.** Group rows are bold on the palette's Button colour; the names of modified properties are bold; read-only
  values are dimmed; hidden properties are hidden rows; rows alternate in colour. Long values are elided in the middle.
  Colours can be set from style sheets; see [Style sheets](style-sheets.md).
- **Name column.** It fits its contents (up to 60% of the view) until you set a width with `setNameColumnWidth()` or the
  user drags the header.
- **Context menu.** "Reset to default" on a property (disabled when it is not modified, is read-only or is disabled)
  and "Reset group" on a group (enabled when at least one property in it can be reset).
- **Editing.** The editor opens when the current item changes, when a selected item is clicked, or with the platform's
  edit key. Values are committed through the model; if the model rejects one, the editor closes, the old value stays and
  the message shows as a tool tip at the cell.

## Form view

`PropertyFormView` is a `QScrollArea` (since 1.1) that shows a form: a label and an editor for every property, and
every group as a collapsible section with a title button and an indented body.

- Editors stay open all the time; a type without an editor is shown as selectable read-only text. `editor(path)` returns
  the widget of a property (the text label for a type without editor), or `nullptr` for groups, unknown paths and
  properties filtered out by a proxy.
- Expanded or collapsed state is kept by path, also when the form is rebuilt: `isExpanded(groupPath)` (true by default)
  and `setExpanded()`. The state can be set for a group that does not exist yet.
- The form follows the model: values, display names, tool tips, visibility, read-only and enabled states, attributes,
  and added or removed properties. Labels of modified properties are bold. Hidden properties hide their label and
  editor, or the whole section for a group; a disabled property has a disabled label and editor, a read-only one a
  disabled editor and a normal label. An editor is rewritten only when the model's value changes (or its attributes
  are applied again), so text being typed survives other updates such as a new tool tip or colour.
- `showModelForeground` (a `Q_PROPERTY`, since 1.9, off by default): labels and section titles show the model's
  `Qt::ForegroundRole` colour, the name cell's or else the value cell's, also while a section is collapsed. A `QColor`
  or a `QBrush` with a single colour counts; gradient and texture brushes, `Qt::NoBrush` and other values mean no
  colour. The colour follows `dataChanged()` without a rebuild, wins over the application's style sheet, and leaves the
  bold weight of modified labels and titles alone; a disabled label gets no model colour and keeps the colour it has
  without the property (the style's disabled colour, or the application sheet's). With the property off
  the model's colours are ignored. See [Style sheets](style-sheets.md#model-colours-in-the-form-view).
- Structural changes in the model (rows added, removed or moved, a model reset, a layout change) rebuild the form once,
  when control returns to the event loop, so a filter that changes many rows at once costs one rebuild. The focused
  editor is committed first and gets the focus back afterwards. `editor(path)`, `setExpanded()`, the constructor and
  `setModel()` do not wait: they bring the form up to date at once.
- The context menu of a label offers "Reset to default"; the one of a section title offers "Reset group". They are
  enabled under the same conditions as in the tree view.

## Search: the filter proxy

`PropertyFilterProxyModel` (since 1.1) filters by display name, ignoring case. Use it between the model and either view:

```cpp
qpb::PropertyFilterProxyModel proxy;
proxy.setSourceModel(&model);
QObject::connect(searchEdit, &QLineEdit::textChanged, &proxy, &QSortFilterProxyModel::setFilterFixedString);
view.setModel(&proxy);
```

A group is shown when one of its descendants matches, and with all its content when its own name matches. Hidden
properties never match. Set the text with the usual `QSortFilterProxyModel` functions such as `setFilterFixedString()`.
When you look up properties after filtering, map indexes back to the source model with `mapToSource()`.

## Your own view

`PropertyTreeView` installs a `PropertyDelegate`. To get the same editing in another `QAbstractItemView` that shows a
`PropertyModel` (or a proxy of one), set the delegate on it:

```cpp
anyView->setItemDelegate(new qpb::PropertyDelegate(anyView));
```

The delegate creates editors with the `EditorFactory`, paints values (check boxes, modified, read-only) and handles the
keys below. Rejected values keep the old value and show the message as a tool tip.

## Keyboard

| Key | Tree view (and delegate) | Form view |
|---|---|---|
| Enter | Commits and closes the editor. In a multi-line string editor, Enter inserts a line break and Ctrl+Enter commits | Commits. In a multi-line string editor, Enter inserts a line break and Ctrl+Enter commits |
| Esc | Cancels the edit | Puts the model's value back into the editor |
| Tab / Shift+Tab | Commits and opens the editor of the next / previous editable value, skipping groups, read-only and hidden rows, and check boxes | The normal focus chain of the form |
| Space | Toggles a check box | Toggles a check box |
| Focus out | Commits (not while an editor shows a dialog or a popup is active) | Commits (not while an editor shows a dialog or a popup is active) |

In the tree view check boxes are not in the Tab chain by default; the arrow keys reach them. Call
`setTabStopsOnCheckBoxes(true)` (since 1.5) to let Tab and Shift+Tab stop on check boxes the user can change: no editor
opens there, Space toggles the value, and the next Tab goes on. Leaving the view at either end works as before. The
form view needs no such option: its check boxes are ordinary widgets in the focus chain.

The form view commits an editor on focus out, on Enter (Ctrl+Enter in a multi-line editor), and at once for check
boxes, combo boxes, path editors and custom editors that call `EditorFactory::notifyCommit()`. A value that did not
change is not written.

## Threads

qpb is a GUI-thread library. Use the model, the views and the registries (`TypeRegistry`, `EditorFactory`) from the GUI
thread only.
