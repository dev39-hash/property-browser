// PropertyTreeView + PropertyDelegate interaction (docs/PLAN.md M3.2-M3.7, SPEC §5.4-5.5).

#include <qpb/qpb.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

#include "widgets/PathEdit_p.h"

using namespace qpb;

namespace {

// Camera
// ├── name          string (validator rejects "forbidden")
// ├── visible       bool
// ├── Transform
// │   ├── x         double
// │   ├── locked    double, read-only
// │   └── y         double
// ├── Camera
// │   ├── fov       int
// │   ├── projection enum
// │   └── lut       file path
// └── cacheDir      dir path
std::unique_ptr<PropertyGroup> createTree()
{
    auto root = PropertyGroup::create(QStringLiteral("Camera"));
    root->addString(QStringLiteral("name"), QStringLiteral("Main"))
        .validator([](const QVariant& value, const Property&) {
            return value.toString() == QStringLiteral("forbidden")
                ? ValidationResult::error(QStringLiteral("This name is not allowed"))
                : ValidationResult::valid();
        });
    root->addBool(QStringLiteral("visible"), true);
    PropertyGroup& transform = root->addGroup(QStringLiteral("Transform"));
    transform.addDouble(QStringLiteral("x"), 0.0).range(-100.0, 100.0);
    transform.addDouble(QStringLiteral("locked"), 1.0).readOnly();
    transform.addDouble(QStringLiteral("y"), 0.0);
    PropertyGroup& camera = root->addGroup(QStringLiteral("Camera"));
    camera.addInt(QStringLiteral("fov"), 60).range(10, 170).suffix(QStringLiteral("°"));
    camera.addEnum(QStringLiteral("projection"),
        {QStringLiteral("Perspective"), QStringLiteral("Orthographic")}, 0);
    camera.addFilePath(QStringLiteral("lut"), QString()).filter(QStringLiteral("LUT (*.cube)"));
    root->addDirPath(QStringLiteral("cacheDir"), QString());
    return root;
}

struct Fixture
{
    Fixture()
        : model(createTree())
    {
        auto* layout = new QVBoxLayout(&window);
        view = new PropertyTreeView;
        other = new QLineEdit;
        layout->addWidget(view);
        layout->addWidget(other);
        view->setModel(&model);
        window.resize(420, 420);
        window.show();
    }

    QModelIndex value(const QString& path) const
    {
        return model.indexOf(model.find(path), PropertyModel::ValueColumn);
    }

    QVariant stored(const QString& path) const
    {
        return model.find(path)->value();
    }

    // Starts editing path's value (edit trigger: current changed).
    template <class Editor> Editor* edit(const QString& path)
    {
        view->setFocus();
        view->setCurrentIndex(value(path));
        return editor<Editor>();
    }

    // The open editor. Closed editors are hidden and deleted later, so skip them.
    template <class Editor> Editor* editor() const
    {
        const QList<Editor*> editors = view->findChildren<Editor*>();
        for (Editor* candidate : editors) {
            if (candidate->isVisible())
                return candidate;
        }
        return nullptr;
    }

    QWidget window;
    PropertyModel model;
    PropertyTreeView* view = nullptr;
    QLineEdit* other = nullptr;
};

// True while the view has an editor open (editors carry the delegate's marker).
bool editorOpen(const PropertyTreeView* view)
{
    const QList<QWidget*> children = view->viewport()->findChildren<QWidget*>();
    for (const QWidget* child : children) {
        if (child->property("_qpb_editorOwner").isValid())
            return true;
    }
    return false;
}

} // namespace

class tst_PropertyTreeView : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void defaults();
    void enterCommits();
    void escapeCancels();
    void focusOutCommits();
    void tabSkipsGroupsReadOnlyAndCheckBoxes();
    void backtabMovesBackwards();
    void enumCommitsOnSelection();
    void pathDialogKeepsEditorOpen();
    void customEditorWithDialogScope();
    void checkBoxTogglesOnClickAndSpace();
    void rejectedValueShowsToolTip();
    void groupsAndNamesNotEditable();
    void listAndTreeModes();
    void hiddenRowsFollowVisibility();
    void insertedGroupsAreExpandedAndSpanned();
    void contextMenuResets();
    void contextMenuLeavesReadOnlyAlone();
    void contextMenuLeavesLiveAlone();
    void conditions();
    void nameColumnFitsContents();
    void worksThroughProxyModel();
    void multilineEditor();
    void screenshot();

