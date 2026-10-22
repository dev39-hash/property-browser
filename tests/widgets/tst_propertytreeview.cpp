// PropertyTreeView + PropertyDelegate interaction (see qpb/doc/guide/views.md).

#include <qpb/qpb.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QDoubleSpinBox>
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
#include <QTreeView>
#include <QVBoxLayout>

#include <memory>

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

// Whether any pixel of image inside rect is close to the pure colour red,
// green or blue: text drawn in such a colour on black stays recognisable when
// it is anti-aliased (and "tofu" boxes of font-less test platforms count too).
bool hasPixels(const QImage& image, const QRect& rect, Qt::GlobalColor pure)
{
    for (int y = rect.top(); y <= rect.bottom(); ++y) {
        for (int x = rect.left(); x <= rect.right(); ++x) {
            const QColor c = image.pixelColor(x, y);
            const int r = c.red(), g = c.green(), b = c.blue();
            if ((pure == Qt::red && r > 150 && g < 80 && b < 80)
                || (pure == Qt::green && g > 150 && r < 80 && b < 80)
                || (pure == Qt::blue && b > 150 && r < 80 && g < 80)
                || (pure == Qt::yellow && r > 150 && g > 150 && b < 80))
                return true;
        }
    }
    return false;
}

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

// True when every editor the delegate has open is for a row that still
// exists (its remembered index is valid).
bool openEditorsBelongToLiveRows(const PropertyTreeView* view)
{
    const QList<QWidget*> children = view->viewport()->findChildren<QWidget*>();
    for (const QWidget* child : children) {
        if (!child->property("_qpb_editorOwner").isValid())
            continue;
        const auto index = child->property("_qpb_editorIndex").value<QPersistentModelIndex>();
        if (!index.isValid())
            return false;
    }
    return true;
}

// Reports when a property is destroyed: its validator owns a token that the
// returned pointer watches.
std::weak_ptr<int> trackLifetime(Property& property)
{
    auto token = std::make_shared<int>(0);
    property.setValidator([token](const QVariant&, const Property&) {
        return *token == 0 ? ValidationResult::valid() : ValidationResult::error(QString());
    });
    return token;
}

enum class TreeChange {
    ReplaceTree,
    RemoveGroup,
    RemoveProperty,
};

// Changes the tree createTree() made the way an application handler would.
void changeTree(PropertyModel& model, TreeChange change)
{
    switch (change) {
    case TreeChange::ReplaceTree:
        model.setRoot(createTree());
        break;
    case TreeChange::RemoveGroup:
        model.root()->remove(QStringLiteral("Camera"));
        break;
    case TreeChange::RemoveProperty:
        model.find(QStringLiteral("Camera"))->toGroup()->remove(QStringLiteral("fov"));
        break;
    }
}

void addTreeChangeRows()
{
    QTest::addColumn<int>("change");
    QTest::newRow("replaceTree") << int(TreeChange::ReplaceTree);
    QTest::newRow("removeGroup") << int(TreeChange::RemoveGroup);
    QTest::newRow("removeProperty") << int(TreeChange::RemoveProperty);
}

// A handler of valueChanged() for "Camera/fov" that changes the tree, and a
// second handler connected after it that records the first change it sees.
struct TreeChangingHandlers
{
    TreeChangingHandlers(PropertyModel& model, TreeChange change)
        : lifetime(trackLifetime(*model.find(QStringLiteral("Camera/fov"))))
    {
        QObject::connect(&model, &PropertyModel::valueChanged, &model,
            [this, &model, change](const QString& path) {
                if (path == QStringLiteral("Camera/fov") && !treeChanged) {
                    treeChanged = true;
                    changeTree(model, change);
                }
            });
        QObject::connect(&model, &PropertyModel::valueChanged, &model,
            [this](const QString& path, const QVariant& newValue, const QVariant& oldValue) {
                if (calls++ > 0)
                    return;
                propertyAlive = !lifetime.expired();
                if (!propertyAlive)
                    return; // the values would refer to a destroyed property
                firstPath = path;
                firstNewValue = newValue;
                firstOldValue = oldValue;
            });
    }

    std::weak_ptr<int> lifetime;
    bool treeChanged = false;
    int calls = 0;
    bool propertyAlive = false;
    QString firstPath;
    QVariant firstNewValue;
    QVariant firstOldValue;
};

