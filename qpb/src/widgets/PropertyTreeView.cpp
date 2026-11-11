#include <qpb/Property.h>
#include <qpb/PropertyModel.h>
#include <qpb/widgets/PropertyDelegate.h>
#include <qpb/widgets/PropertyTreeView.h>

#include <QtCore/qset.h>
#include <QtCore/qtimer.h>
#include <QtGui/qevent.h>
#include <QtGui/qfontmetrics.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qmenu.h>

#include <limits>

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
        fitNameColumn();
    }

    // The fonts a bold name is measured with, and how many rows are left to
    // look at.
    struct BoldMeasure
    {
        explicit BoldMeasure(const QFont& font, int rows)
            : regular(font)
            , bold(boldFont(font))
            , budget(rows)
        { }
        static QFont boldFont(QFont font)
        {
            font.setBold(true);
            return font;
        }
        QFontMetrics regular;
        QFontMetrics bold;
        int budget;
    };

    // How many rows of names are measured: the rows the view measures for its
    // contents (resizeContentsPrecision(): -1 all of them; 0 the rows that fit
    // into the viewport).
    int measuredRows() const
    {
        const int precision = q->header()->resizeContentsPrecision();
        if (precision < 0)
            return std::numeric_limits<int>::max();
        if (precision == 0)
            return q->viewport()->height() / qMax(1, q->fontMetrics().height()) + 1;
        return precision;
    }

    // How much wider the bold name of a modified property is than the regular
    // text the style measures, at most over the rows shown below parent.
    int boldExtra(const QModelIndex& parent, BoldMeasure& measure) const
    {
        const QAbstractItemModel* model = q->model();
        int extra = 0;
        for (int row = 0; row < model->rowCount(parent) && measure.budget > 0; ++row) {
            if (q->isRowHidden(row, parent))
                continue;
            --measure.budget;
            const QModelIndex index = model->index(row, PropertyModel::NameColumn, parent);
            if (isGroup(index)) {
                if (q->isExpanded(index))
                    extra = qMax(extra, boldExtra(index, measure));
            } else if (index.data(PropertyModel::IsModifiedRole).toBool()) {
                const QString text = index.data(Qt::DisplayRole).toString();
                extra = qMax(extra,
                    measure.bold.horizontalAdvance(text) - measure.regular.horizontalAdvance(text));
            }
        }
        return extra;
    }

    // What the measured width depends on besides the names: settings that
    // change without an event or a model signal. A resize reuses the measured
    // width only while they are the same.
    struct MeasureKey
    {
        int indentation = -1;
        bool rootDecorated = false;
        int precision = -1;
        int viewportHeight = -1; // only with precision 0: the measured window
        QFont font;
        bool operator==(const MeasureKey& other) const
        {
            return indentation == other.indentation && rootDecorated == other.rootDecorated
                && precision == other.precision && viewportHeight == other.viewportHeight
                && font == other.font;
        }
        bool operator!=(const MeasureKey& other) const { return !(*this == other); }
    };

    MeasureKey currentKey() const
    {
        MeasureKey key;
        key.indentation = q->indentation();
        key.rootDecorated = q->rootIsDecorated();
        key.precision = q->header()->resizeContentsPrecision();
        key.viewportHeight = key.precision == 0 ? q->viewport()->height() : 0;
        key.font = q->font();
        return key;
    }

    // Width the name cells need: the contents the style measures plus the
    // bold allowance. Measured again after a trigger that changes the names,
    // their fonts or the rows shown (measureDirty) or when a setting it
    // depends on changed (measuredKey); a resize otherwise reuses it.
    int nameWidthNeeded()
    {
        const MeasureKey key = currentKey();
        if (measureDirty || neededWidth < 0 || key != measuredKey) {
            q->resizeColumnToContents(PropertyModel::NameColumn);
            BoldMeasure measure(q->font(), measuredRows());
            neededWidth = qMax(q->columnWidth(PropertyModel::NameColumn)
                    + boldExtra(QModelIndex(), measure),
                q->header()->minimumSectionSize());
            measuredKey = key;
            measureDirty = false;
        }
        return neededWidth;
    }

    // Turns the header's stretching of the last section back on if the fit
    // turned it off (an application's own setting is never overridden).
    void restoreStretch()
    {
        if (stretchOffByFit)
            q->header()->setStretchLastSection(true);
        stretchOffByFit = false;
    }

    // The header applies stretchLastSection() again.
    void relayoutStretch()
    {
        QHeaderView* header = q->header();
        if (!stretchRelayoutPending || !header->stretchLastSection())
            return;
        stretchRelayoutPending = false;
        const bool wasResizing = resizing;
        resizing = true;
        header->setStretchLastSection(false);
        header->setStretchLastSection(true);
        resizing = wasResizing;
    }

    // The names, their fonts or the rows shown changed: measure them again,
    // then fit.
    void fitNameColumn()
    {
        measureDirty = true;
        applyNameColumnFit();
    }

    // Until the width is set explicitly (setNameColumnWidth() or by dragging
    // the header), makes the name column as wide as the longest name shown
    // (indentation, decoration and bold included), so that no name is elided.
    // The value column keeps at least the header's default section size: where
    // the view is too narrow for both, it keeps that size and the view scrolls
    // horizontally; otherwise it takes the rest of the viewport. A header
    // whose last section the application does not stretch is left as it is.
    void applyNameColumnFit()
    {
        if (!q->model())
            return;
        if (nameWidthFixed) {
            // A width set by the user or the application is kept as it is.
            restoreStretch();
            return;
        }
        QHeaderView* header = q->header();
        // The application stretches the last section itself again (or the
        // fit never had it on): it is not the fit's to restore.
        if (header->stretchLastSection())
            stretchOffByFit = false;
        const bool wasResizing = resizing;
        resizing = true;
        const int width = nameWidthNeeded();
        q->setColumnWidth(PropertyModel::NameColumn, width);

        // Corner case: the fit turned stretching off and the application
        // turns it off too (it reads false either way): the fit takes it as
        // its own and turns it on again once the view is wide.
        if (header->stretchLastSection() || stretchOffByFit) {
            const int valueMinimum = header->defaultSectionSize();
            if (q->viewport()->width() - width < valueMinimum) {
                if (header->stretchLastSection()) {
                    header->setStretchLastSection(false);
                    stretchOffByFit = true;
                }
                q->setColumnWidth(PropertyModel::ValueColumn, valueMinimum);
            } else {
                restoreStretch();
            }
        }
        lastViewportWidth = q->viewport()->width();
        lastViewportHeight = q->viewport()->height();
        resizing = wasResizing;
    }

    // Fits again once the pending events (a style sheet being applied, rows
    // added or removed, groups expanded) are handled: once per event loop
    // pass, with the final font and metrics.
    void fitNameColumnLater()
    {
        measureDirty = true;
        if (fitPending)
            return;
        fitPending = true;
        QTimer::singleShot(0, q, [this] {
            fitPending = false;
            fitNameColumn();
        });
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
    bool isTabStop(const QModelIndex& nameIndex) const
    {
        const QModelIndex value = nameIndex.siblingAtColumn(PropertyModel::ValueColumn);
        if (!value.isValid() || q->isRowHidden(nameIndex.row(), nameIndex.parent()))
            return false;
        return value.flags().testFlag(Qt::ItemIsEditable) || isCheckBoxStop(nameIndex);
    }

    // A check box the user can change, when check boxes are Tab stops.
    bool isCheckBoxStop(const QModelIndex& nameIndex) const
    {
        const QModelIndex value = nameIndex.siblingAtColumn(PropertyModel::ValueColumn);
        const Qt::ItemFlags flags = value.flags();
        return tabStopsOnCheckBoxes && value.isValid() && flags.testFlag(Qt::ItemIsUserCheckable)
            && flags.testFlag(Qt::ItemIsEnabled)
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
    bool nameWidthFixed = false; // set explicitly: stop fitting it to the contents
    bool resizing = false; // fitNameColumn() is resizing the section
    bool fitPending = false; // a fitNameColumnLater() is queued
    bool stretchOffByFit = false; // the fit turned off stretchLastSection (to scroll)
    bool stretchRelayoutPending = false; // the user's drag restored stretching: lay out after it
    bool measureDirty = true; // the names must be measured again
    int neededWidth = -1; // the width measured last (nameWidthNeeded())
    MeasureKey measuredKey; // the settings it was measured with
    int lastViewportWidth = -1; // width the name column was last fitted for
    int lastViewportHeight = -1; // height it was last fitted for
    QObject* watcher = nullptr; // child of the view, deleted before this
    bool tabStopsOnCheckBoxes = false;
    // Colours for style sheets (1.6); invalid / NoBrush: the default.
    QBrush groupBackground;
    QColor groupForeground;
    QColor modifiedForeground;
    QColor readOnlyForeground;
};

// Watches the view and its viewport for what changes the room or the metrics
// the name column is fitted to, without adding virtual members to the view.
class NameColumnWatcher : public QObject
{
public:
    NameColumnWatcher(PropertyTreeView* view, PropertyTreeViewPrivate* priv)
        : QObject(view)
        , q(view)
        , d(priv)
    { }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == q->viewport()) {
            // The scroll bars or the stretched header resize the viewport
            // while a fit is running: that must not start another. A change
            // of the height only (a scroll bar appearing) needs no fit, unless
            // the measured rows depend on the height.
            if (event->type() == QEvent::Resize && !d->resizing
                && (q->viewport()->width() != d->lastViewportWidth
                    || (q->header()->resizeContentsPrecision() == 0
                        && q->viewport()->height() != d->lastViewportHeight))) {
                d->applyNameColumnFit();
            }
        } else if (watched == q->header()->viewport()) {
            if (event->type() == QEvent::MouseButtonRelease && d->stretchRelayoutPending)
                QTimer::singleShot(0, q, [this] { d->relayoutStretch(); });
        } else if (watched == q) {
            switch (event->type()) {
            case QEvent::Show:
                d->fitNameColumn();
                break;
            case QEvent::FontChange:
            case QEvent::StyleChange:
            case QEvent::PolishRequest:
            case QEvent::Polish:
                d->fitNameColumnLater();
                break;
            default:
                break;
            }
        }
        return false;
    }