private:
    std::unique_ptr<Fixture> f;
};

void tst_PropertyTreeView::init()
{
    f = std::make_unique<Fixture>();
    QVERIFY(QTest::qWaitForWindowActive(&f->window));
}

void tst_PropertyTreeView::cleanup()
{
    detail::PathEdit::setDialogProviderForTesting({});
    f.reset();
}

void tst_PropertyTreeView::defaults()
{
    PropertyTreeView* view = f->view;
    QVERIFY(view->propertyDelegate());
    QCOMPARE(view->itemDelegate(), view->propertyDelegate());
    QCOMPARE(view->mode(), PropertyTreeView::Mode::Tree);
    QVERIFY(view->editTriggers().testFlag(QAbstractItemView::CurrentChanged));
    QVERIFY(view->alternatingRowColors());
    const QModelIndex transform = f->model.indexOf(f->model.find(QStringLiteral("Transform")));
    QVERIFY(view->isExpanded(transform));
    QVERIFY(view->isFirstColumnSpanned(transform.row(), transform.parent()));
    QVERIFY(!view->isFirstColumnSpanned(0, QModelIndex()));
}

void tst_PropertyTreeView::enterCommits()
{
    auto* spinBox = f->edit<QSpinBox>(QStringLiteral("Camera/fov"));
    QVERIFY(spinBox);
    QCOMPARE(spinBox->value(), 60);
    spinBox->selectAll();
    QTest::keyClicks(spinBox, QStringLiteral("90"));
    QTest::keyClick(spinBox, Qt::Key_Return);
    QTRY_COMPARE(f->stored(QStringLiteral("Camera/fov")), QVariant(90));
    QTRY_VERIFY(!f->editor<QSpinBox>());
}

void tst_PropertyTreeView::escapeCancels()
{
    auto* spinBox = f->edit<QSpinBox>(QStringLiteral("Camera/fov"));
    QVERIFY(spinBox);
    spinBox->setValue(120);
    QTest::keyClick(spinBox, Qt::Key_Escape);
    QTRY_VERIFY(!f->editor<QSpinBox>());
    QCOMPARE(f->stored(QStringLiteral("Camera/fov")), QVariant(60));
}

void tst_PropertyTreeView::focusOutCommits()
{
    auto* lineEdit = f->edit<QLineEdit>(QStringLiteral("name"));
    QVERIFY(lineEdit);
    lineEdit->setText(QStringLiteral("Renamed"));
    f->other->setFocus();
    QTRY_COMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Renamed")));
    QTRY_VERIFY(!f->editor<QLineEdit>());
}

void tst_PropertyTreeView::tabSkipsGroupsReadOnlyAndCheckBoxes()
{
    // name → (visible: check box) → (Transform: group) → x
    auto* lineEdit = f->edit<QLineEdit>(QStringLiteral("name"));
    QVERIFY(lineEdit);
    QTest::keyClick(lineEdit, Qt::Key_Tab);
    QTRY_COMPARE(f->view->currentIndex(), f->value(QStringLiteral("Transform/x")));
    QTRY_VERIFY(f->editor<QDoubleSpinBox>());

    // x → (locked: read-only) → y
    QTest::keyClick(f->editor<QDoubleSpinBox>(), Qt::Key_Tab);
    QTRY_COMPARE(f->view->currentIndex(), f->value(QStringLiteral("Transform/y")));

    // y → (Camera: group) → fov
    QTRY_VERIFY(f->editor<QDoubleSpinBox>());
    QTest::keyClick(f->editor<QDoubleSpinBox>(), Qt::Key_Tab);
    QTRY_COMPARE(f->view->currentIndex(), f->value(QStringLiteral("Camera/fov")));
}

void tst_PropertyTreeView::backtabMovesBackwards()
{
    auto* spinBox = f->edit<QSpinBox>(QStringLiteral("Camera/fov"));
    QVERIFY(spinBox);
    QTest::keyClick(spinBox, Qt::Key_Backtab, Qt::ShiftModifier);
    QTRY_COMPARE(f->view->currentIndex(), f->value(QStringLiteral("Transform/y")));
}