// Opens the context menu of the row at index and triggers its only action if
// enabled. Returns whether the action was enabled.
bool resetFromContextMenu(PropertyTreeView* view, const QModelIndex& index)
{
    bool enabled = false;
    QTimer::singleShot(0, view, [&enabled] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu)
            return;
        enabled = menu->actions().value(0) && menu->actions().value(0)->isEnabled();
        if (enabled)
            menu->actions().value(0)->trigger();
        menu->close();
    });
    const QPoint position = view->visualRect(index).center();
    QContextMenuEvent event(
        QContextMenuEvent::Mouse, position, view->viewport()->mapToGlobal(position));
    QApplication::sendEvent(view->viewport(), &event);
    return enabled;
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
    void tabStopsOnCheckBoxes();
    void tabStopsOnCheckBoxesSkipsUnchangeable();
    void tabStopsOnCheckBoxesAtTheEnd();
    void tabStopsOnCheckBoxesThroughProxy();
    void styleSheetColours();
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

    void commitWithHandlerChangingTree_data();
    void commitWithHandlerChangingTree();
    void groupResetWithHandlerChangingTree_data();
    void groupResetWithHandlerChangingTree();

    void openIntEditorFollowsAttributeChanges();
    void openDoubleEditorFollowsAttributeChanges();
    void customEditorAppliesAttributesOncePerChange();
    void plainTreeViewEditorFollowsAttributeChanges();
    void editorThroughFilterProxyFollowsAttributeChanges();
    void typedTextSurvivesUnrelatedChanges();
    void openEditorFollowsAnExternalValueChange();
    void attributeChangesAfterTheEditorClosedAreHarmless();

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

// Since 1.5: check boxes join the Tab chain on request only, so the keyboard
// behaviour of existing applications does not change on upgrade.
void tst_PropertyTreeView::tabStopsOnCheckBoxes()
{
    PropertyTreeView* view = f->view;
    QVERIFY(!view->tabStopsOnCheckBoxes());
    QVERIFY(view->setProperty("tabStopsOnCheckBoxes", true)); // also a Q_PROPERTY
    QVERIFY(view->tabStopsOnCheckBoxes());

    // name → visible: the check box is current, no editor opens.
    auto* lineEdit = f->edit<QLineEdit>(QStringLiteral("name"));
    QVERIFY(lineEdit);
    QTest::keyClick(lineEdit, Qt::Key_Tab);
    QTRY_COMPARE(view->currentIndex(), f->value(QStringLiteral("visible")));
    QTRY_VERIFY(!editorOpen(view));
    QTRY_VERIFY(view->hasFocus());

    // Space toggles it.
    QTest::keyClick(view, Qt::Key_Space);
    QCOMPARE(f->stored(QStringLiteral("visible")), QVariant(false));

    // Tab goes on to the next editor: visible → (Transform: group) → x.
    QTest::keyClick(view, Qt::Key_Tab);
    QTRY_COMPARE(view->currentIndex(), f->value(QStringLiteral("Transform/x")));
    QTRY_VERIFY(f->editor<QDoubleSpinBox>());

    // Shift+Tab stops on the check box again, then reaches name's editor.
    QTest::keyClick(f->editor<QDoubleSpinBox>(), Qt::Key_Backtab, Qt::ShiftModifier);
    QTRY_COMPARE(view->currentIndex(), f->value(QStringLiteral("visible")));
    QTRY_VERIFY(!editorOpen(view));
    QTRY_VERIFY(view->hasFocus());
    QTest::keyClick(view, Qt::Key_Backtab, Qt::ShiftModifier);
    QTRY_COMPARE(view->currentIndex(), f->value(QStringLiteral("name")));
    QTRY_VERIFY(f->editor<QLineEdit>());

    // Switched off again: the 1.4 chain.
    view->setTabStopsOnCheckBoxes(false);
    QTest::keyClick(f->editor<QLineEdit>(), Qt::Key_Tab);
    QTRY_COMPARE(view->currentIndex(), f->value(QStringLiteral("Transform/x")));
}

