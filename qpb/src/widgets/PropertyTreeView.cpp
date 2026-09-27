#include <qpb/Property.h>
#include <qpb/PropertyGroup.h>
#include <qpb/PropertyModel.h>
#include <qpb/widgets/PropertyDelegate.h>
#include <qpb/widgets/PropertyTreeView.h>

#include <QtCore/qset.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qmenu.h>

#include <functional>

#include "widgets_p.h"

namespace qpb {

namespace detail {

class PropertyTreeViewPrivate
{
public:
    explicit PropertyTreeViewPrivate(PropertyTreeView* view)
        : q(view)
        , delegate(new PropertyDelegate(view))
        , treeIndentation(view->indentation())
    { }

    bool isGroup(const QModelIndex& index) const
    {
        return index.data(PropertyModel::IsGroupRole).toBool();
    }

    // Applies hidden rows, spanned group rows and expansion to rows
    // first..last under parent (and, with recursive, to their descendants).
    void updateRows(const QModelIndex& parent, int first, int last, bool recursive)
    {
        QAbstractItemModel* model = q->model();
        if (!model)
            return;
        for (int row = first; row <= last; ++row) {
            const QModelIndex index = model->index(row, PropertyModel::NameColumn, parent);
            if (!index.isValid())
                continue;
            q->setRowHidden(row, parent, !index.data(PropertyModel::IsVisibleRole).toBool());
            const bool group = isGroup(index);
            q->setFirstColumnSpanned(row, parent, group);
            if (group && (mode == PropertyTreeView::Mode::List || !seen.contains(index))) {
                seen.insert(index);
                q->expand(index);
            }
            if (recursive && model->rowCount(index) > 0)
                updateRows(index, 0, model->rowCount(index) - 1, true);
        }
    }

    void updateAll()
    {
        seen.clear();
        if (QAbstractItemModel* model = q->model())
            updateRows(QModelIndex(), 0, model->rowCount() - 1, true);
    }

    void applyMode()
    {
        const bool list = mode == PropertyTreeView::Mode::List;
        q->setRootIsDecorated(!list);
        q->setItemsExpandable(!list);
        q->setExpandsOnDoubleClick(!list);
        q->setIndentation(list ? 0 : treeIndentation);
        if (list)
            q->expandAll();
    }

    // A value cell Tab can stop at.
    bool isEditableValue(const QModelIndex& nameIndex) const
    {
        const QModelIndex value = nameIndex.siblingAtColumn(PropertyModel::ValueColumn);
        return value.isValid() && value.flags().testFlag(Qt::ItemIsEditable)
            && !q->isRowHidden(nameIndex.row(), nameIndex.parent());
    }

    PropertyTreeView* q;
    PropertyDelegate* delegate;
    int treeIndentation;
    PropertyTreeView::Mode mode = PropertyTreeView::Mode::Tree;
    // Groups whose default expansion has been applied (Tree mode keeps the
    // user's later choices).
    QSet<QPersistentModelIndex> seen;
    QList<QMetaObject::Connection> connections;
};

} // namespace detail

PropertyTreeView::PropertyTreeView(QWidget* parent)
    : QTreeView(parent)
    , d(std::make_unique<detail::PropertyTreeViewPrivate>(this))
{
    setItemDelegate(d->delegate);
    setEditTriggers(QAbstractItemView::CurrentChanged | QAbstractItemView::SelectedClicked
        | QAbstractItemView::EditKeyPressed);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAlternatingRowColors(true);
    setUniformRowHeights(true);
    setAllColumnsShowFocus(true);
    header()->setStretchLastSection(true);
    header()->setSectionResizeMode(QHeaderView::Interactive);
}

PropertyTreeView::~PropertyTreeView() = default;

void PropertyTreeView::setModel(QAbstractItemModel* model)
{
    for (const QMetaObject::Connection& connection : std::as_const(d->connections))
        disconnect(connection);
    d->connections.clear();

    QTreeView::setModel(model);
    d->updateAll();
    if (!model)
        return;

    d->connections << connect(model, &QAbstractItemModel::rowsInserted, this,
        [this](const QModelIndex& parent, int first, int last) {
            d->updateRows(parent, first, last, true);
        });
    d->connections << connect(model, &QAbstractItemModel::dataChanged, this,
        [this](
            const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) {
            if (roles.isEmpty() || roles.contains(PropertyModel::IsVisibleRole)
                || roles.contains(PropertyModel::IsGroupRole)) {
                d->updateRows(topLeft.parent(), topLeft.row(), bottomRight.row(), false);
            }
        });
    d->connections << connect(
        model, &QAbstractItemModel::modelReset, this, [this] { d->updateAll(); });
    d->connections << connect(
        model, &QAbstractItemModel::layoutChanged, this, [this] { d->updateAll(); });
}

PropertyTreeView::Mode PropertyTreeView::mode() const
{
    return d->mode;
}

void PropertyTreeView::setMode(Mode mode)
{
    if (d->mode == mode)
        return;
    d->mode = mode;
    d->applyMode();
    d->updateAll();
}

int PropertyTreeView::nameColumnWidth() const
{
    return columnWidth(PropertyModel::NameColumn);
}

void PropertyTreeView::setNameColumnWidth(int width)
{
    setColumnWidth(PropertyModel::NameColumn, width);
}

PropertyDelegate* PropertyTreeView::propertyDelegate() const
{
    return d->delegate;
}

void PropertyTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    const QModelIndex index = indexAt(event->pos());
    const Property* property = detail::propertyOf(index);
    QModelIndex sourceIndex;
    PropertyModel* propertyModel = detail::propertyModelOf(index, &sourceIndex);
    if (!property || !propertyModel) {
        QTreeView::contextMenuEvent(event);
        return;
    }

    // Enabled when resetting would change something.
    const std::function<bool(const Property*)> resettable = [&](const Property* p) {
        if (const PropertyGroup* group = p->toGroup()) {
            for (const Property* child : group->children()) {
                if (resettable(child))
                    return true;
            }
            return false;
        }
        return p->isModified() && !p->isReadOnly() && p->isEnabled();
    };

    QMenu menu(this);
    QAction* reset
        = menu.addAction(property->isGroup() ? tr("Reset group") : tr("Reset to default"));
    reset->setEnabled(resettable(property));
    connect(reset, &QAction::triggered, propertyModel,
        [propertyModel, persistent = QPersistentModelIndex(sourceIndex)] {
            propertyModel->resetToDefault(persistent);
        });
    menu.exec(event->globalPos());
    event->accept();
}

QModelIndex PropertyTreeView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    if (cursorAction != MoveNext && cursorAction != MovePrevious)
        return QTreeView::moveCursor(cursorAction, modifiers);

    const bool forward = cursorAction == MoveNext;
    QModelIndex index = currentIndex().siblingAtColumn(PropertyModel::NameColumn);
    while (true) {
        index = forward ? indexBelow(index) : indexAbove(index);
        if (!index.isValid())
            return QModelIndex();
        if (d->isEditableValue(index))
            return index.siblingAtColumn(PropertyModel::ValueColumn);
    }
}

} // namespace qpb