void tst_PropertyTreeView::enumCommitsOnSelection()
{
    auto* comboBox = f->edit<QComboBox>(QStringLiteral("Camera/projection"));
    QVERIFY(comboBox);
    QCOMPARE(comboBox->currentIndex(), 0);
    QTest::keyClick(comboBox, Qt::Key_Down);
    QTRY_COMPARE(f->stored(QStringLiteral("Camera/projection")), QVariant(1));
    QTRY_VERIFY(!f->editor<QComboBox>());
    QCOMPARE(f->value(QStringLiteral("Camera/projection")).data().toString(),
        QStringLiteral("Orthographic"));
}

void tst_PropertyTreeView::pathDialogKeepsEditorOpen()
{
    auto* pathEdit = f->edit<detail::PathEdit>(QStringLiteral("Camera/lut"));
    QVERIFY(pathEdit);
    QPointer<detail::PathEdit> guard(pathEdit);
    bool editorSurvivedFocusLoss = false;
    QString filter;
    detail::PathEdit::setDialogProviderForTesting([&](detail::PathEdit* edit) {
        filter = edit->filter();
        // A modal dialog takes the focus away from the editor.
        f->other->setFocus();
        QCoreApplication::processEvents();
        editorSurvivedFocusLoss = !guard.isNull();
        return QStringLiteral("/luts/film.cube");
    });
    pathEdit->browseButton()->click();
    QCOMPARE(filter, QStringLiteral("LUT (*.cube)"));
    QVERIFY(editorSurvivedFocusLoss);
    QTRY_COMPARE(
        f->stored(QStringLiteral("Camera/lut")), QVariant(QStringLiteral("/luts/film.cube")));
    QTRY_VERIFY(guard.isNull());
}

void tst_PropertyTreeView::customEditorWithDialogScope()
{
    // A custom editor (like a color button) that opens a dialog and commits.
    const TypeId type = QStringLiteral("test.counter");
    TypeRegistry::global().registerType<int>(type, TypeHandler());
    EditorHandler handler;
    handler.createEditor = [](QWidget* parent, const Property&) {
        auto* button = new QPushButton(parent);
        QObject::connect(button, &QPushButton::clicked, button, [button] {
            {
                EditorDialogScope scope(button);
                button->window()->findChild<QLineEdit*>()->setFocus(); // "dialog" steals focus
                QCoreApplication::processEvents();
            }
            button->setText(QString::number(button->text().toInt() + 1));
            EditorFactory::notifyCommit(button);
        });
        return button;
    };
    handler.setEditorData = [](QWidget* editor, const QVariant& value, const Property&) {
        static_cast<QPushButton*>(editor)->setText(value.toString());
    };
    handler.editorData = [](QWidget* editor, const Property&) {
        return QVariant(static_cast<QPushButton*>(editor)->text().toInt());
    };
    EditorFactory::global().registerEditor(type, handler);

    f->model.root()->add(type, QStringLiteral("counter"), 41);
    auto* button = f->edit<QPushButton>(QStringLiteral("counter"));
    QVERIFY(button);
    button->click();
    QTRY_COMPARE(f->stored(QStringLiteral("counter")), QVariant(42));
}

void tst_PropertyTreeView::checkBoxTogglesOnClickAndSpace()
{
    const QModelIndex visible = f->value(QStringLiteral("visible"));
    QVERIFY(!f->view->findChild<QCheckBox*>());
    const QRect cell = f->view->visualRect(visible);
    // Anywhere in the cell, not only on the indicator.
    QTest::mouseClick(f->view->viewport(), Qt::LeftButton, {}, cell.topRight() + QPoint(-4, 4));
    QCOMPARE(f->stored(QStringLiteral("visible")), QVariant(false));

    f->view->setCurrentIndex(visible);
    QTest::keyClick(f->view, Qt::Key_Space);
    QCOMPARE(f->stored(QStringLiteral("visible")), QVariant(true));

    f->model.find(QStringLiteral("visible"))->setReadOnly(true);
    QTest::mouseClick(f->view->viewport(), Qt::LeftButton, {}, cell.center());
    QCOMPARE(f->stored(QStringLiteral("visible")), QVariant(true));
}