void tst_PropertyTreeView::tabStopsOnCheckBoxesSkipsUnchangeable()
{
    f->view->setTabStopsOnCheckBoxes(true);
    Property* visible = f->model.find(QStringLiteral("visible"));
    const auto tabFromName = [this] {
        auto* lineEdit = f->edit<QLineEdit>(QStringLiteral("name"));
        QVERIFY(lineEdit);
        QTest::keyClick(lineEdit, Qt::Key_Tab);
        QTRY_COMPARE(f->view->currentIndex(), f->value(QStringLiteral("Transform/x")));
        QTRY_VERIFY(f->editor<QDoubleSpinBox>());
        QTest::keyClick(f->editor<QDoubleSpinBox>(), Qt::Key_Escape);
        QTRY_VERIFY(!editorOpen(f->view));
    };

    visible->setEnabled(false);
    tabFromName();
    if (QTest::currentTestFailed())
        return;
    visible->setEnabled(true);

    visible->setReadOnly(true);
    tabFromName();
    if (QTest::currentTestFailed())
        return;
    visible->setReadOnly(false);

    visible->setVisible(false);
    tabFromName();
}

void tst_PropertyTreeView::tabStopsOnCheckBoxesAtTheEnd()
{
    // The last stop is a check box: Tab leaves the view, as after the last editor.
    f->model.root()->addInt(QStringLiteral("count"), 0);
    f->model.root()->addBool(QStringLiteral("last"), false);
    f->view->setTabStopsOnCheckBoxes(true);
    auto* spinBox = f->edit<QSpinBox>(QStringLiteral("count"));
    QVERIFY(spinBox);
    QTest::keyClick(spinBox, Qt::Key_Tab);
    QTRY_COMPARE(f->view->currentIndex(), f->value(QStringLiteral("last")));
    QTRY_VERIFY(f->view->hasFocus());
    QTest::keyClick(f->view, Qt::Key_Tab);
    QTRY_VERIFY(f->other->hasFocus());
    QCOMPARE(f->view->currentIndex(), f->value(QStringLiteral("last")));
}

void tst_PropertyTreeView::tabStopsOnCheckBoxesThroughProxy()
{
    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&f->model);
    PropertyTreeView* view = f->view;
    view->setModel(&proxy);
    view->setTabStopsOnCheckBoxes(true);
    view->setFocus();
    view->setCurrentIndex(proxy.mapFromSource(f->value(QStringLiteral("name"))));
    QTRY_VERIFY(f->editor<QLineEdit>());
    QTest::keyClick(f->editor<QLineEdit>(), Qt::Key_Tab);
    QTRY_COMPARE(view->currentIndex(), proxy.mapFromSource(f->value(QStringLiteral("visible"))));
    QTest::keyClick(view, Qt::Key_Space);
    QCOMPARE(f->stored(QStringLiteral("visible")), QVariant(false));
    QTest::keyClick(view, Qt::Key_Tab);
    QTRY_COMPARE(view->currentIndex(), proxy.mapFromSource(f->value(QStringLiteral("Transform/x"))));
    view->setModel(&f->model); // before the proxy goes away
}