private:
    PropertyTreeView* q;
    PropertyTreeViewPrivate* d;
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
    d->watcher = new detail::NameColumnWatcher(this, d.get());
    installEventFilter(d->watcher);
    viewport()->installEventFilter(d->watcher);
    header()->viewport()->installEventFilter(d->watcher);
    connect(header(), &QHeaderView::sectionResized, this, [this](int section) {
        if (section == PropertyModel::NameColumn && !d->resizing) {
            // The user chose a width: the value column stretches over the rest again.
            d->nameWidthFixed = true;
            const bool turnedOff = d->stretchOffByFit;
            d->restoreStretch();
            // While a section is being dragged the header only remembers the
            // setting: lay the sections out once the mouse button is released.
            if (turnedOff)
                d->stretchRelayoutPending = true;
        }
    });
    // The rows a group shows are measured: fit again when they come or go.
    connect(this, &QTreeView::expanded, this, [this] { d->fitNameColumnLater(); });
    connect(this, &QTreeView::collapsed, this, [this] { d->fitNameColumnLater(); });
}

PropertyTreeView::~PropertyTreeView()
{
    // The watcher reaches the private data, which goes first.
    delete d->watcher;
}

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
            d->fitNameColumnLater();
        });
    d->connections << connect(model, &QAbstractItemModel::rowsRemoved, this,
        [this] { d->fitNameColumnLater(); });
    d->connections << connect(model, &QAbstractItemModel::dataChanged, this,
        [this](
            const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles) {
            if (roles.isEmpty() || roles.contains(PropertyModel::IsVisibleRole)
                || roles.contains(PropertyModel::IsGroupRole)) {
                d->updateRows(topLeft.parent(), topLeft.row(), bottomRight.row(), false);
            }
            if (topLeft.column() == PropertyModel::NameColumn)
                d->fitNameColumn();
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
    d->updateAll(); // indentation changed: refit the name column
}

int PropertyTreeView::nameColumnWidth() const
{
    return columnWidth(PropertyModel::NameColumn);
}

void PropertyTreeView::setNameColumnWidth(int width)
{
    d->nameWidthFixed = true;
    const bool wasResizing = d->resizing;
    d->resizing = true; // not a drag: the header's sectionResized needs no handling
    setColumnWidth(PropertyModel::NameColumn, width);
    d->resizing = wasResizing;
    d->restoreStretch();
}

PropertyDelegate* PropertyTreeView::propertyDelegate() const
{
    return d->delegate;
}

bool PropertyTreeView::tabStopsOnCheckBoxes() const
{
    return d->tabStopsOnCheckBoxes;
}

void PropertyTreeView::setTabStopsOnCheckBoxes(bool on)
{
    d->tabStopsOnCheckBoxes = on;
}

QBrush PropertyTreeView::groupBackground() const
{
    return d->groupBackground;
}

void PropertyTreeView::setGroupBackground(const QBrush& brush)
{
    d->groupBackground = brush;
    viewport()->update();
}

QColor PropertyTreeView::groupForeground() const
{
    return d->groupForeground;
}

void PropertyTreeView::setGroupForeground(const QColor& color)
{
    d->groupForeground = color;
    viewport()->update();
}

QColor PropertyTreeView::modifiedForeground() const
{
    return d->modifiedForeground;
}

void PropertyTreeView::setModifiedForeground(const QColor& color)
{
    d->modifiedForeground = color;
    viewport()->update();
}

QColor PropertyTreeView::readOnlyForeground() const
{
    return d->readOnlyForeground;
}

void PropertyTreeView::setReadOnlyForeground(const QColor& color)
{
    d->readOnlyForeground = color;
    viewport()->update();
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

    QMenu menu(this);
    QAction* reset
        = menu.addAction(property->isGroup() ? tr("Reset group") : tr("Reset to default"));
    reset->setEnabled(detail::isResettableByUser(property));
    // A user action: read-only and disabled properties (possibly maintained by
    // the application) are left alone.
    connect(reset, &QAction::triggered, propertyModel,
        [propertyModel, persistent = QPersistentModelIndex(sourceIndex)] {
            if (const Property* target = propertyModel->propertyAt(persistent))
                detail::resetByUser(propertyModel, target);
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
        if (d->isTabStop(index))
            return index.siblingAtColumn(PropertyModel::ValueColumn);
    }
}

bool PropertyTreeView::focusNextPrevChild(bool next)
{
    // A check box stop has no editor, so Tab reaches the view itself: go on
    // along the chain instead of leaving it (at either end, leave as usual).
    if (state() != EditingState && d->isCheckBoxStop(currentIndex())) {
        const QModelIndex target = moveCursor(next ? MoveNext : MovePrevious, Qt::NoModifier);
        if (target.isValid()) {
            setCurrentIndex(target);
            return true;
        }
    }
    return QTreeView::focusNextPrevChild(next);
}

} // namespace qpb