void tst_PropertyTreeView::rejectedValueShowsToolTip()
{
    auto* lineEdit = f->edit<QLineEdit>(QStringLiteral("name"));
    QVERIFY(lineEdit);
    lineEdit->setText(QStringLiteral("forbidden"));
    QTest::keyClick(lineEdit, Qt::Key_Return);
    QTRY_VERIFY(!f->editor<QLineEdit>());
    QCOMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Main")));
    QTRY_COMPARE(QToolTip::text(), QStringLiteral("This name is not allowed"));
}

void tst_PropertyTreeView::groupsAndNamesNotEditable()
{
    const QModelIndex transform = f->model.indexOf(f->model.find(QStringLiteral("Transform")));
    f->view->setCurrentIndex(transform.siblingAtColumn(1));
    QVERIFY(!editorOpen(f->view));
    f->view->setCurrentIndex(f->value(QStringLiteral("Camera/fov")).siblingAtColumn(0));
    QVERIFY(!f->editor<QSpinBox>());
}

void tst_PropertyTreeView::listAndTreeModes()
{
    PropertyTreeView* view = f->view;
    const int treeIndentation = view->indentation();
    const QModelIndex transform = f->model.indexOf(f->model.find(QStringLiteral("Transform")));
    view->collapse(transform);
    view->setCurrentIndex(f->value(QStringLiteral("Camera/fov")).siblingAtColumn(0));
    const QModelIndex current = view->currentIndex();

    view->setMode(PropertyTreeView::Mode::List);
    QCOMPARE(view->mode(), PropertyTreeView::Mode::List);
    QVERIFY(!view->rootIsDecorated());
    QVERIFY(!view->itemsExpandable());
    QCOMPARE(view->indentation(), 0);
    QVERIFY(view->isExpanded(transform)); // everything is shown flat
    QVERIFY(view->isFirstColumnSpanned(transform.row(), transform.parent()));
    QCOMPARE(view->currentIndex(), current);

    view->setMode(PropertyTreeView::Mode::Tree);
    QVERIFY(view->rootIsDecorated());
    QVERIFY(view->itemsExpandable());
    QCOMPARE(view->indentation(), treeIndentation);
    QCOMPARE(view->currentIndex(), current);
    QCOMPARE(view->model(), &f->model);
}

void tst_PropertyTreeView::hiddenRowsFollowVisibility()
{
    Property* y = f->model.find(QStringLiteral("Transform/y"));
    const QModelIndex index = f->model.indexOf(y);
    QVERIFY(!f->view->isRowHidden(index.row(), index.parent()));
    y->setVisible(false);
    QVERIFY(f->view->isRowHidden(index.row(), index.parent()));
    y->setVisible(true);
    QVERIFY(!f->view->isRowHidden(index.row(), index.parent()));

    f->model.find(QStringLiteral("Transform"))->setVisible(false);
    const QModelIndex transform = f->model.indexOf(f->model.find(QStringLiteral("Transform")));
    QVERIFY(f->view->isRowHidden(transform.row(), QModelIndex()));
}

void tst_PropertyTreeView::insertedGroupsAreExpandedAndSpanned()
{
    PropertyGroup& added = f->model.root()->addGroup(QStringLiteral("Added"));
    added.addInt(QStringLiteral("i"), 1);
    PropertyGroup& nested = added.addGroup(QStringLiteral("Nested"));
    nested.addBool(QStringLiteral("hidden"), true).visible(false);

    const QModelIndex addedIndex = f->model.indexOf(&added);
    const QModelIndex nestedIndex = f->model.indexOf(&nested);
    QVERIFY(f->view->isExpanded(addedIndex));
    QVERIFY(f->view->isExpanded(nestedIndex));
    QVERIFY(f->view->isFirstColumnSpanned(addedIndex.row(), QModelIndex()));
    QVERIFY(f->view->isFirstColumnSpanned(nestedIndex.row(), addedIndex));
    QVERIFY(f->view->isRowHidden(0, nestedIndex));
}