// Since 1.6 (qpb/doc/guide/style-sheets.md): colours of what the view paints
// itself, set from a style sheet with qproperty-.
void tst_PropertyTreeView::styleSheetColours()
{
    PropertyTreeView* view = f->view;
    QCOMPARE(view->groupBackground().style(), Qt::NoBrush);
    QVERIFY(!view->groupForeground().isValid());
    QVERIFY(!view->modifiedForeground().isValid());
    QVERIFY(!view->readOnlyForeground().isValid());
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 90));
    const QModelIndex transform = f->model.indexOf(f->model.find(QStringLiteral("Transform")));
    const QRect group = view->visualRect(transform);
    const QPoint groupPixel(group.right() - 2, group.center().y()); // right of the title

    // Defaults: the palette's Button behind group rows, as in 1.0-1.5.
    QImage image = view->viewport()->grab().toImage();
    QCOMPARE(image.pixelColor(groupPixel), view->palette().color(QPalette::Button));

    // The sheet's background-color also sets the palette's Button: without
    // groupBackground, group rows would look like any other row.
    const QString sheet = QStringLiteral(
        "qpb--PropertyTreeView { background-color: #000000; alternate-background-color: #000000;"
        "  color: #ffffff; selection-background-color: #000000;"
        "  qproperty-groupBackground: #203040; qproperty-groupForeground: #00ff00;"
        "  qproperty-modifiedForeground: #ff0000; qproperty-readOnlyForeground: #0000ff; }"
        "qpb--PropertyTreeView::item:hover { background-color: #333333; }");
    view->setStyleSheet(sheet);
    view->ensurePolished();
    QCOMPARE(view->groupBackground().color(), QColor(0x20, 0x30, 0x40));
    QCOMPARE(view->groupForeground(), QColor(Qt::green));
    QCOMPARE(view->modifiedForeground(), QColor(Qt::red));
    QCOMPARE(view->readOnlyForeground(), QColor(Qt::blue));
    view->setCurrentIndex(QModelIndex());
    view->clearSelection();

    const auto name = [this](const QString& path) {
        return f->view->visualRect(f->model.indexOf(f->model.find(path)));
    };
    image = view->viewport()->grab().toImage();
    QCOMPARE(image.pixelColor(groupPixel), QColor(0x20, 0x30, 0x40));
    QVERIFY(hasPixels(image, group, Qt::green));
    QVERIFY(hasPixels(image, name(QStringLiteral("Camera/fov")), Qt::red)); // modified
    QVERIFY(!hasPixels(image, name(QStringLiteral("Transform/x")), Qt::red));
    QVERIFY(hasPixels(image, f->view->visualRect(f->value(QStringLiteral("Transform/locked"))),
        Qt::blue)); // read-only

    // A matching ::item rule with a color still wins, as in any view.
    view->setStyleSheet(sheet + QStringLiteral("qpb--PropertyTreeView::item { color: #ffff00; }"));
    image = view->viewport()->grab().toImage();
    QVERIFY(hasPixels(image, name(QStringLiteral("Camera/fov")), Qt::yellow));
    QVERIFY(!hasPixels(image, name(QStringLiteral("Camera/fov")), Qt::red));

    // Invalid values select the defaults again.
    view->setStyleSheet(QString());
    view->setGroupBackground(QBrush());
    view->setModifiedForeground(QColor());
    QCOMPARE(view->groupBackground().style(), Qt::NoBrush);
    QVERIFY(!view->modifiedForeground().isValid());
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
        // Called through the button: MSVC 2019 warns (C4573) on QObject::connect here.
        button->connect(button, &QPushButton::clicked, button, [button] {
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

namespace {

void checkTreeAfter(Fixture& f, TreeChange change)
{
    switch (change) {
    case TreeChange::ReplaceTree:
        QCOMPARE(f.stored(QStringLiteral("Camera/fov")), QVariant(60));
        QCOMPARE(f.stored(QStringLiteral("Camera/projection")), QVariant(0));
        break;
    case TreeChange::RemoveGroup:
        QVERIFY(!f.model.find(QStringLiteral("Camera")));
        QVERIFY(f.model.find(QStringLiteral("name")));
        break;
    case TreeChange::RemoveProperty:
        QVERIFY(f.model.find(QStringLiteral("Camera")));
        QVERIFY(!f.model.find(QStringLiteral("Camera/fov")));
        break;
    }
}

} // namespace

void tst_PropertyTreeView::commitWithHandlerChangingTree_data()
{
    addTreeChangeRows();
}

// A handler of valueChanged() changes the tree while the delegate is writing
// the committed value.
void tst_PropertyTreeView::commitWithHandlerChangingTree()
{
    QFETCH(int, change);
    TreeChangingHandlers handlers(f->model, TreeChange(change));
    auto* spinBox = f->edit<QSpinBox>(QStringLiteral("Camera/fov"));
    QVERIFY(spinBox);

    spinBox->selectAll();
    QTest::keyClicks(spinBox, QStringLiteral("90"));
    QTest::keyClick(spinBox, Qt::Key_Return);

    QTRY_COMPARE(handlers.calls, 1);
    QVERIFY2(handlers.propertyAlive,
        "the edited property was destroyed while a later handler of the same change ran");
    QCOMPARE(handlers.firstPath, QStringLiteral("Camera/fov"));
    QCOMPARE(handlers.firstNewValue, QVariant(90));
    QCOMPARE(handlers.firstOldValue, QVariant(60));
    QTRY_VERIFY(handlers.lifetime.expired());
    if (TreeChange(change) == TreeChange::RemoveProperty) {
        // Removing the edited row makes the view move to the next row, which
        // opens an editor there; no open editor may belong to the removed row.
        QTRY_VERIFY(openEditorsBelongToLiveRows(f->view));
    } else {
        QTRY_VERIFY(!editorOpen(f->view));
    }
    checkTreeAfter(*f, TreeChange(change));

    // The view keeps working with the changed tree.
    auto* lineEdit = f->edit<QLineEdit>(QStringLiteral("name"));
    QVERIFY(lineEdit);
    lineEdit->setText(QStringLiteral("Renamed"));
    QTest::keyClick(lineEdit, Qt::Key_Return);
    QTRY_COMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Renamed")));
}

void tst_PropertyTreeView::groupResetWithHandlerChangingTree_data()
{
    addTreeChangeRows();
}

// "Reset group" from the context menu writes one value after the other while a
// handler of valueChanged() changes the tree.
void tst_PropertyTreeView::groupResetWithHandlerChangingTree()
{
    QFETCH(int, change);
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 100));
    QVERIFY(f->model.setValue(QStringLiteral("Camera/projection"), 1));
    TreeChangingHandlers handlers(f->model, TreeChange(change));
    const QModelIndex camera = f->model.indexOf(f->model.find(QStringLiteral("Camera")));

    QVERIFY(resetFromContextMenu(f->view, camera));

    QVERIFY(handlers.treeChanged);
    QVERIFY(handlers.calls >= 1);
    QVERIFY2(handlers.propertyAlive,
        "the reset property was destroyed while a later handler of the same change ran");
    QCOMPARE(handlers.firstPath, QStringLiteral("Camera/fov"));
    QCOMPARE(handlers.firstNewValue, QVariant(60));
    QCOMPARE(handlers.firstOldValue, QVariant(100));
    QVERIFY(handlers.lifetime.expired());
    checkTreeAfter(*f, TreeChange(change));
}

