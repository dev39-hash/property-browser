#ifndef QPB_WIDGETS_PROPERTYTREEVIEW_H
#define QPB_WIDGETS_PROPERTYTREEVIEW_H

#include <qpb/qpbglobal.h>

#include <QtWidgets/qtreeview.h>

#include <memory>

namespace qpb {

class PropertyDelegate;

namespace detail {
class PropertyTreeViewPrivate;
}

// Two-column view (name | value) for a PropertyModel or a proxy of one
// (docs/SPEC.md §5.5).
//
// Tree mode shows collapsible groups; List mode shows the same model flat,
// with groups as section headers. Switching modes keeps the model, the values
// and the current selection. Hidden properties are hidden rows; modified
// properties are shown in bold; the context menu offers "Reset to default".
class QPB_WIDGETS_EXPORT PropertyTreeView : public QTreeView
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)

public:
    enum class Mode {
        Tree,
        List,
    };
    Q_ENUM(Mode)

    explicit PropertyTreeView(QWidget* parent = nullptr);
    ~PropertyTreeView() override;

    // Accepts a PropertyModel or any proxy model whose source is one.
    void setModel(QAbstractItemModel* model) override;

    Mode mode() const;
    void setMode(Mode mode);

    // Width of the name column in pixels.
    int nameColumnWidth() const;
    void setNameColumnWidth(int width);

    // The delegate installed by this view.
    PropertyDelegate* propertyDelegate() const;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    // MoveNext / MovePrevious (Tab / Shift+Tab while editing) skip to the next
    // editable value, passing over groups, read-only and check box rows.
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;

private:
    std::unique_ptr<detail::PropertyTreeViewPrivate> d;
};

} // namespace qpb

#endif // QPB_WIDGETS_PROPERTYTREEVIEW_H