void tst_PropertyTreeView::contextMenuResets()
{
    const auto openMenu = [this](const QModelIndex& index, bool trigger) {
        bool enabled = false;
        QTimer::singleShot(0, this, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            QVERIFY(menu);
            QCOMPARE(menu->actions().size(), 1);
            enabled = menu->actions().first()->isEnabled();
            if (trigger && enabled)
                menu->actions().first()->trigger();
            menu->close();
        });
        const QPoint position = f->view->visualRect(index).center();
        QContextMenuEvent event(
            QContextMenuEvent::Mouse, position, f->view->viewport()->mapToGlobal(position));
        QApplication::sendEvent(f->view->viewport(), &event);
        return enabled;
    };

    const QModelIndex fov = f->value(QStringLiteral("Camera/fov")).siblingAtColumn(0);
    QVERIFY(!openMenu(fov, false)); // not modified yet

    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 100));
    QVERIFY(f->model.setValue(QStringLiteral("Camera/projection"), 1));
    QVERIFY(openMenu(fov, true));
    QCOMPARE(f->stored(QStringLiteral("Camera/fov")), QVariant(60));

    const QModelIndex camera = f->model.indexOf(f->model.find(QStringLiteral("Camera")));
    QVERIFY(openMenu(camera, true)); // "Reset group"
    QCOMPARE(f->stored(QStringLiteral("Camera/projection")), QVariant(0));
    QVERIFY(!openMenu(camera, false));
}

// "Reset group" is a user action: read-only children maintained by the
// application keep their values.
void tst_PropertyTreeView::contextMenuLeavesReadOnlyAlone()
{
    Property* x = f->model.find(QStringLiteral("Transform/x"));
    Property* locked = f->model.find(QStringLiteral("Transform/locked"));
    QVERIFY(x->setValue(5.0));
    QVERIFY(locked->setValue(7.0)); // application write to a read-only property
    const QModelIndex transform = f->model.indexOf(f->model.find(QStringLiteral("Transform")));
    QTimer::singleShot(0, this, [] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu) {
            menu->actions().first()->trigger();
            menu->close();
        }
    });
    const QPoint position = f->view->visualRect(transform).center();
    QContextMenuEvent event(
        QContextMenuEvent::Mouse, position, f->view->viewport()->mapToGlobal(position));
    QApplication::sendEvent(f->view->viewport(), &event);
    QCOMPARE(x->value(), QVariant(0.0));
    QCOMPARE(locked->value(), QVariant(7.0));
}

// Since 1.4: rows follow conditions (hidden rows, no editor when disabled).
void tst_PropertyTreeView::conditions()
{
    f->model.find(QStringLiteral("Camera/lut"))->setVisibleWhen(QStringLiteral("visible"));
    f->model.find(QStringLiteral("Transform/y"))->setEnabledWhen(QStringLiteral("visible"));
    const QModelIndex lut = f->model.indexOf(f->model.find(QStringLiteral("Camera/lut")));
    QVERIFY(!f->view->isRowHidden(lut.row(), lut.parent()));

    QVERIFY(f->model.setValue(QStringLiteral("visible"), false));
    QVERIFY(f->view->isRowHidden(lut.row(), lut.parent()));
    QVERIFY(!f->value(QStringLiteral("Transform/y")).flags().testFlag(Qt::ItemIsEditable));
    QVERIFY(f->model.setValue(QStringLiteral("visible"), true));
    QVERIFY(!f->view->isRowHidden(lut.row(), lut.parent()));
    QVERIFY(f->value(QStringLiteral("Transform/y")).flags().testFlag(Qt::ItemIsEditable));
}

// Since 1.3: a live value is not bold and survives "Reset group".
void tst_PropertyTreeView::contextMenuLeavesLiveAlone()
{
    Property* x = f->model.find(QStringLiteral("Transform/x"));
    Property* y = f->model.find(QStringLiteral("Transform/y"));
    y->setLive(true);
    QVERIFY(x->setValue(5.0));
    QVERIFY(y->setValue(9.0));
    const QModelIndex yName = f->model.indexOf(y);
    QVERIFY(!yName.data(PropertyModel::IsModifiedRole).toBool());

    const QModelIndex transform = f->model.indexOf(f->model.find(QStringLiteral("Transform")));
    QTimer::singleShot(0, this, [] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu) {
            menu->actions().first()->trigger();
            menu->close();
        }
    });
    const QPoint position = f->view->visualRect(transform).center();
    QContextMenuEvent event(
        QContextMenuEvent::Mouse, position, f->view->viewport()->mapToGlobal(position));
    QApplication::sendEvent(f->view->viewport(), &event);
    QCOMPARE(x->value(), QVariant(0.0));
    QCOMPARE(y->value(), QVariant(9.0));
}