// An editor that is open while its property's range, precision or suffix
// changes follows the change.
void tst_PropertyTreeView::openIntEditorFollowsAttributeChanges()
{
    auto* spinBox = f->edit<QSpinBox>(QStringLiteral("Camera/fov"));
    QVERIFY(spinBox);
    QCOMPARE(spinBox->maximum(), 170);
    Property* fov = f->model.find(QStringLiteral("Camera/fov"));

    fov->setAttribute(Attr::Maximum, 100);
    QTRY_COMPARE(spinBox->maximum(), 100);
    fov->setAttribute(Attr::Minimum, 20);
    QTRY_COMPARE(spinBox->minimum(), 20);
    fov->setAttribute(Attr::Suffix, QStringLiteral(" deg"));
    QTRY_COMPARE(spinBox->suffix(), QStringLiteral(" deg"));
    QVERIFY(editorOpen(f->view));

    spinBox->setValue(150);
    QCOMPARE(spinBox->value(), 100);
    QTest::keyClick(spinBox, Qt::Key_Return);
    QTRY_COMPARE(f->stored(QStringLiteral("Camera/fov")), QVariant(100));
    QTRY_VERIFY(!editorOpen(f->view));
}

void tst_PropertyTreeView::openDoubleEditorFollowsAttributeChanges()
{
    auto* spinBox = f->edit<QDoubleSpinBox>(QStringLiteral("Transform/x"));
    QVERIFY(spinBox);
    QCOMPARE(spinBox->decimals(), 2);
    Property* x = f->model.find(QStringLiteral("Transform/x"));

    x->setAttribute(Attr::Decimals, 4);
    QTRY_COMPARE(spinBox->decimals(), 4);
    x->setAttribute(Attr::Minimum, -5.0);
    QTRY_COMPARE(spinBox->minimum(), -5.0);
    x->setAttribute(Attr::Maximum, 5.0);
    QTRY_COMPARE(spinBox->maximum(), 5.0);
    x->setAttribute(Attr::Suffix, QStringLiteral(" m"));
    QTRY_COMPARE(spinBox->suffix(), QStringLiteral(" m"));
    QVERIFY(editorOpen(f->view));

    x->removeAttribute(Attr::Suffix);
    QTRY_COMPARE(spinBox->suffix(), QString());
}

// A custom handler's applyAttributes runs once per actual attribute change of
// the edited property, and its setEditorData does not run for unrelated changes.
void tst_PropertyTreeView::customEditorAppliesAttributesOncePerChange()
{
    struct Counters
    {
        int applied = 0;
        int shown = 0;
    };
    auto counters = std::make_shared<Counters>();
    const TypeId type = QStringLiteral("test.tree.attributes");
    TypeRegistry::global().registerType<int>(type, TypeHandler());
    EditorHandler handler;
    handler.createEditor = [](QWidget* parent, const Property&) { return new QPushButton(parent); };
    handler.setEditorData = [counters](QWidget* editor, const QVariant& value, const Property&) {
        ++counters->shown;
        static_cast<QPushButton*>(editor)->setText(value.toString());
    };
    handler.editorData = [](QWidget* editor, const Property&) {
        return QVariant(static_cast<QPushButton*>(editor)->text().toInt());
    };
    handler.applyAttributes = [counters](QWidget* editor, const Property& property) {
        ++counters->applied;
        editor->setProperty("seen", property.attribute(QStringLiteral("test.key")));
    };
    QVERIFY(EditorFactory::global().registerEditor(type, handler));

    Property& counter = f->model.root()->add(type, QStringLiteral("counter"), 41);
    auto* button = f->edit<QPushButton>(QStringLiteral("counter"));
    QVERIFY(button);
    const int applied = counters->applied;
    const auto settle = [] {
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
    };

    counter.setAttribute(QStringLiteral("test.key"), 1);
    QTRY_COMPARE(counters->applied, applied + 1);
    QCOMPARE(button->property("seen"), QVariant(1));
    counter.setAttribute(QStringLiteral("test.key"), 2);
    QTRY_COMPARE(counters->applied, applied + 2);
    QCOMPARE(button->property("seen"), QVariant(2));
    counter.removeAttribute(QStringLiteral("test.key"));
    QTRY_COMPARE(counters->applied, applied + 3);
    QVERIFY(!button->property("seen").isValid() || button->property("seen").isNull());

    counter.setAttribute(QStringLiteral("test.key"), 2);
    QTRY_COMPARE(counters->applied, applied + 4);

    // Nothing changes for the edited property: not run again.
    const int shown = counters->shown;
    counter.setAttribute(QStringLiteral("test.key"), 2); // unchanged
    settle();
    QCOMPARE(counters->applied, applied + 4);
    counter.setToolTip(QStringLiteral("Tip"));
    counter.setDisplayName(QStringLiteral("Counter"));
    const QModelIndex index = f->value(QStringLiteral("counter"));
    emit f->model.dataChanged(index, index, {Qt::ToolTipRole});
    emit f->model.dataChanged(index, index, {PropertyModel::AttributesRole}); // same attributes
    settle();
    QCOMPARE(counters->applied, applied + 4);
    QCOMPARE(counters->shown, shown); // the editor is not reloaded either

    // Another row's attributes are not the editor's.
    f->model.find(QStringLiteral("Camera/fov"))->setAttribute(Attr::Maximum, 100);
    f->model.find(QStringLiteral("Transform/x"))->setToolTip(QStringLiteral("x"));
    settle();
    QCOMPARE(counters->applied, applied + 4);
    QVERIFY(editorOpen(f->view));
}

// A QTreeView that only uses the delegate gets the same behaviour.
void tst_PropertyTreeView::plainTreeViewEditorFollowsAttributeChanges()
{
    QTreeView view;
    view.setItemDelegate(new PropertyDelegate(&view));
    view.setModel(&f->model);
    view.expandAll();
    view.resize(420, 420);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    view.setCurrentIndex(f->value(QStringLiteral("Camera/fov")));
    view.edit(f->value(QStringLiteral("Camera/fov")));
    QSpinBox* spinBox = nullptr;
    const QList<QSpinBox*> spinBoxes = view.viewport()->findChildren<QSpinBox*>();
    for (QSpinBox* candidate : spinBoxes) {
        if (candidate->isVisible())
            spinBox = candidate;
    }
    QVERIFY(spinBox);
    QCOMPARE(spinBox->maximum(), 170);

    Property* fov = f->model.find(QStringLiteral("Camera/fov"));
    fov->setAttribute(Attr::Maximum, 100);
    QTRY_COMPARE(spinBox->maximum(), 100);
    fov->setAttribute(Attr::Suffix, QStringLiteral(" deg"));
    QTRY_COMPARE(spinBox->suffix(), QStringLiteral(" deg"));
}