void tst_PropertyTreeView::nameColumnFitsContents()
{
    PropertyTreeView view;
    PropertyModel model(createTree());
    Property* fov = model.find(QStringLiteral("Camera/fov"));
    fov->setDisplayName(QStringLiteral("A rather long display name"));
    view.resize(600, 300);
    view.setModel(&model);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    const int needed = view.fontMetrics().horizontalAdvance(fov->displayName());
    QVERIFY2(view.nameColumnWidth() > needed,
        qPrintable(QStringLiteral("%1 <= %2").arg(view.nameColumnWidth()).arg(needed)));

    // Growing names refit the column; an explicit width is kept.
    fov->setDisplayName(QStringLiteral("An even longer display name for the field of view"));
    QVERIFY(view.nameColumnWidth()
        > view.fontMetrics().horizontalAdvance(QStringLiteral("An even longer display name")));
    view.setNameColumnWidth(80);
    model.root()->addInt(QStringLiteral("aVeryLongPropertyIdentifierIndeed"), 1);
    QCOMPARE(view.nameColumnWidth(), 80);
}

// Since 1.1: a multi-line text editor taller than the row; Enter adds a line,
// Ctrl+Enter commits.
void tst_PropertyTreeView::multilineEditor()
{
    f->model.root()->addString(QStringLiteral("notes"), QStringLiteral("a\nb")).multiline();
    const QModelIndex notes = f->value(QStringLiteral("notes"));
    QCOMPARE(notes.data(Qt::DisplayRole).toString(), QStringLiteral("a \u00B6 b"));

    auto* editor = f->edit<QPlainTextEdit>(QStringLiteral("notes"));
    QVERIFY(editor);
    QCOMPARE(editor->toPlainText(), QStringLiteral("a\nb"));
    QVERIFY(editor->height() > f->view->visualRect(notes).height());
    QVERIFY(editor->geometry().bottom() < f->view->viewport()->height());

    editor->moveCursor(QTextCursor::End);
    QTest::keyClick(editor, Qt::Key_Return);
    QTest::keyClicks(editor, QStringLiteral("c"));
    QCoreApplication::processEvents();
    QVERIFY(editorOpen(f->view));
    QCOMPARE(f->stored(QStringLiteral("notes")), QVariant(QStringLiteral("a\nb")));

    QTest::keyClick(editor, Qt::Key_Return, Qt::ControlModifier);
    QTRY_VERIFY(!editorOpen(f->view));
    QCOMPARE(f->stored(QStringLiteral("notes")), QVariant(QStringLiteral("a\nb\nc")));
}

void tst_PropertyTreeView::worksThroughProxyModel()
{
    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&f->model);
    proxy.setRecursiveFilteringEnabled(true);
    f->view->setModel(&proxy);

    const QModelIndex fov = proxy.mapFromSource(f->value(QStringLiteral("Camera/fov")));
    f->view->setFocus();
    f->view->setCurrentIndex(fov);
    auto* spinBox = f->editor<QSpinBox>();
    QVERIFY(spinBox);
    spinBox->setValue(33);
    QTest::keyClick(spinBox, Qt::Key_Return);
    QTRY_COMPARE(f->stored(QStringLiteral("Camera/fov")), QVariant(33));

    proxy.setFilterFixedString(QStringLiteral("fov"));
    QCOMPARE(proxy.rowCount(), 1); // the Camera group, kept for its matching child
}

void tst_PropertyTreeView::screenshot()
{
    const QString directory = qEnvironmentVariable("QPB_SCREENSHOT_DIR");
    if (directory.isEmpty())
        QSKIP("Set QPB_SCREENSHOT_DIR to save screenshots of the view");
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 90));
    f->view->setCurrentIndex(f->value(QStringLiteral("Transform/x")));
    QVERIFY(f->window.grab().save(QDir(directory).filePath(QStringLiteral("tree.png"))));
    f->view->setMode(PropertyTreeView::Mode::List);
    QVERIFY(f->window.grab().save(QDir(directory).filePath(QStringLiteral("list.png"))));
}

QTEST_MAIN(tst_PropertyTreeView)
#include "tst_propertytreeview.moc"