void tst_PropertyTreeView::editorThroughFilterProxyFollowsAttributeChanges()
{
    PropertyFilterProxyModel proxy;
    proxy.setSourceModel(&f->model);
    f->view->setModel(&proxy);

    f->view->setFocus();
    f->view->setCurrentIndex(proxy.mapFromSource(f->value(QStringLiteral("Camera/fov"))));
    auto* spinBox = f->editor<QSpinBox>();
    QVERIFY(spinBox);
    QCOMPARE(spinBox->maximum(), 170);

    Property* fov = f->model.find(QStringLiteral("Camera/fov"));
    fov->setAttribute(Attr::Maximum, 100);
    QTRY_COMPARE(spinBox->maximum(), 100);
    fov->setAttribute(Attr::Suffix, QStringLiteral(" deg"));
    QTRY_COMPARE(spinBox->suffix(), QStringLiteral(" deg"));
    QVERIFY(editorOpen(f->view));

    spinBox->setValue(150);
    QTest::keyClick(spinBox, Qt::Key_Return);
    QTRY_COMPARE(f->stored(QStringLiteral("Camera/fov")), QVariant(100));
}

// Text typed into an open editor stays when other data of the row changes.
void tst_PropertyTreeView::typedTextSurvivesUnrelatedChanges()
{
    auto* lineEdit = f->edit<QLineEdit>(QStringLiteral("name"));
    QVERIFY(lineEdit);
    lineEdit->setText(QStringLiteral("Rena"));
    Property* name = f->model.find(QStringLiteral("name"));

    name->setToolTip(QStringLiteral("The name"));
    QCoreApplication::processEvents();
    QCOMPARE(lineEdit->text(), QStringLiteral("Rena"));
    name->setDisplayName(QStringLiteral("Title"));
    QCoreApplication::processEvents();
    QCOMPARE(lineEdit->text(), QStringLiteral("Rena"));
    const QModelIndex index = f->value(QStringLiteral("name"));
    emit f->model.dataChanged(index, index, {Qt::ToolTipRole});
    QCoreApplication::processEvents();
    QCOMPARE(lineEdit->text(), QStringLiteral("Rena"));
    QVERIFY(editorOpen(f->view));

    QTest::keyClick(lineEdit, Qt::Key_Return);
    QTRY_COMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Rena")));
}

// The value of the edited property still reaches the open editor when the
// application changes it, also after an attribute change reloaded the editor.
void tst_PropertyTreeView::openEditorFollowsAnExternalValueChange()
{
    auto* spinBox = f->edit<QSpinBox>(QStringLiteral("Camera/fov"));
    QVERIFY(spinBox);
    Property* fov = f->model.find(QStringLiteral("Camera/fov"));

    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 80));
    QTRY_COMPARE(spinBox->value(), 80);
    fov->setAttribute(Attr::Maximum, 90);
    QTRY_COMPARE(spinBox->maximum(), 90);
    QCOMPARE(spinBox->value(), 80);
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 85));
    QTRY_COMPARE(spinBox->value(), 85);
    // Back to a value written earlier: the editor shows it again.
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 80));
    QTRY_COMPARE(spinBox->value(), 80);
}

// Attribute changes of a row whose editor is closed (committed, cancelled)
// neither crash nor reopen anything.
void tst_PropertyTreeView::attributeChangesAfterTheEditorClosedAreHarmless()
{
    auto* spinBox = f->edit<QSpinBox>(QStringLiteral("Camera/fov"));
    QVERIFY(spinBox);
    QTest::keyClick(spinBox, Qt::Key_Escape);
    QTRY_VERIFY(!editorOpen(f->view));

    Property* fov = f->model.find(QStringLiteral("Camera/fov"));
    fov->setAttribute(Attr::Maximum, 100);
    fov->setAttribute(Attr::Suffix, QStringLiteral(" deg"));
    QCoreApplication::processEvents();
    QVERIFY(!editorOpen(f->view));

    // A new editor starts with the current attributes.
    f->view->setCurrentIndex(f->value(QStringLiteral("name")));
    QTRY_VERIFY(editorOpen(f->view));
    f->view->setCurrentIndex(QModelIndex());
    QTRY_VERIFY(!editorOpen(f->view));
    spinBox = f->edit<QSpinBox>(QStringLiteral("Camera/fov"));
    QVERIFY(spinBox);
    QCOMPARE(spinBox->maximum(), 100);
    QCOMPARE(spinBox->suffix(), QStringLiteral(" deg"));
}

QTEST_MAIN(tst_PropertyTreeView)
#include "tst_propertytreeview.moc"
