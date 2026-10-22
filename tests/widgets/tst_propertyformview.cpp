// PropertyFormView (see qpb/doc/guide/views.md).

#include <qpb/qpb.h>

#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDoubleSpinBox>
#include <QIdentityProxyModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>

#include "widgets/PathEdit_p.h"

using namespace qpb;

namespace {

// Camera
// |-- name          string (validator rejects "forbidden")
// |-- visible       bool
// |-- notes         string, multiline
// |-- Transform
// |   |-- x         double
// |   |-- locked    double, read-only
// |   `-- y         double
// |-- Camera
// |   |-- fov       int, "Field of view"
// |   |-- projection enum
// |   `-- lut       file path
// `-- cacheDir      dir path
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
    root->addString(QStringLiteral("notes"), QString()).multiline();
    PropertyGroup& transform = root->addGroup(QStringLiteral("Transform"));
    transform.addDouble(QStringLiteral("x"), 0.0).range(-100.0, 100.0);
    transform.addDouble(QStringLiteral("locked"), 1.0).readOnly();
    transform.addDouble(QStringLiteral("y"), 0.0);
    PropertyGroup& camera = root->addGroup(QStringLiteral("Camera"));
    camera.addInt(QStringLiteral("fov"), 60)
        .range(10, 170)
        .displayName(QStringLiteral("Field of view"));
    camera.addEnum(QStringLiteral("projection"),
        {QStringLiteral("Perspective"), QStringLiteral("Orthographic")}, 0);
    camera.addFilePath(QStringLiteral("lut"), QString());
    root->addDirPath(QStringLiteral("cacheDir"), QString());
    return root;
}

struct Fixture
{
    Fixture()
        : model(createTree())
    {
        auto* layout = new QVBoxLayout(&window);
        view = new PropertyFormView;
        other = new QLineEdit;
        layout->addWidget(view);
        layout->addWidget(other);
        view->setModel(&model);
        window.resize(420, 640);
        window.show();
    }

    template <class Editor> Editor* editor(const QString& path) const
    {
        return qobject_cast<Editor*>(view->editor(path));
    }

    QVariant stored(const QString& path) const
    {
        return model.find(path)->value();
    }

    // The label of the row whose editor is at path.
    QLabel* label(const QString& path) const
    {
        const QList<QLabel*> labels = view->findChildren<QLabel*>();
        for (QLabel* candidate : labels) {
            if (candidate->buddy() && candidate->buddy() == view->editor(path))
                return candidate;
        }
        return nullptr;
    }

    // The title button of the section of the group at path.
    QToolButton* title(const QString& path) const
    {
        const QString text = model.find(path)->displayName();
        const QList<QToolButton*> buttons = view->findChildren<QToolButton*>();
        for (QToolButton* candidate : buttons) {
            if (candidate->isCheckable() && candidate->text() == text)
                return candidate;
        }
        return nullptr;
    }

    QWidget window;
    PropertyModel model;
    PropertyFormView* view = nullptr;
    QLineEdit* other = nullptr;
};

// A proxy that annotates cells, like an application's layer of marks: the
// foreground and the tool tip of each path are set by the test, everything
// else comes from the source model. It can signal dataChanged for a row, a
// cell or the whole tree without any value having changed.
class MarkProxy : public QIdentityProxyModel
{
public:
    void setForeground(const QString& path, int column, const QVariant& foreground)
    {
        if (foreground.isValid())
            m_foregrounds.insert({path, column}, foreground);
        else
            m_foregrounds.remove({path, column});
    }

    void setToolTip(const QString& path, const QString& toolTip)
    {
        m_toolTips.insert(path, toolTip);
    }

    // dataChanged for the whole row of path (all columns).
    void refresh(const QString& path, const QVector<int>& roles)
    {
        const QModelIndex first = indexOfPath(path, 0);
        Q_ASSERT(first.isValid());
        emit dataChanged(first, first.siblingAtColumn(columnCount(first.parent()) - 1), roles);
    }

    // dataChanged for one cell.
    void refreshCell(const QString& path, int column, const QVector<int>& roles)
    {
        const QModelIndex cell = indexOfPath(path, column);
        Q_ASSERT(cell.isValid());
        emit dataChanged(cell, cell, roles);
    }

    // dataChanged for every row under parent, one signal per level (a proxy
    // that refreshes all its marks).
    void refreshAll(const QVector<int>& roles, const QModelIndex& parent = QModelIndex())
    {
        const int count = rowCount(parent);
        if (count == 0)
            return;
        emit dataChanged(
            index(0, 0, parent), index(count - 1, columnCount(parent) - 1, parent), roles);
        for (int row = 0; row < count; ++row)
            refreshAll(roles, index(row, 0, parent));
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::ForegroundRole || role == Qt::ToolTipRole) {
            const QString path
                = QIdentityProxyModel::data(index.siblingAtColumn(0), PropertyModel::PathRole)
                      .toString();
            if (role == Qt::ForegroundRole) {
                const auto it = m_foregrounds.constFind({path, index.column()});
                if (it != m_foregrounds.constEnd())
                    return it.value();
            } else if (index.column() == 0 && m_toolTips.contains(path)) {
                return m_toolTips.value(path);
            }
        }
        return QIdentityProxyModel::data(index, role);
    }

private:
    QModelIndex indexOfPath(
        const QString& path, int column, const QModelIndex& parent = QModelIndex()) const
    {
        const int count = rowCount(parent);
        for (int row = 0; row < count; ++row) {
            const QModelIndex name = index(row, 0, parent);
            if (QIdentityProxyModel::data(name, PropertyModel::PathRole).toString() == path)
                return name.siblingAtColumn(column);
            const QModelIndex found = indexOfPath(path, column, name);
            if (found.isValid())
                return found;
        }
        return QModelIndex();
    }

    QMap<QPair<QString, int>, QVariant> m_foregrounds;
    QHash<QString, QString> m_toolTips;
};

QVariant brushOf(const QColor& color)
{
    return QVariant::fromValue(QBrush(color));
}

// Opens the context menu of widget and triggers its only action if enabled.
// Returns whether the action was enabled.
bool resetFromMenu(QWidget* widget)
{
    bool enabled = false;
    QTimer::singleShot(0, widget, [&enabled] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu)
            return;
        QAction* action = menu->actions().value(0);
        enabled = action && action->isEnabled();
        if (enabled)
            action->trigger();
        menu->close();
    });
    const QPoint position = widget->rect().center();
    QContextMenuEvent event(QContextMenuEvent::Mouse, position, widget->mapToGlobal(position));
    QApplication::sendEvent(widget, &event);
    return enabled;
}

} // namespace

class tst_PropertyFormView : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void editorsForEveryProperty();
    void enterAndFocusOutCommit();
    void checkBoxAndComboBoxCommitAtOnce();
    void rejectedValueIsRestored();
    void followsValueAndAttributeChanges();
    void visibilityReadOnlyAndEnabled();
    void followsStructureChanges();
    void sectionsCollapse();
    void worksWithFilterProxy();
    void multilineEditing();
    void customEditorWithDialog();
    void contextMenuResets();
    void liveValues();
    void conditions();
    void commitThatChangesTheTree();
    void typeWithoutEditorIsShownAsText();
    void modelDestroyedFirst();
    void styleSheetSelectors();
    void styleSheetRulesApply();
    void boldSurvivesApplicationStyleSheet();
    void normalisedCommitShowsModelValue();
    void dataChangedKeepsFocusAndTypedText_data();
    void dataChangedKeepsFocusAndTypedText();
    void valueChangeAndRevertWithTypedText();
    void dataChangedForRangesAndWholeTree();
    void attributeChangeResyncsAdjustedEditor();

    void foregroundIsOffByDefault();
    void turningOnColoursExistingRowsAtOnce();
    void turningOffRemovesEveryColour();
    void turningOffRestoresApplicationSheet();
    void styleSheetTurnsOnModelForeground();
    void foregroundSurvivesRebuild();
    void collapsedSectionTitleShowsColour();
    void foregroundColoursLabel();
    void foregroundFallsBackToValueColumn();
    void foregroundIgnoresGradientAndTextureBrushes();
    void foregroundKeepsAlpha();
    void noForegroundChangesNothing();
    void clearedForegroundRestoresLabelColour();
    void clearedForegroundRestoresModifiedRule();
    void foregroundColoursSectionTitle();
    void foregroundWinsOverApplicationSheet();
    void disabledLabelKeepsDisabledColour();
    void foregroundIsAppliedOnlyWhenChanged();
    void turningOnWhileRebuildIsPendingColoursEveryRow();
    void queuedPolishAfterTurningOnIsHarmless();

private:
    // Shows the form through a MarkProxy over the fixture's model.
    MarkProxy* useProxy();
    // The same, with the model's foreground colours turned on first.
    MarkProxy* useForegroundProxy();

    std::unique_ptr<Fixture> f;
    std::unique_ptr<MarkProxy> markProxy;
};

void tst_PropertyFormView::init()
{
    f = std::make_unique<Fixture>();
    QVERIFY(QTest::qWaitForWindowExposed(&f->window));
}

void tst_PropertyFormView::cleanup()
{
    qApp->setStyleSheet(QString());
    detail::PathEdit::setDialogProviderForTesting({});
    f.reset();
    markProxy.reset();
}

MarkProxy* tst_PropertyFormView::useProxy()
{
    markProxy = std::make_unique<MarkProxy>();
    markProxy->setSourceModel(&f->model);
    f->view->setModel(markProxy.get());
    return markProxy.get();
}

MarkProxy* tst_PropertyFormView::useForegroundProxy()
{
    f->view->setShowModelForeground(true);
    return useProxy();
}

void tst_PropertyFormView::editorsForEveryProperty()
{
    QCOMPARE(f->view->model(), &f->model);
    QVERIFY(f->editor<QLineEdit>(QStringLiteral("name")));
    QVERIFY(f->editor<QCheckBox>(QStringLiteral("visible")));
    QVERIFY(f->editor<QPlainTextEdit>(QStringLiteral("notes")));
    QVERIFY(f->editor<QDoubleSpinBox>(QStringLiteral("Transform/x")));
    QVERIFY(f->editor<QSpinBox>(QStringLiteral("Camera/fov")));
    QVERIFY(f->editor<QComboBox>(QStringLiteral("Camera/projection")));
    QVERIFY(f->editor<detail::PathEdit>(QStringLiteral("Camera/lut")));
    QVERIFY(f->editor<detail::PathEdit>(QStringLiteral("cacheDir")));
    QVERIFY(!f->view->editor(QStringLiteral("Transform"))); // groups have none
    QVERIFY(!f->view->editor(QStringLiteral("nothing")));

    // Values and labels come from the model; editors are framed in a form.
    QCOMPARE(f->editor<QLineEdit>(QStringLiteral("name"))->text(), QStringLiteral("Main"));
    QVERIFY(f->editor<QLineEdit>(QStringLiteral("name"))->hasFrame());
    QCOMPARE(f->editor<QSpinBox>(QStringLiteral("Camera/fov"))->value(), 60);
    QCOMPARE(f->editor<QSpinBox>(QStringLiteral("Camera/fov"))->maximum(), 170);
    QCOMPARE(f->label(QStringLiteral("Camera/fov"))->text(), QStringLiteral("Field of view"));
    QVERIFY(f->title(QStringLiteral("Transform")));
    QVERIFY(f->title(QStringLiteral("Camera")));
}

void tst_PropertyFormView::enterAndFocusOutCommit()
{
    QSignalSpy changed(&f->model, &PropertyModel::valueChanged);

    auto* name = f->editor<QLineEdit>(QStringLiteral("name"));
    name->setFocus();
    name->setText(QStringLiteral("Rim"));
    QCOMPARE(changed.count(), 0); // nothing until the edit is finished
    QTest::keyClick(name, Qt::Key_Return);
    QTRY_COMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Rim")));

    auto* x = f->editor<QDoubleSpinBox>(QStringLiteral("Transform/x"));
    x->setFocus();
    x->setValue(12.5);
    f->other->setFocus();
    QTRY_COMPARE(f->stored(QStringLiteral("Transform/x")), QVariant(12.5));
    QCOMPARE(changed.count(), 2);

    // Focus moving inside a composite editor does not commit.
    auto* lut = f->editor<detail::PathEdit>(QStringLiteral("Camera/lut"));
    lut->lineEdit()->setFocus();
    lut->lineEdit()->setText(QStringLiteral("/tmp/a.cube"));
    lut->browseButton()->setFocus();
    QCoreApplication::processEvents();
    QCOMPARE(changed.count(), 2);
    f->other->setFocus();
    QTRY_COMPARE(f->stored(QStringLiteral("Camera/lut")), QVariant(QStringLiteral("/tmp/a.cube")));
}

void tst_PropertyFormView::checkBoxAndComboBoxCommitAtOnce()
{
    auto* visible = f->editor<QCheckBox>(QStringLiteral("visible"));
    QTest::mouseClick(visible, Qt::LeftButton, {}, QPoint(6, visible->height() / 2));
    QCOMPARE(f->stored(QStringLiteral("visible")), QVariant(false));

    auto* projection = f->editor<QComboBox>(QStringLiteral("Camera/projection"));
    projection->setFocus();
    QTest::keyClick(projection, Qt::Key_Down); // emits activated
    QCOMPARE(f->stored(QStringLiteral("Camera/projection")), QVariant(1));
}

void tst_PropertyFormView::rejectedValueIsRestored()
{
    QSignalSpy failed(&f->model, &PropertyModel::validationFailed);
    auto* name = f->editor<QLineEdit>(QStringLiteral("name"));
    name->setFocus();
    name->setText(QStringLiteral("forbidden"));
    QTest::keyClick(name, Qt::Key_Return);
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Main")));
    QCOMPARE(name->text(), QStringLiteral("Main"));
    QTRY_COMPARE(QToolTip::text(), QStringLiteral("This name is not allowed"));

    // Escape reverts an edit that was not committed yet.
    name->setText(QStringLiteral("typing"));
    QTest::keyClick(name, Qt::Key_Escape);
    QCOMPARE(name->text(), QStringLiteral("Main"));
}

void tst_PropertyFormView::followsValueAndAttributeChanges()
{
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 90));
    QCOMPARE(f->editor<QSpinBox>(QStringLiteral("Camera/fov"))->value(), 90);
    QVERIFY(f->label(QStringLiteral("Camera/fov"))->font().bold()); // modified
    QVERIFY(!f->label(QStringLiteral("name"))->font().bold());

    QVERIFY(f->model.setValue(QStringLiteral("visible"), false));
    QVERIFY(!f->editor<QCheckBox>(QStringLiteral("visible"))->isChecked());

    Property* fov = f->model.find(QStringLiteral("Camera/fov"));
    fov->setAttribute(Attr::Maximum, 120);
    QCOMPARE(f->editor<QSpinBox>(QStringLiteral("Camera/fov"))->maximum(), 120);
    fov->setDisplayName(QStringLiteral("FOV"));
    QCOMPARE(f->label(QStringLiteral("Camera/fov"))->text(), QStringLiteral("FOV"));
    f->model.find(QStringLiteral("Transform"))->setDisplayName(QStringLiteral("Placement"));
    QVERIFY(f->title(QStringLiteral("Transform")));

    fov->resetToDefault();
    QCOMPARE(f->editor<QSpinBox>(QStringLiteral("Camera/fov"))->value(), 60);
    QVERIFY(!f->label(QStringLiteral("Camera/fov"))->font().bold());
}

void tst_PropertyFormView::visibilityReadOnlyAndEnabled()
{
    QWidget* y = f->view->editor(QStringLiteral("Transform/y"));
    QLabel* yLabel = f->label(QStringLiteral("Transform/y"));
    QVERIFY(y->isVisible());
    f->model.find(QStringLiteral("Transform/y"))->setVisible(false);
    QVERIFY(!y->isVisible());
    QVERIFY(!yLabel->isVisible());
    f->model.find(QStringLiteral("Transform/y"))->setVisible(true);
    QVERIFY(y->isVisible());

    f->model.find(QStringLiteral("Camera"))->setVisible(false);
    QVERIFY(!f->title(QStringLiteral("Camera"))->isVisible());
    QVERIFY(!f->view->editor(QStringLiteral("Camera/fov"))->isVisible());

    // Read-only: the value is shown, the editor cannot be used.
    QVERIFY(!f->view->editor(QStringLiteral("Transform/locked"))->isEnabled());
    QVERIFY(f->label(QStringLiteral("Transform/locked"))->isEnabled());

    // Disabled: label and editor.
    f->model.find(QStringLiteral("Transform/x"))->setEnabled(false);
    QVERIFY(!f->view->editor(QStringLiteral("Transform/x"))->isEnabled());
    QVERIFY(!f->label(QStringLiteral("Transform/x"))->isEnabled());
    f->model.find(QStringLiteral("Transform/x"))->setEnabled(true);
    QVERIFY(f->view->editor(QStringLiteral("Transform/x"))->isEnabled());
}

void tst_PropertyFormView::followsStructureChanges()
{
    PropertyGroup* transform = f->model.find(QStringLiteral("Transform"))->toGroup();
    transform->addDouble(QStringLiteral("z"), 3.0);
    // Queries apply pending changes at once.
    auto* z = f->editor<QDoubleSpinBox>(QStringLiteral("Transform/z"));
    QVERIFY(z);
    QCOMPARE(z->value(), 3.0);

    QPointer<QWidget> y = f->view->editor(QStringLiteral("Transform/y"));
    transform->remove(QStringLiteral("y"));
    QVERIFY(!f->view->editor(QStringLiteral("Transform/y")));
    QTRY_VERIFY(!y); // old widgets are deleted later

    // Changes are also applied without a query, once back in the event loop.
    const auto visibleSpinBoxes = [this] {
        const QList<QSpinBox*> spinBoxes = f->view->findChildren<QSpinBox*>();
        return std::count_if(spinBoxes.begin(), spinBoxes.end(),
            [](const QSpinBox* spinBox) { return spinBox->isVisible(); });
    };
    const auto before = visibleSpinBoxes();
    transform->addInt(QStringLiteral("w"), 1);
    QTRY_COMPARE(visibleSpinBoxes(), before + 1);

    auto root = PropertyGroup::create(QStringLiteral("Other"));
    root->addInt(QStringLiteral("count"), 7);
    f->model.setRoot(std::move(root));
    QCOMPARE(f->editor<QSpinBox>(QStringLiteral("count"))->value(), 7);
    QVERIFY(!f->view->editor(QStringLiteral("name")));

    f->view->setModel(nullptr);
    QVERIFY(!f->view->model());
    QVERIFY(!f->view->editor(QStringLiteral("count")));
}

void tst_PropertyFormView::sectionsCollapse()
{
    QVERIFY(f->view->isExpanded(QStringLiteral("Transform")));
    QWidget* x = f->view->editor(QStringLiteral("Transform/x"));
    QVERIFY(x->isVisible());

    f->view->setExpanded(QStringLiteral("Transform"), false);
    QVERIFY(!f->view->isExpanded(QStringLiteral("Transform")));
    QVERIFY(!x->isVisible());
    QCOMPARE(f->title(QStringLiteral("Transform"))->arrowType(), Qt::RightArrow);

    // Kept across rebuilds.
    f->model.find(QStringLiteral("Transform"))->toGroup()->addInt(QStringLiteral("w"), 0);
    QVERIFY(!f->view->editor(QStringLiteral("Transform/w"))->isVisible());

    // The title button toggles the section.
    QTest::mouseClick(f->title(QStringLiteral("Transform")), Qt::LeftButton);
    QVERIFY(f->view->isExpanded(QStringLiteral("Transform")));
    QVERIFY(f->view->editor(QStringLiteral("Transform/w"))->isVisible());
}

void tst_PropertyFormView::worksWithFilterProxy()
{
    PropertyFilterProxyModel proxy;
    proxy.setSourceModel(&f->model);
    f->view->setModel(&proxy);
    QVERIFY(f->view->editor(QStringLiteral("name")));

    proxy.setFilterFixedString(QStringLiteral("view")); // "Field of view"
    QVERIFY(!f->view->editor(QStringLiteral("name")));
    QVERIFY(!f->view->editor(QStringLiteral("Transform/x")));
    auto* fov = f->editor<QSpinBox>(QStringLiteral("Camera/fov"));
    QVERIFY(fov);

    fov->setFocus();
    fov->setValue(100);
    QTest::keyClick(fov, Qt::Key_Return);
    QTRY_COMPARE(f->stored(QStringLiteral("Camera/fov")), QVariant(100));

    proxy.setFilterFixedString(QString());
    QVERIFY(f->view->editor(QStringLiteral("name")));
}

void tst_PropertyFormView::multilineEditing()
{
    auto* notes = f->editor<QPlainTextEdit>(QStringLiteral("notes"));
    QVERIFY(notes->minimumHeight() > notes->fontMetrics().lineSpacing() * 2);
    notes->setFocus();
    QTest::keyClicks(notes, QStringLiteral("one"));
    QTest::keyClick(notes, Qt::Key_Return); // a new line, no commit
    QTest::keyClicks(notes, QStringLiteral("two"));
    QCoreApplication::processEvents();
    QCOMPARE(f->stored(QStringLiteral("notes")), QVariant(QString()));
    QTest::keyClick(notes, Qt::Key_Return, Qt::ControlModifier);
    QTRY_COMPARE(f->stored(QStringLiteral("notes")), QVariant(QStringLiteral("one\ntwo")));
    QCOMPARE(notes->toPlainText(), QStringLiteral("one\ntwo"));

    QTest::keyClicks(notes, QStringLiteral("!"));
    f->other->setFocus();
    QTRY_COMPARE(f->stored(QStringLiteral("notes")), QVariant(QStringLiteral("one\ntwo!")));
}

// A custom editor that shows a "dialog" (which takes the focus) and then
// commits with notifyCommit().
void tst_PropertyFormView::customEditorWithDialog()
{
    EditorHandler handler;
    QLineEdit* dialogField = f->other;
    handler.createEditor = [dialogField](QWidget* parent, const Property&) {
        auto* button = new QPushButton(parent);
        // Called through the button: MSVC 2019 warns (C4573) on QObject::connect here.
        button->connect(button, &QPushButton::clicked, button, [button, dialogField] {
            {
                EditorDialogScope scope(button);
                dialogField->setFocus(); // the "dialog" steals the focus
                QCoreApplication::processEvents();
                button->setText(QStringLiteral("picked"));
            }
            EditorFactory::notifyCommit(button);
        });
        return button;
    };
    handler.setEditorData = [](QWidget* editor, const QVariant& value, const Property&) {
        static_cast<QPushButton*>(editor)->setText(value.toString());
    };
    handler.editorData = [](QWidget* editor, const Property&) {
        return QVariant(static_cast<QPushButton*>(editor)->text());
    };
    const QString editorId = QStringLiteral("test.form.button");
    if (!EditorFactory::global().contains(editorId))
        QVERIFY(EditorFactory::global().registerEditor(editorId, handler));

    f->model.root()->addString(QStringLiteral("choice"), QStringLiteral("none")).editor(editorId);
    auto* button = f->editor<QPushButton>(QStringLiteral("choice"));
    QVERIFY(button);
    QCOMPARE(button->text(), QStringLiteral("none"));

    QSignalSpy changed(&f->model, &PropertyModel::valueChanged);
    button->setFocus();
    button->click();
    QCOMPARE(changed.count(), 1); // once, from notifyCommit, not from the focus change
    QCOMPARE(f->stored(QStringLiteral("choice")), QVariant(QStringLiteral("picked")));
}

void tst_PropertyFormView::contextMenuResets()
{
    QVERIFY(!resetFromMenu(f->label(QStringLiteral("Camera/fov")))); // not modified

    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 100));
    QVERIFY(f->model.setValue(QStringLiteral("Camera/projection"), 1));
    QVERIFY(resetFromMenu(f->label(QStringLiteral("Camera/fov"))));
    QCOMPARE(f->stored(QStringLiteral("Camera/fov")), QVariant(60));

    QVERIFY(resetFromMenu(f->title(QStringLiteral("Camera")))); // "Reset group"
    QCOMPARE(f->stored(QStringLiteral("Camera/projection")), QVariant(0));
    QCOMPARE(f->editor<QComboBox>(QStringLiteral("Camera/projection"))->currentIndex(), 0);

    // Read-only properties are left alone (the form reacts to the user only).
    QVERIFY(f->model.setValue(QStringLiteral("Transform/locked"), 5.0));
    QVERIFY(f->model.setValue(QStringLiteral("Transform/x"), 5.0));
    QVERIFY(resetFromMenu(f->title(QStringLiteral("Transform"))));
    QCOMPARE(f->stored(QStringLiteral("Transform/x")), QVariant(0.0));
    QCOMPARE(f->stored(QStringLiteral("Transform/locked")), QVariant(5.0));
}

// Since 1.3: live values are never bold, and "Reset group" leaves them alone.
void tst_PropertyFormView::liveValues()
{
    Property* y = f->model.find(QStringLiteral("Transform/y"));
    y->setLive(true);
    QVERIFY(y->setValue(9.0));
    QVERIFY(f->model.setValue(QStringLiteral("Transform/x"), 5.0));
    QVERIFY(!f->label(QStringLiteral("Transform/y"))->font().bold());
    QVERIFY(f->label(QStringLiteral("Transform/x"))->font().bold());
    QCOMPARE(f->editor<QDoubleSpinBox>(QStringLiteral("Transform/y"))->value(), 9.0);

    QVERIFY(!resetFromMenu(f->label(QStringLiteral("Transform/y"))));
    QVERIFY(resetFromMenu(f->title(QStringLiteral("Transform"))));
    QCOMPARE(f->stored(QStringLiteral("Transform/x")), QVariant(0.0));
    QCOMPARE(f->stored(QStringLiteral("Transform/y")), QVariant(9.0));
}

// Since 1.4: the form follows conditions without any code of its own.
void tst_PropertyFormView::conditions()
{
    f->model.find(QStringLiteral("Transform/y"))->setEnabledWhen(QStringLiteral("visible"));
    f->model.find(QStringLiteral("Camera/lut"))
        ->setVisibleWhen(QStringLiteral("Camera/projection"), 1);
    QWidget* y = f->view->editor(QStringLiteral("Transform/y"));
    QWidget* lut = f->view->editor(QStringLiteral("Camera/lut"));
    QVERIFY(y->isEnabled());
    QVERIFY(!lut->isVisible());

    auto* visible = f->editor<QCheckBox>(QStringLiteral("visible"));
    QTest::mouseClick(visible, Qt::LeftButton, {}, QPoint(6, visible->height() / 2));
    QVERIFY(!y->isEnabled());
    QVERIFY(!f->label(QStringLiteral("Transform/y"))->isEnabled());

    auto* projection = f->editor<QComboBox>(QStringLiteral("Camera/projection"));
    projection->setFocus();
    QTest::keyClick(projection, Qt::Key_Down);
    QVERIFY(lut->isVisible());
}

// An application that adds properties in response to an edit: the editor that
// committed is replaced while its handler runs.
void tst_PropertyFormView::commitThatChangesTheTree()
{
    connect(&f->model, &PropertyModel::valueChanged, this, [this](const QString& path) {
        if (path == QStringLiteral("name"))
            f->model.root()->addInt(QStringLiteral("added"), 1);
    });
    auto* name = f->editor<QLineEdit>(QStringLiteral("name"));
    name->setFocus();
    name->setText(QStringLiteral("Rim"));
    QTest::keyClick(name, Qt::Key_Return);
    QTRY_VERIFY(f->view->editor(QStringLiteral("added")));
    QCOMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Rim")));
    // The focus stays on the property that was edited.
    QTRY_COMPARE(QApplication::focusWidget(), f->view->editor(QStringLiteral("name")));
}

void tst_PropertyFormView::typeWithoutEditorIsShownAsText()
{
    const QString typeId = QStringLiteral("test.form.noeditor");
    if (!TypeRegistry::global().contains(typeId)) {
        TypeHandler type;
        type.displayText = [](const QVariant& value, const Property&) {
            return QStringLiteral("<%1>").arg(value.toInt());
        };
        QVERIFY(TypeRegistry::global().registerType<int>(typeId, type));
    }
    f->model.root()->add(typeId, QStringLiteral("opaque"), 4);
    auto* text = f->editor<QLabel>(QStringLiteral("opaque"));
    QVERIFY(text);
    QCOMPARE(text->text(), QStringLiteral("<4>"));
    QVERIFY(f->model.setValue(QStringLiteral("opaque"), 5));
    QCOMPARE(text->text(), QStringLiteral("<5>"));
}

void tst_PropertyFormView::modelDestroyedFirst()
{
    auto model = std::make_unique<PropertyModel>(createTree());
    PropertyFormView view;
    view.setModel(model.get());
    view.show();
    QVERIFY(view.editor(QStringLiteral("name")));
    model.reset();
    QVERIFY(!view.model());
    QVERIFY(!view.editor(QStringLiteral("name")));
}

namespace {

QString part(const QWidget* widget)
{
    return widget ? widget->property("qpbPart").toString() : QString();
}

// Whether widget or one of its ancestors has the style sheet part.
bool inPart(const QWidget* widget, const QString& name)
{
    for (; widget; widget = widget->parentWidget()) {
        if (part(widget) == name)
            return true;
    }
    return false;
}

} // namespace

// Since 1.6 (qpb/doc/guide/style-sheets.md): stable selectors on the form's widgets.
void tst_PropertyFormView::styleSheetSelectors()
{
    QToolButton* title = f->title(QStringLiteral("Transform"));
    QVERIFY(title);
    QCOMPARE(part(title), QStringLiteral("groupTitle"));
    QCOMPARE(part(title->parentWidget()), QStringLiteral("group"));
    QVERIFY(inPart(f->view->editor(QStringLiteral("Transform/x")), QStringLiteral("groupBody")));
    QVERIFY(!inPart(f->view->editor(QStringLiteral("name")), QStringLiteral("group")));

    QLabel* label = f->label(QStringLiteral("Transform/x"));
    QVERIFY(label);
    QCOMPARE(part(label), QStringLiteral("label"));
    QCOMPARE(label->property("qpbModified"), QVariant(false));
    QVERIFY(f->model.setValue(QStringLiteral("Transform/x"), 5.0));
    QCOMPARE(label->property("qpbModified"), QVariant(true));
    QVERIFY(f->model.find(QStringLiteral("Transform/x"))->resetToDefault());
    QCOMPARE(label->property("qpbModified"), QVariant(false));

    auto* lut = f->editor<detail::PathEdit>(QStringLiteral("Camera/lut"));
    QVERIFY(lut);
    QCOMPARE(part(lut->findChild<QToolButton*>()), QStringLiteral("browse"));

    const QString typeId = QStringLiteral("test.form.noeditor");
    if (!TypeRegistry::global().contains(typeId)) {
        TypeHandler type;
        QVERIFY(TypeRegistry::global().registerType<int>(typeId, type));
    }
    f->model.root()->add(typeId, QStringLiteral("opaque"), 4);
    QCOMPARE(part(f->view->editor(QStringLiteral("opaque"))), QStringLiteral("value"));
}

void tst_PropertyFormView::styleSheetRulesApply()
{
    // A style sheet rule for QLabel resets the font of styled labels; the bold font of
    // modified labels (and of group titles) is applied again afterwards, so the
    // "modified" cue stays visible under application themes.
    qApp->setStyleSheet(QStringLiteral("QLabel { color: #e0e4ea; }"
                                       "QLabel[qpbModified=\"true\"] { color: #ff0000; }"
                                       "QToolButton[qpbPart=\"groupTitle\"] { color: #00ff00; }"));
    QLabel* label = f->label(QStringLiteral("Transform/x"));
    QToolButton* title = f->title(QStringLiteral("Transform"));
    QVERIFY(label);
    QVERIFY(title);
    QVERIFY(title->font().bold());
    QCOMPARE(title->palette().color(QPalette::ButtonText), QColor(Qt::green));
    QVERIFY(!label->font().bold());
    QCOMPARE(label->palette().color(QPalette::WindowText), QColor(0xe0, 0xe4, 0xea));

    QVERIFY(f->model.setValue(QStringLiteral("Transform/x"), 5.0));
    QVERIFY(label->font().bold());
    QCOMPARE(label->palette().color(QPalette::WindowText), QColor(Qt::red));

    QVERIFY(f->model.find(QStringLiteral("Transform/x"))->resetToDefault());
    QVERIFY(!label->font().bold());
    QCOMPARE(label->palette().color(QPalette::WindowText), QColor(0xe0, 0xe4, 0xea));
}

// The bold font of modified labels survives an application style sheet when the
// sheet is set first and the form is then built for a model that already has
// modified properties.
void tst_PropertyFormView::boldSurvivesApplicationStyleSheet()
{
    qApp->setStyleSheet(QStringLiteral("QWidget { color: #e0e4ea; } QLabel { color: #e0e4ea; }"));
    PropertyModel model(createTree());
    QVERIFY(model.setValue(QStringLiteral("name"), QStringLiteral("Other")));
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* view = new PropertyFormView;
    layout->addWidget(view);
    view->setModel(&model);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QLabel* name = nullptr;
    QLabel* visible = nullptr;
    for (QLabel* candidate : view->findChildren<QLabel*>()) {
        if (candidate->buddy() == view->editor(QStringLiteral("name")))
            name = candidate;
        if (candidate->buddy() == view->editor(QStringLiteral("visible")))
            visible = candidate;
    }
    QVERIFY(name);
    QVERIFY(visible);
    QTRY_VERIFY(name->font().bold());
    QVERIFY(!visible->font().bold());
    // A sheet set later polishes the widgets again.
    qApp->setStyleSheet(QStringLiteral("QLabel { color: #c0c4ca; }"));
    QTRY_VERIFY(name->font().bold());
    QVERIFY(!visible->font().bold());
}

// An accepted value that the model normalises to what it holds sends no
// signal; the editor still shows the model's value afterwards.
void tst_PropertyFormView::normalisedCommitShowsModelValue()
{
    const QString typeId = QStringLiteral("test.form.trimmed");
    if (!TypeRegistry::global().contains(typeId)) {
        TypeHandler type;
        type.normalize = [](const QVariant& value, const Property&) {
            return QVariant(value.toString().trimmed());
        };
        QVERIFY(TypeRegistry::global().registerType<QString>(typeId, type));
    }
    if (!EditorFactory::global().contains(typeId)) {
        EditorHandler handler;
        handler.createEditor
            = [](QWidget* parent, const Property&) { return new QLineEdit(parent); };
        handler.setEditorData = [](QWidget* editor, const QVariant& value, const Property&) {
            static_cast<QLineEdit*>(editor)->setText(value.toString());
        };
        handler.editorData = [](QWidget* editor, const Property&) {
            return QVariant(static_cast<QLineEdit*>(editor)->text());
        };
        QVERIFY(EditorFactory::global().registerEditor(typeId, handler));
    }
    f->model.root()->add(typeId, QStringLiteral("tag"), QStringLiteral("abc"));
    auto* tag = f->editor<QLineEdit>(QStringLiteral("tag"));
    QVERIFY(tag);
    QCOMPARE(tag->text(), QStringLiteral("abc"));
    QSignalSpy changed(&f->model, &PropertyModel::valueChanged);

    // Enter.
    tag->setFocus();
    tag->setText(QStringLiteral("  abc  "));
    QTest::keyClick(tag, Qt::Key_Return);
    QTRY_COMPARE(tag->text(), QStringLiteral("abc"));
    QCOMPARE(f->stored(QStringLiteral("tag")), QVariant(QStringLiteral("abc")));
    QCOMPARE(changed.count(), 0);

    // Focus out.
    tag->setText(QStringLiteral(" abc"));
    f->other->setFocus();
    QTRY_COMPARE(tag->text(), QStringLiteral("abc"));
    QCOMPARE(changed.count(), 0);

    // A value that does change is committed and shown normalised.
    tag->setFocus();
    tag->setText(QStringLiteral("  xyz "));
    QTest::keyClick(tag, Qt::Key_Return);
    QTRY_COMPARE(f->stored(QStringLiteral("tag")), QVariant(QStringLiteral("xyz")));
    QCOMPARE(tag->text(), QStringLiteral("xyz"));
    QCOMPARE(changed.count(), 1);
}

// A dataChanged that does not change the value must not replace what the user
// is typing, and must not move the focus.
void tst_PropertyFormView::dataChangedKeepsFocusAndTypedText_data()
{
    QTest::addColumn<int>("scope"); // 0: the row with roles, 1: no role list, 2: whole tree
    QTest::newRow("rowWithColourAndToolTipRoles") << 0;
    QTest::newRow("rowWithoutRoleList") << 1;
    QTest::newRow("wholeTreeWithoutRoleList") << 2;
}

void tst_PropertyFormView::dataChangedKeepsFocusAndTypedText()
{
    QFETCH(int, scope);
    MarkProxy* marks = useProxy();
    const auto refresh = [scope](MarkProxy* proxy) {
        if (scope == 0)
            proxy->refresh(QStringLiteral("name"), {Qt::ForegroundRole, Qt::ToolTipRole});
        else if (scope == 1)
            proxy->refresh(QStringLiteral("name"), {});
        else
            proxy->refreshAll({});
    };
    f->window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&f->window));
    auto* name = f->editor<QLineEdit>(QStringLiteral("name"));
    QLabel* label = f->label(QStringLiteral("name"));
    QSignalSpy changed(&f->model, &PropertyModel::valueChanged);

    name->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), name);
    QTest::keyClicks(name, QStringLiteral("abc"));
    QCOMPARE(name->text(), QStringLiteral("Mainabc"));

    const auto expectIntact = [&](const QString& toolTip) {
        QCOMPARE(name->text(), QStringLiteral("Mainabc"));
        QCOMPARE(QApplication::focusWidget(), name);
        QCOMPARE(label->toolTip(), toolTip);
        QCOMPARE(name->toolTip(), toolTip);
        QCOMPARE(changed.count(), 0); // nothing was committed
        QCOMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Main")));
    };

    // Twice, so that annotations already applied are replaced while typing too.
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::red));
    marks->setToolTip(QStringLiteral("name"), QStringLiteral("first"));
    refresh(marks);
    expectIntact(QStringLiteral("first"));
    if (QTest::currentTestFailed())
        return;
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::green));
    marks->setToolTip(QStringLiteral("name"), QStringLiteral("second"));
    refresh(marks);
    expectIntact(QStringLiteral("second"));
    if (QTest::currentTestFailed())
        return;

    // Leaving the editor still commits what was typed.
    f->other->setFocus();
    QTRY_COMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Mainabc")));
}

// A change of the value itself replaces the text, an Escape restores the
// model's value, and a rejected value is replaced too.
void tst_PropertyFormView::valueChangeAndRevertWithTypedText()
{
    MarkProxy* marks = useProxy();
    f->window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&f->window));
    auto* name = f->editor<QLineEdit>(QStringLiteral("name"));
    name->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), name);

    QTest::keyClicks(name, QStringLiteral("abc"));
    QVERIFY(f->model.setValue(QStringLiteral("name"), QStringLiteral("Ext")));
    QCOMPARE(name->text(), QStringLiteral("Ext"));

    // Escape: back to the model's value, even after a refresh in between.
    QTest::keyClicks(name, QStringLiteral("x"));
    marks->refreshAll({});
    QCOMPARE(name->text(), QStringLiteral("Extx"));
    QTest::keyClick(name, Qt::Key_Escape);
    QCOMPARE(name->text(), QStringLiteral("Ext"));
    QTest::keyClicks(name, QStringLiteral("y"));
    marks->refreshAll({});
    QCOMPARE(name->text(), QStringLiteral("Exty"));

    // Rejected: the model's value is shown again, then typing survives a refresh.
    QSignalSpy failed(&f->model, &PropertyModel::validationFailed);
    name->setText(QStringLiteral("forbidden"));
    QTest::keyClick(name, Qt::Key_Return);
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(name->text(), QStringLiteral("Ext"));
    QTest::keyClicks(name, QStringLiteral("z"));
    marks->refreshAll({});
    QCOMPARE(name->text(), QStringLiteral("Extz"));
    QTest::keyClick(name, Qt::Key_Return);
    QTRY_COMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Extz")));
    marks->refreshAll({});
    QCOMPARE(name->text(), QStringLiteral("Extz"));

    // A value changed from outside replaces text typed after the last commit.
    QTest::keyClicks(name, QStringLiteral("!"));
    QVERIFY(f->model.setValue(QStringLiteral("name"), QStringLiteral("Main")));
    QCOMPARE(name->text(), QStringLiteral("Main"));
}

// A proxy that refreshes all its marks, or only one cell, reaches every row,
// groups and nested properties included: labels, section titles and tool tips
// follow, values change only where the model's value changed.
void tst_PropertyFormView::dataChangedForRangesAndWholeTree()
{
    MarkProxy* marks = useProxy();
    f->window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&f->window));
    auto* name = f->editor<QLineEdit>(QStringLiteral("name"));
    auto* fov = f->editor<QSpinBox>(QStringLiteral("Camera/fov"));
    auto* x = f->editor<QDoubleSpinBox>(QStringLiteral("Transform/x"));
    QToolButton* transform = f->title(QStringLiteral("Transform"));
    QVERIFY(name && fov && x && transform);

    // The whole tree: a top-level row, a nested row, a group and a row of
    // another group. The user is typing in two editors meanwhile.
    name->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), name);
    QTest::keyClicks(name, QStringLiteral("abc"));
    x->setValue(7.5);
    marks->setToolTip(QStringLiteral("name"), QStringLiteral("name tip"));
    marks->setToolTip(QStringLiteral("Transform/x"), QStringLiteral("x tip"));
    marks->setToolTip(QStringLiteral("Transform"), QStringLiteral("group tip"));
    marks->setToolTip(QStringLiteral("Camera/fov"), QStringLiteral("range tip"));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(Qt::green));
    marks->refreshAll({});

    QCOMPARE(f->label(QStringLiteral("name"))->toolTip(), QStringLiteral("name tip"));
    QCOMPARE(f->label(QStringLiteral("Transform/x"))->toolTip(), QStringLiteral("x tip"));
    QCOMPARE(transform->toolTip(), QStringLiteral("group tip"));
    QCOMPARE(f->label(QStringLiteral("Camera/fov"))->toolTip(), QStringLiteral("range tip"));
    QCOMPARE(fov->toolTip(), QStringLiteral("range tip"));
    QCOMPARE(name->text(), QStringLiteral("Mainabc"));
    QCOMPARE(x->value(), 7.5);
    QCOMPARE(fov->value(), 60);
    QCOMPARE(QApplication::focusWidget(), name);

    // A range over the value column only reaches the row's label as well.
    marks->setToolTip(QStringLiteral("Transform/x"), QStringLiteral("x tip 2"));
    marks->refreshCell(QStringLiteral("Transform/x"), PropertyModel::ValueColumn, {});
    QCOMPARE(f->label(QStringLiteral("Transform/x"))->toolTip(), QStringLiteral("x tip 2"));
    QCOMPARE(x->toolTip(), QStringLiteral("x tip 2"));
    QCOMPARE(x->value(), 7.5);
    QCOMPARE(name->text(), QStringLiteral("Mainabc"));
    QCOMPARE(f->label(QStringLiteral("name"))->toolTip(), QStringLiteral("name tip"));

    // A value that changed in the model is shown; the other editors are left
    // alone, also when everything is refreshed once more.
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 90));
    marks->refreshAll({});
    QCOMPARE(fov->value(), 90);
    QCOMPARE(x->value(), 7.5);
    QCOMPARE(name->text(), QStringLiteral("Mainabc"));
    QCOMPARE(QApplication::focusWidget(), name);
    QCOMPARE(f->stored(QStringLiteral("name")), QVariant(QStringLiteral("Main")));
    QCOMPARE(f->stored(QStringLiteral("Transform/x")), QVariant(0.0));
}

// An attribute change can make an editor adjust its own value (a narrower
// range clamps a spin box); when the attributes change again the editor shows
// the model's value again, although the model's value itself never changed.
void tst_PropertyFormView::attributeChangeResyncsAdjustedEditor()
{
    const QString path = QStringLiteral("Camera/fov"); // int, range 10..170
    auto* fov = f->editor<QSpinBox>(path);
    QVERIFY(fov);
    QVERIFY(f->model.setValue(path, 90));
    QCOMPARE(fov->value(), 90);

    Property* property = f->model.find(path);
    property->setAttribute(Attr::Maximum, 50);
    QCOMPARE(fov->maximum(), 50);
    QCOMPARE(fov->value(), 50); // clamped by the editor
    QCOMPARE(f->stored(path), QVariant(90)); // the model keeps its value

    property->setAttribute(Attr::Maximum, 200);
    QCOMPARE(fov->maximum(), 200);
    QCOMPARE(fov->value(), 90);
    QCOMPARE(f->stored(path), QVariant(90));
}

namespace {

const QColor kMark(0x00, 0xc0, 0xff);
const QVector<int> kForegroundRole{Qt::ForegroundRole};

QVariant colorOf(const QColor& color)
{
    return QVariant::fromValue(color);
}

QColor labelColor(const QLabel* label)
{
    return label->palette().color(QPalette::WindowText);
}

QColor titleColor(const QToolButton* title)
{
    return title->palette().color(QPalette::ButtonText);
}

// Counts the style changes a widget receives (each setStyleSheet sends one).
class StyleChangeCounter : public QObject
{
public:
    explicit StyleChangeCounter(QWidget* widget)
        : QObject(widget)
    {
        widget->installEventFilter(this);
    }

    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::StyleChange)
            ++count;
        return false;
    }

    int count = 0;
};

// dataChanged for every row under parent of any model, one signal per level.
void emitDataChangedForAll(
    QAbstractItemModel* model, const QVector<int>& roles, const QModelIndex& parent = QModelIndex())
{
    const int count = model->rowCount(parent);
    if (count == 0)
        return;
    emit model->dataChanged(model->index(0, 0, parent),
        model->index(count - 1, model->columnCount(parent) - 1, parent), roles);
    for (int row = 0; row < count; ++row)
        emitDataChangedForAll(model, roles, model->index(row, 0, parent));
}

// Every label and every section title of the form.
QList<QWidget*> labelsAndTitles(const PropertyFormView* view)
{
    QList<QWidget*> widgets;
    const QList<QLabel*> labels = view->findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (part(label) == QLatin1String("label"))
            widgets << label;
    }
    const QList<QToolButton*> buttons = view->findChildren<QToolButton*>();
    for (QToolButton* button : buttons) {
        if (part(button) == QLatin1String("groupTitle"))
            widgets << button;
    }
    return widgets;
}

// The label whose buddy is the editor at path, in any form.
QLabel* labelIn(const PropertyFormView* view, const QString& path)
{
    const QList<QLabel*> labels = view->findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (label->buddy() && label->buddy() == view->editor(path))
            return label;
    }
    return nullptr;
}

const QString kForegroundSheet
    = QStringLiteral("qpb--PropertyFormView { qproperty-showModelForeground: true; }");

} // namespace

// With the property off (the default) the model's colours are ignored: no
// widget is coloured, restyled or even touched, whatever the model gives.
void tst_PropertyFormView::foregroundIsOffByDefault()
{
    QVERIFY(!f->view->showModelForeground());
    const QVariant property = f->view->property("showModelForeground");
    QVERIFY(property.isValid());
    QCOMPARE(property.toBool(), false);

    MarkProxy* marks = useProxy();
    const QColor plainLabel = labelColor(f->label(QStringLiteral("name")));
    const QColor plainTitle = titleColor(f->title(QStringLiteral("Transform")));
    QVERIFY(plainLabel != QColor(Qt::red));
    QVERIFY(plainTitle != kMark);

    // The form is built for a model that colours the name cell of a leaf, the
    // value cell of another leaf and a group.
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::red));
    marks->setForeground(QStringLiteral("Camera/fov"), 1, colorOf(Qt::green));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    f->view->setModel(&f->model);
    f->view->setModel(marks);
    QLabel* name = f->label(QStringLiteral("name"));
    QLabel* fov = f->label(QStringLiteral("Camera/fov"));
    QToolButton* transform = f->title(QStringLiteral("Transform"));
    QVERIFY(name && fov && transform);
    QCOMPARE(labelColor(name), plainLabel);
    QCOMPARE(labelColor(fov), plainLabel);
    QCOMPARE(titleColor(transform), plainTitle);

    // The model's colours change and are announced: nothing is applied.
    const QList<QWidget*> widgets = labelsAndTitles(f->view);
    QCOMPARE(widgets.size(), 12); // 10 leaves and 2 groups
    QList<StyleChangeCounter*> counters;
    for (QWidget* widget : widgets)
        counters << new StyleChangeCounter(widget);
    marks->refreshAll(kForegroundRole);
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::blue));
    marks->setForeground(QStringLiteral("Transform"), 0, colorOf(Qt::yellow));
    marks->refreshAll(kForegroundRole);
    marks->refreshAll({});
    QCOMPARE(labelColor(name), plainLabel);
    QCOMPARE(labelColor(fov), plainLabel);
    QCOMPARE(titleColor(transform), plainTitle);
    for (int i = 0; i < widgets.size(); ++i) {
        QVERIFY(widgets.at(i)->styleSheet().isEmpty());
        QCOMPARE(counters.at(i)->count, 0);
    }
    qDeleteAll(counters);

    // An application sheet colours the labels and titles as it always did.
    qApp->setStyleSheet(QStringLiteral("QLabel[qpbPart=\"label\"] { color: #e0e4ea; }"
                                       "QToolButton[qpbPart=\"groupTitle\"] { color: #00ff00; }"));
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), QColor(0xe0, 0xe4, 0xea));
    QCOMPARE(labelColor(f->label(QStringLiteral("Camera/fov"))), QColor(0xe0, 0xe4, 0xea));
    QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), QColor(Qt::green));
}

// Turning the property on colours the rows that exist, without waiting for a
// change of the model; a colour that changes later follows.
void tst_PropertyFormView::turningOnColoursExistingRowsAtOnce()
{
    MarkProxy* marks = useProxy();
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::red));
    marks->setForeground(QStringLiteral("Camera/fov"), 1, colorOf(Qt::green));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    marks->refreshAll(kForegroundRole);
    const QColor plainLabel = labelColor(f->label(QStringLiteral("visible")));
    const QColor plainTitle = titleColor(f->title(QStringLiteral("Camera")));
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), plainLabel);
    QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), plainTitle);

    f->view->setShowModelForeground(true);
    QVERIFY(f->view->showModelForeground());
    QCOMPARE(f->view->property("showModelForeground").toBool(), true);
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), QColor(Qt::red));
    QCOMPARE(labelColor(f->label(QStringLiteral("Camera/fov"))), QColor(Qt::green));
    QCOMPARE(labelColor(f->label(QStringLiteral("visible"))), plainLabel);
    QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), kMark);
    QCOMPARE(titleColor(f->title(QStringLiteral("Camera"))), plainTitle);

    // A change announced while on is applied.
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::blue));
    marks->refresh(QStringLiteral("name"), kForegroundRole);
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), QColor(Qt::blue));

    // Turning it on again changes nothing in the widgets.
    const QList<QWidget*> widgets = labelsAndTitles(f->view);
    QList<StyleChangeCounter*> counters;
    for (QWidget* widget : widgets)
        counters << new StyleChangeCounter(widget);
    f->view->setShowModelForeground(true);
    for (const StyleChangeCounter* counter : std::as_const(counters))
        QCOMPARE(counter->count, 0);
    qDeleteAll(counters);
}

// Turning the property off removes every colour at once; while it is off,
// colours that change are not applied, and turning it on again shows the
// model's current colours.
void tst_PropertyFormView::turningOffRemovesEveryColour()
{
    MarkProxy* marks = useForegroundProxy();
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 90)); // modified: bold label
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::red));
    marks->setForeground(QStringLiteral("Camera/fov"), 1, colorOf(Qt::green));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    marks->setForeground(QStringLiteral("Camera"), 1, brushOf(Qt::magenta));
    marks->refreshAll(kForegroundRole);
    const QColor plainLabel = labelColor(f->label(QStringLiteral("visible")));
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), QColor(Qt::red));
    QCOMPARE(labelColor(f->label(QStringLiteral("Camera/fov"))), QColor(Qt::green));
    QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), kMark);
    QCOMPARE(titleColor(f->title(QStringLiteral("Camera"))), QColor(Qt::magenta));

    f->view->setShowModelForeground(false);
    QVERIFY(!f->view->showModelForeground());
    const QColor plainOther = titleColor(f->title(QStringLiteral("Transform")));
    QVERIFY(plainOther != kMark);
    QVERIFY(plainOther != QColor(Qt::magenta));
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), plainLabel);
    QCOMPARE(labelColor(f->label(QStringLiteral("Camera/fov"))), plainLabel);
    QCOMPARE(titleColor(f->title(QStringLiteral("Camera"))), plainOther);
    const QList<QWidget*> widgets = labelsAndTitles(f->view);
    QCOMPARE(widgets.size(), 12);
    for (const QWidget* widget : widgets)
        QVERIFY(widget->styleSheet().isEmpty());
    // The bold weight of modified labels and of titles stays.
    QVERIFY(f->label(QStringLiteral("Camera/fov"))->font().bold());
    QVERIFY(f->title(QStringLiteral("Transform"))->font().bold());

    // Off: new colours are not applied, not even when announced.
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::blue));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(Qt::yellow));
    marks->refreshAll(kForegroundRole);
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), plainLabel);
    QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), plainOther);

    // On again: the model's current colours.
    f->view->setShowModelForeground(true);
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), QColor(Qt::blue));
    QCOMPARE(labelColor(f->label(QStringLiteral("Camera/fov"))), QColor(Qt::green));
    QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), QColor(Qt::yellow));
    QCOMPARE(titleColor(f->title(QStringLiteral("Camera"))), QColor(Qt::magenta));

    // Turning it off when it is off changes nothing.
    f->view->setShowModelForeground(false);
    QList<StyleChangeCounter*> counters;
    for (QWidget* widget : labelsAndTitles(f->view))
        counters << new StyleChangeCounter(widget);
    f->view->setShowModelForeground(false);
    for (const StyleChangeCounter* counter : std::as_const(counters))
        QCOMPARE(counter->count, 0);
    qDeleteAll(counters);
}

// Turning the property off hands the colours back to the style and to the
// application's sheet, including the rule for modified labels.
void tst_PropertyFormView::turningOffRestoresApplicationSheet()
{
    qApp->setStyleSheet(QStringLiteral(
        "QLabel[qpbPart=\"label\"] { color: #e0e4ea; }"
        "QLabel[qpbModified=\"true\"] { color: #ff0000; }"
        "QToolButton[qpbPart=\"groupTitle\"], QToolButton[qpbPart=\"groupTitle\"]:checked"
        " { color: #00ff00; }"));
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 90));
    MarkProxy* marks = useForegroundProxy();
    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(kMark));
    marks->setForeground(QStringLiteral("name"), 0, brushOf(kMark));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    marks->refreshAll(kForegroundRole);
    QCOMPARE(labelColor(f->label(QStringLiteral("Camera/fov"))), kMark);
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), kMark);
    QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), kMark);

    f->view->setShowModelForeground(false);
    QCOMPARE(labelColor(f->label(QStringLiteral("Camera/fov"))), QColor(Qt::red));
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), QColor(0xe0, 0xe4, 0xea));
    QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), QColor(Qt::green));
    QTRY_VERIFY(f->label(QStringLiteral("Camera/fov"))->font().bold());
    QTRY_VERIFY(f->title(QStringLiteral("Transform"))->font().bold());
    QVERIFY(!f->label(QStringLiteral("name"))->font().bold());
}

// The sheet form of the property turns the colours on and off, also for a
// form that is created while the sheet is active.
void tst_PropertyFormView::styleSheetTurnsOnModelForeground()
{
    MarkProxy* marks = useProxy();
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::red));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    marks->refreshAll(kForegroundRole);
    const QColor plainLabel = labelColor(f->label(QStringLiteral("name")));
    const QColor plainTitle = titleColor(f->title(QStringLiteral("Transform")));
    QVERIFY(!f->view->showModelForeground());

    qApp->setStyleSheet(kForegroundSheet);
    QVERIFY(f->view->showModelForeground());
    QTRY_COMPARE(labelColor(f->label(QStringLiteral("name"))), QColor(Qt::red));
    QTRY_COMPARE(titleColor(f->title(QStringLiteral("Transform"))), kMark);

    qApp->setStyleSheet(
        QStringLiteral("qpb--PropertyFormView { qproperty-showModelForeground: false; }"));
    QVERIFY(!f->view->showModelForeground());
    QTRY_COMPARE(labelColor(f->label(QStringLiteral("name"))), plainLabel);
    QTRY_COMPARE(titleColor(f->title(QStringLiteral("Transform"))), plainTitle);
    for (const QWidget* widget : labelsAndTitles(f->view))
        QVERIFY(widget->styleSheet().isEmpty());

    // A form created while the sheet is active.
    qApp->setStyleSheet(kForegroundSheet);
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* second = new PropertyFormView;
    layout->addWidget(second);
    second->setModel(marks);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_VERIFY(second->showModelForeground());
    QLabel* name = labelIn(second, QStringLiteral("name"));
    QVERIFY(name);
    QTRY_COMPARE(labelColor(name), QColor(Qt::red));
}

// The colours survive a rebuild of the form: rows inserted or removed, a
// group that gains a row, a model reset; a rebuild with the property off
// builds an uncoloured form.
void tst_PropertyFormView::foregroundSurvivesRebuild()
{
    qApp->setStyleSheet(QStringLiteral("QLabel[qpbPart=\"label\"] { color: #e0e4ea; }"));
    MarkProxy* marks = useForegroundProxy();
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::red));
    marks->setForeground(QStringLiteral("Camera/fov"), 1, colorOf(Qt::green));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    marks->setForeground(QStringLiteral("extra"), 0, brushOf(Qt::blue));
    marks->setForeground(QStringLiteral("Transform/z"), 0, brushOf(Qt::yellow));
    marks->refreshAll(kForegroundRole);
    const QColor sheetLabel(0xe0, 0xe4, 0xea);
    // The colour of a label or title, or an invalid colour while it does not exist.
    const auto labelAt = [this](const QString& path) {
        const QLabel* label = f->label(path);
        return label ? labelColor(label) : QColor();
    };
    const auto titleAt = [this](const QString& path) {
        const QToolButton* title = f->title(path);
        return title ? titleColor(title) : QColor();
    };
    const auto expectColours = [&](bool extra, bool z) {
        QTRY_COMPARE(labelAt(QStringLiteral("name")), QColor(Qt::red));
        QTRY_COMPARE(labelAt(QStringLiteral("Camera/fov")), QColor(Qt::green));
        QTRY_COMPARE(titleAt(QStringLiteral("Transform")), kMark);
        QTRY_COMPARE(labelAt(QStringLiteral("Transform/y")), sheetLabel);
        if (extra)
            QTRY_COMPARE(labelAt(QStringLiteral("extra")), QColor(Qt::blue));
        if (z)
            QTRY_COMPARE(labelAt(QStringLiteral("Transform/z")), QColor(Qt::yellow));
    };
    expectColours(false, false);
    if (QTest::currentTestFailed())
        return;

    // A row inserted at the top level.
    f->model.root()->addInt(QStringLiteral("extra"), 1);
    QVERIFY(f->view->editor(QStringLiteral("extra")));
    expectColours(true, false);
    if (QTest::currentTestFailed())
        return;

    // A row inserted into a group, a row removed.
    f->model.find(QStringLiteral("Transform"))->toGroup()->addDouble(QStringLiteral("z"), 3.0);
    QVERIFY(f->view->editor(QStringLiteral("Transform/z")));
    expectColours(true, true);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(f->model.root()->remove(QStringLiteral("visible")));
    QVERIFY(!f->view->editor(QStringLiteral("visible")));
    expectColours(true, true);
    if (QTest::currentTestFailed())
        return;

    // A row inserted without any query: the rebuild happens back in the
    // event loop. A row removed after it.
    f->model.root()->addInt(QStringLiteral("late"), 2);
    QTRY_COMPARE(labelsAndTitles(f->view).size(), 14);
    QVERIFY(f->model.root()->remove(QStringLiteral("extra")));
    expectColours(false, true);
    if (QTest::currentTestFailed())
        return;

    // A model reset (a new tree with the same paths).
    f->model.setRoot(createTree());
    QVERIFY(f->view->editor(QStringLiteral("name")));
    f->model.find(QStringLiteral("Transform"))->toGroup()->addDouble(QStringLiteral("z"), 3.0);
    QVERIFY(f->view->editor(QStringLiteral("Transform/z")));
    expectColours(false, true);
    if (QTest::currentTestFailed())
        return;

    // Off: a rebuilt form has no colours.
    f->view->setShowModelForeground(false);
    f->model.root()->addInt(QStringLiteral("extra"), 1);
    QVERIFY(f->view->editor(QStringLiteral("extra")));
    QTRY_COMPARE(labelAt(QStringLiteral("extra")), sheetLabel);
    QCOMPARE(labelAt(QStringLiteral("name")), sheetLabel);
    QCOMPARE(labelAt(QStringLiteral("Transform/z")), sheetLabel);
    for (const QWidget* widget : labelsAndTitles(f->view))
        QVERIFY(widget->styleSheet().isEmpty());
}

// A section title shows its group's colour while the section is collapsed:
// when the colour arrives, when the form is built, and after a rebuild.
void tst_PropertyFormView::collapsedSectionTitleShowsColour()
{
    f->view->setExpanded(QStringLiteral("Transform"), false);
    MarkProxy* marks = useForegroundProxy();
    QToolButton* transform = f->title(QStringLiteral("Transform"));
    QToolButton* camera = f->title(QStringLiteral("Camera"));
    QVERIFY(transform && camera);
    QVERIFY(!f->view->isExpanded(QStringLiteral("Transform")));
    QVERIFY(!transform->isChecked());
    QVERIFY(!f->view->editor(QStringLiteral("Transform/x"))->isVisible());
    const QColor plain = titleColor(camera);

    // The colour arrives while collapsed.
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    marks->refresh(QStringLiteral("Transform"), kForegroundRole);
    QCOMPARE(titleColor(transform), kMark);
    QCOMPARE(titleColor(camera), plain);
    QVERIFY(transform->font().bold());

    // A second section collapses: its colour (from the value cell) shows.
    marks->setForeground(QStringLiteral("Camera"), 1, colorOf(Qt::green));
    marks->refreshCell(QStringLiteral("Camera"), 1, kForegroundRole);
    f->view->setExpanded(QStringLiteral("Camera"), false);
    QCOMPARE(titleColor(f->title(QStringLiteral("Camera"))), QColor(Qt::green));

    // A rebuild keeps both sections collapsed and coloured.
    f->model.root()->addInt(QStringLiteral("extra"), 1);
    QVERIFY(f->view->editor(QStringLiteral("extra")));
    transform = f->title(QStringLiteral("Transform"));
    camera = f->title(QStringLiteral("Camera"));
    QVERIFY(transform && camera);
    QVERIFY(!transform->isChecked());
    QVERIFY(!camera->isChecked());
    QTRY_COMPARE(titleColor(transform), kMark);
    QTRY_COMPARE(titleColor(camera), QColor(Qt::green));

    // Expanding and collapsing again does not lose it.
    f->view->setExpanded(QStringLiteral("Transform"), true);
    QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), kMark);
    f->view->setExpanded(QStringLiteral("Transform"), false);
    QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), kMark);
}

// Foreground colours of the model reach the labels.
void tst_PropertyFormView::foregroundColoursLabel()
{
    MarkProxy* marks = useForegroundProxy();
    QLabel* fov = f->label(QStringLiteral("Camera/fov"));
    QLabel* name = f->label(QStringLiteral("name"));
    QLabel* visible = f->label(QStringLiteral("visible"));
    QVERIFY(fov && name && visible);
    const QColor plain = labelColor(name);
    QVERIFY(plain != QColor(Qt::red));
    QCOMPARE(labelColor(fov), plain);

    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(Qt::red));
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), QColor(Qt::red));
    QCOMPARE(labelColor(name), plain);
    QCOMPARE(labelColor(visible), plain);

    // A QColor works like a brush.
    marks->setForeground(QStringLiteral("visible"), 0, colorOf(Qt::green));
    marks->refresh(QStringLiteral("visible"), kForegroundRole);
    QCOMPARE(labelColor(visible), QColor(Qt::green));
    QCOMPARE(labelColor(fov), QColor(Qt::red));

    // Values that hold no colour: no brush, an invalid colour, other types.
    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(Qt::red));
    marks->setForeground(QStringLiteral("visible"), 0, colorOf(Qt::green));
    marks->setForeground(QStringLiteral("name"), 0, QVariant::fromValue(QBrush(Qt::NoBrush)));
    marks->refresh(QStringLiteral("name"), kForegroundRole);
    QCOMPARE(labelColor(name), plain);
    marks->setForeground(QStringLiteral("Camera/fov"), 0, QVariant::fromValue(QBrush()));
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), plain);
    marks->setForeground(QStringLiteral("visible"), 0, QVariant::fromValue(QColor()));
    marks->refresh(QStringLiteral("visible"), kForegroundRole);
    QCOMPARE(labelColor(visible), plain);
    marks->setForeground(QStringLiteral("name"), 0, QVariant(5));
    marks->refresh(QStringLiteral("name"), kForegroundRole);
    QCOMPARE(labelColor(name), plain);
    marks->setForeground(QStringLiteral("name"), 0, QVariant(QStringLiteral("red")));
    marks->refresh(QStringLiteral("name"), kForegroundRole);
    QCOMPARE(labelColor(name), plain);
}

void tst_PropertyFormView::foregroundFallsBackToValueColumn()
{
    MarkProxy* marks = useForegroundProxy();
    QLabel* fov = f->label(QStringLiteral("Camera/fov"));
    QLabel* name = f->label(QStringLiteral("name"));
    const QColor plain = labelColor(name);

    // Only the value cell is coloured: its dataChanged names the value column.
    marks->setForeground(QStringLiteral("Camera/fov"), 1, brushOf(Qt::red));
    marks->refreshCell(QStringLiteral("Camera/fov"), 1, kForegroundRole);
    QCOMPARE(labelColor(fov), QColor(Qt::red));
    QCOMPARE(labelColor(name), plain);

    // The name cell wins when both are coloured.
    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(Qt::blue));
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), QColor(Qt::blue));

    // A name cell without a colour does not hide the value cell's colour.
    marks->setForeground(
        QStringLiteral("Camera/fov"), 0, QVariant::fromValue(QBrush(Qt::NoBrush)));
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), QColor(Qt::red));

    // A QColor on the value cell.
    marks->setForeground(QStringLiteral("Camera/fov"), 0, QVariant());
    marks->setForeground(QStringLiteral("Camera/fov"), 1, colorOf(Qt::green));
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), QColor(Qt::green));

    marks->setForeground(QStringLiteral("Camera/fov"), 1, QVariant());
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), plain);
}

// Gradient and texture brushes have no single colour: the style's colour stays.
void tst_PropertyFormView::foregroundIgnoresGradientAndTextureBrushes()
{
    MarkProxy* marks = useForegroundProxy();
    QLabel* name = f->label(QStringLiteral("name"));
    QToolButton* transform = f->title(QStringLiteral("Transform"));
    const QColor plainLabel = labelColor(name);
    const QColor plainTitle = titleColor(transform);

    QPixmap pixmap(4, 4);
    pixmap.fill(Qt::red);
    const QList<QBrush> brushes{QBrush(QLinearGradient(0, 0, 1, 0)),
        QBrush(QRadialGradient(0, 0, 1)), QBrush(QConicalGradient(0, 0, 0)), QBrush(pixmap)};
    for (const QBrush& brush : brushes) {
        marks->setForeground(QStringLiteral("name"), 0, QVariant::fromValue(brush));
        marks->setForeground(QStringLiteral("Transform"), 0, QVariant::fromValue(brush));
        marks->refreshAll(kForegroundRole);
        QCOMPARE(labelColor(name), plainLabel);
        QVERIFY(name->styleSheet().isEmpty());
        QCOMPARE(titleColor(transform), plainTitle);
        QVERIFY(transform->styleSheet().isEmpty());
    }

    // Such a name cell holds no colour, so the value cell's colour is used.
    marks->setForeground(
        QStringLiteral("name"), 0, QVariant::fromValue(QBrush(QLinearGradient(0, 0, 1, 0))));
    marks->setForeground(QStringLiteral("name"), 1, brushOf(Qt::red));
    marks->refreshAll(kForegroundRole);
    QCOMPARE(labelColor(name), QColor(Qt::red));
}

void tst_PropertyFormView::foregroundKeepsAlpha()
{
    MarkProxy* marks = useForegroundProxy();
    QLabel* name = f->label(QStringLiteral("name"));
    const QColor translucent(10, 20, 30, 128);
    marks->setForeground(QStringLiteral("name"), 0, brushOf(translucent));
    marks->refresh(QStringLiteral("name"), kForegroundRole);
    QCOMPARE(labelColor(name), translucent);
    marks->setForeground(QStringLiteral("name"), 0, colorOf(QColor(255, 128, 0, 255)));
    marks->refresh(QStringLiteral("name"), kForegroundRole);
    QCOMPARE(labelColor(name), QColor(255, 128, 0));
}

// A model that gives no colours (the plain model, or a proxy without marks)
// leaves every label and title alone, whatever dataChanged says, also with the
// property on.
void tst_PropertyFormView::noForegroundChangesNothing()
{
    f->view->setShowModelForeground(true);
    for (const bool proxied : {false, true}) {
        QAbstractItemModel* model = &f->model;
        if (proxied)
            model = useProxy();
        f->view->setModel(model);

        QList<StyleChangeCounter*> counters;
        const QList<QWidget*> widgets = labelsAndTitles(f->view);
        QCOMPARE(widgets.size(), 12); // 10 leaves and 2 groups
        for (QWidget* widget : widgets)
            counters << new StyleChangeCounter(widget);

        emitDataChangedForAll(model, {});
        emitDataChangedForAll(model, kForegroundRole);
        if (proxied)
            static_cast<MarkProxy*>(model)->refreshAll({});

        for (int i = 0; i < widgets.size(); ++i) {
            QVERIFY(widgets.at(i)->styleSheet().isEmpty());
            QCOMPARE(counters.at(i)->count, 0);
        }
        qDeleteAll(counters);
    }
}

void tst_PropertyFormView::clearedForegroundRestoresLabelColour()
{
    MarkProxy* marks = useForegroundProxy();
    QLabel* fov = f->label(QStringLiteral("Camera/fov"));
    const QColor plain = labelColor(fov);

    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(kMark));
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), kMark);
    marks->setForeground(QStringLiteral("Camera/fov"), 0, QVariant());
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), plain);
    QVERIFY(fov->styleSheet().isEmpty());

    // With an application sheet the sheet's colour is back.
    qApp->setStyleSheet(QStringLiteral("QLabel { color: #e0e4ea; }"));
    QCOMPARE(labelColor(fov), QColor(0xe0, 0xe4, 0xea));
    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(kMark));
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), kMark);
    marks->setForeground(QStringLiteral("Camera/fov"), 0, QVariant());
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), QColor(0xe0, 0xe4, 0xea));
}

void tst_PropertyFormView::clearedForegroundRestoresModifiedRule()
{
    qApp->setStyleSheet(QStringLiteral("QLabel { color: #e0e4ea; }"
                                       "QLabel[qpbModified=\"true\"] { color: #ff0000; }"));
    MarkProxy* marks = useForegroundProxy();
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 90));
    QLabel* fov = f->label(QStringLiteral("Camera/fov"));
    QCOMPARE(labelColor(fov), QColor(Qt::red));

    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(kMark));
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), kMark);
    QVERIFY(fov->font().bold());
    marks->setForeground(QStringLiteral("Camera/fov"), 0, QVariant());
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), QColor(Qt::red));
    QVERIFY(fov->font().bold());

    // Reset while coloured: the label is no longer modified, the colour stays.
    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(kMark));
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QVERIFY(f->model.find(QStringLiteral("Camera/fov"))->resetToDefault());
    QCOMPARE(labelColor(fov), kMark);
    QVERIFY(!fov->font().bold());
    marks->setForeground(QStringLiteral("Camera/fov"), 0, QVariant());
    marks->refresh(QStringLiteral("Camera/fov"), kForegroundRole);
    QCOMPARE(labelColor(fov), QColor(0xe0, 0xe4, 0xea));
}

void tst_PropertyFormView::foregroundColoursSectionTitle()
{
    MarkProxy* marks = useForegroundProxy();
    QToolButton* transform = f->title(QStringLiteral("Transform"));
    QToolButton* camera = f->title(QStringLiteral("Camera"));
    QVERIFY(transform && camera);
    const QColor plain = titleColor(transform);
    QVERIFY(plain != QColor(Qt::red));

    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(Qt::red));
    marks->refresh(QStringLiteral("Transform"), kForegroundRole);
    QCOMPARE(titleColor(transform), QColor(Qt::red));
    QCOMPARE(titleColor(camera), plain);
    QVERIFY(transform->font().bold());

    // A collapsed section keeps its colour and follows changes.
    f->view->setExpanded(QStringLiteral("Transform"), false);
    QCOMPARE(titleColor(transform), QColor(Qt::red));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    marks->refresh(QStringLiteral("Transform"), kForegroundRole);
    QCOMPARE(titleColor(transform), kMark);
    f->view->setExpanded(QStringLiteral("Transform"), true);
    QCOMPARE(titleColor(transform), kMark);

    // Cleared: back to the style's colour.
    f->view->setExpanded(QStringLiteral("Transform"), false);
    marks->setForeground(QStringLiteral("Transform"), 0, QVariant());
    marks->refresh(QStringLiteral("Transform"), kForegroundRole);
    QCOMPARE(titleColor(transform), plain);
    QVERIFY(transform->styleSheet().isEmpty());
    QVERIFY(transform->font().bold());

    // The value cell is the fallback, as for a leaf; a QColor works too.
    marks->setForeground(QStringLiteral("Camera"), 1, colorOf(Qt::green));
    marks->refreshCell(QStringLiteral("Camera"), 1, kForegroundRole);
    QCOMPARE(titleColor(camera), QColor(Qt::green));
}

// An application sheet colours labels, modified labels and group titles; the
// model's colour wins over all three, in either order of the two.
void tst_PropertyFormView::foregroundWinsOverApplicationSheet()
{
    const QString sheet = QStringLiteral(
        "QLabel[qpbPart=\"label\"] { color: #e0e4ea; }"
        "QLabel[qpbModified=\"true\"] { color: #ff0000; }"
        "QToolButton[qpbPart=\"groupTitle\"], QToolButton[qpbPart=\"groupTitle\"]:checked"
        " { color: #00ff00; }");
    QVERIFY(f->model.setValue(QStringLiteral("Camera/fov"), 90));

    const auto check = [this](const QColor& plainLabel, const QColor& modifiedLabel,
                           const QColor& plainTitle, bool marked) {
        QLabel* fov = f->label(QStringLiteral("Camera/fov")); // modified, marked
        QLabel* name = f->label(QStringLiteral("name")); // marked
        QLabel* visible = f->label(QStringLiteral("visible")); // never marked
        QToolButton* transform = f->title(QStringLiteral("Transform")); // marked
        QToolButton* camera = f->title(QStringLiteral("Camera")); // never marked
        QVERIFY(fov && name && visible && transform && camera);
        QTRY_VERIFY(fov->font().bold());
        QTRY_VERIFY(transform->font().bold());
        QVERIFY(camera->font().bold());
        QVERIFY(!name->font().bold());
        QVERIFY(!visible->font().bold());
        QCOMPARE(labelColor(fov), marked ? kMark : modifiedLabel);
        QCOMPARE(labelColor(name), marked ? kMark : plainLabel);
        QCOMPARE(titleColor(transform), marked ? kMark : plainTitle);
        QCOMPARE(labelColor(visible), plainLabel);
        QCOMPARE(titleColor(camera), plainTitle);
    };
    const QColor plainLabel(0xe0, 0xe4, 0xea);
    const QColor modifiedLabel(0xff, 0x00, 0x00);
    const QColor plainTitle(0x00, 0xff, 0x00);

    // The sheet is set first and the view is built for a coloured model.
    f->view->setShowModelForeground(true);
    qApp->setStyleSheet(sheet);
    markProxy = std::make_unique<MarkProxy>();
    MarkProxy* marks = markProxy.get();
    marks->setSourceModel(&f->model);
    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(kMark));
    marks->setForeground(QStringLiteral("name"), 0, brushOf(kMark));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    f->view->setModel(marks);
    check(plainLabel, modifiedLabel, plainTitle, true);
    if (QTest::currentTestFailed())
        return;

    // Clearing the colours brings the sheet back.
    marks->setForeground(QStringLiteral("Camera/fov"), 0, QVariant());
    marks->setForeground(QStringLiteral("name"), 0, QVariant());
    marks->setForeground(QStringLiteral("Transform"), 0, QVariant());
    marks->refreshAll(kForegroundRole);
    check(plainLabel, modifiedLabel, plainTitle, false);
    if (QTest::currentTestFailed())
        return;

    // The colours are applied first, the sheet is set (or replaced) later.
    qApp->setStyleSheet(QString());
    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(kMark));
    marks->setForeground(QStringLiteral("name"), 0, brushOf(kMark));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    marks->refreshAll(kForegroundRole);
    qApp->setStyleSheet(sheet);
    check(plainLabel, modifiedLabel, plainTitle, true);
    if (QTest::currentTestFailed())
        return;
    QString replaced = sheet;
    replaced.replace(QStringLiteral("#e0e4ea"), QStringLiteral("#123456"));
    qApp->setStyleSheet(replaced);
    check(QColor(0x12, 0x34, 0x56), modifiedLabel, plainTitle, true);
}

void tst_PropertyFormView::disabledLabelKeepsDisabledColour()
{
    Property* x = f->model.find(QStringLiteral("Transform/x"));
    x->setEnabled(false);
    MarkProxy* marks = useForegroundProxy();
    QLabel* label = f->label(QStringLiteral("Transform/x"));
    QVERIFY(!label->isEnabled());
    const QColor disabled = label->palette().color(QPalette::Disabled, QPalette::WindowText);

    marks->setForeground(QStringLiteral("Transform/x"), 0, brushOf(kMark));
    marks->refresh(QStringLiteral("Transform/x"), kForegroundRole);
    QVERIFY(disabled != kMark);
    QCOMPARE(label->palette().color(QPalette::Disabled, QPalette::WindowText), disabled);

    // Enabled again: the colour shows.
    x->setEnabled(true);
    QVERIFY(label->isEnabled());
    QCOMPARE(labelColor(label), kMark);
    x->setEnabled(false);
    QCOMPARE(label->palette().color(QPalette::Disabled, QPalette::WindowText), disabled);
}

void tst_PropertyFormView::foregroundIsAppliedOnlyWhenChanged()
{
    MarkProxy* marks = useForegroundProxy();
    QLabel* fov = f->label(QStringLiteral("Camera/fov"));
    QToolButton* transform = f->title(QStringLiteral("Transform"));
    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(Qt::red));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(Qt::red));
    marks->refreshAll(kForegroundRole);
    QCOMPARE(labelColor(fov), QColor(Qt::red));

    StyleChangeCounter labelChanges(fov);
    StyleChangeCounter titleChanges(transform);
    marks->setToolTip(QStringLiteral("Camera/fov"), QStringLiteral("tip"));
    marks->refreshAll(kForegroundRole);
    marks->refreshAll({});
    marks->refresh(QStringLiteral("Camera/fov"), {Qt::ToolTipRole});
    QCOMPARE(fov->toolTip(), QStringLiteral("tip"));
    QCOMPARE(labelChanges.count, 0);
    QCOMPARE(titleChanges.count, 0);

    // A different colour is applied, a QColor equal to the brush's is not.
    marks->setForeground(QStringLiteral("Camera/fov"), 0, brushOf(Qt::blue));
    marks->refreshAll(kForegroundRole);
    QCOMPARE(labelColor(fov), QColor(Qt::blue));
    QVERIFY(labelChanges.count > 0);
    const int applied = labelChanges.count;
    marks->setForeground(QStringLiteral("Camera/fov"), 0, colorOf(Qt::blue));
    marks->refreshAll(kForegroundRole);
    QCOMPARE(labelChanges.count, applied);

    // Cleared once, then nothing more to clear.
    marks->setForeground(QStringLiteral("Camera/fov"), 0, QVariant());
    marks->refreshAll(kForegroundRole);
    QVERIFY(labelChanges.count > applied);
    const int cleared = labelChanges.count;
    marks->refreshAll(kForegroundRole);
    QCOMPARE(labelChanges.count, cleared);
}

namespace {

// Counts the warnings logged while it lives. The platform plugin's size-hint
// notice is not a warning of the form and is left out.
class WarningCounter
{
public:
    WarningCounter() { s_previous = qInstallMessageHandler(&handle); s_count = 0; }
    ~WarningCounter() { qInstallMessageHandler(s_previous); }
    int count() const { return s_count; }

private:
    static void handle(QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        if ((type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)
            && !message.contains(QLatin1String("propagateSizeHints")))
            ++s_count;
        if (s_previous)
            s_previous(type, context, message);
    }

    static inline QtMessageHandler s_previous = nullptr;
    static inline int s_count = 0;
};

// Lets the timers and posted events that are already queued run; it is not a
// way to wait for a result.
void drainEventLoop()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

} // namespace

// Turning the property on while a structural change has not been applied yet:
// the pending rebuild reads the flag, so the new row and the old rows are all
// coloured once control returns to the event loop.
void tst_PropertyFormView::turningOnWhileRebuildIsPendingColoursEveryRow()
{
    qApp->setStyleSheet(QStringLiteral("QLabel[qpbPart=\"label\"] { color: #e0e4ea; }"));
    MarkProxy* marks = useProxy();
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::red));
    marks->setForeground(QStringLiteral("Camera/fov"), 1, colorOf(Qt::green));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    marks->setForeground(QStringLiteral("extra"), 0, brushOf(Qt::blue));
    const QColor sheetLabel(0xe0, 0xe4, 0xea);
    QCOMPARE(labelColor(f->label(QStringLiteral("name"))), sheetLabel);
    QCOMPARE(labelsAndTitles(f->view).size(), 12);

    // A row is added and the property is turned on before the rebuild runs.
    f->model.root()->addInt(QStringLiteral("extra"), 1);
    QCOMPARE(labelsAndTitles(f->view).size(), 12); // not rebuilt yet
    f->view->setShowModelForeground(true);
    QVERIFY(f->view->showModelForeground());

    QTRY_COMPARE(labelsAndTitles(f->view).size(), 13);
    QTRY_COMPARE(labelColor(f->label(QStringLiteral("extra"))), QColor(Qt::blue));
    QTRY_COMPARE(labelColor(f->label(QStringLiteral("name"))), QColor(Qt::red));
    QTRY_COMPARE(labelColor(f->label(QStringLiteral("Camera/fov"))), QColor(Qt::green));
    QTRY_COMPARE(titleColor(f->title(QStringLiteral("Transform"))), kMark);
    QCOMPARE(labelColor(f->label(QStringLiteral("visible"))), sheetLabel);

    // The same with a row removed, turning the property off again.
    QVERIFY(f->model.root()->remove(QStringLiteral("visible")));
    f->view->setShowModelForeground(false);
    QTRY_COMPARE(labelsAndTitles(f->view).size(), 12);
    QTRY_COMPARE(labelColor(f->label(QStringLiteral("name"))), sheetLabel);
    for (const QWidget* widget : labelsAndTitles(f->view))
        QVERIFY(widget->styleSheet().isEmpty());
}

// The re-polish that follows turning the property on runs later from the event
// loop; it does nothing harmful when the form was rebuilt or deleted first.
void tst_PropertyFormView::queuedPolishAfterTurningOnIsHarmless()
{
    MarkProxy* marks = useProxy();
    marks->setForeground(QStringLiteral("name"), 0, brushOf(Qt::red));
    marks->setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
    marks->setForeground(QStringLiteral("extra"), 0, brushOf(Qt::blue));

    // Rebuilt before the queued polish runs.
    {
        WarningCounter warnings;
        f->view->setShowModelForeground(true); // queues the polish
        f->model.root()->addInt(QStringLiteral("extra"), 1);
        QVERIFY(f->view->editor(QStringLiteral("extra"))); // rebuilds now
        drainEventLoop();
        QTRY_COMPARE(labelColor(f->label(QStringLiteral("extra"))), QColor(Qt::blue));
        QCOMPARE(labelColor(f->label(QStringLiteral("name"))), QColor(Qt::red));
        QCOMPARE(titleColor(f->title(QStringLiteral("Transform"))), kMark);
        QCOMPARE(labelsAndTitles(f->view).size(), 13);
        QCOMPARE(warnings.count(), 0);
    }

    // Deleted before the queued polish runs.
    {
        WarningCounter warnings;
        PropertyModel model(createTree());
        MarkProxy proxy;
        proxy.setSourceModel(&model);
        proxy.setForeground(QStringLiteral("name"), 0, brushOf(Qt::red));
        proxy.setForeground(QStringLiteral("Transform"), 0, brushOf(kMark));
        auto* second = new PropertyFormView;
        QPointer<PropertyFormView> guard(second);
        second->setModel(&proxy);
        QVERIFY(second->editor(QStringLiteral("name")));
        second->setShowModelForeground(true); // queues the polish
        // The row holds a colour, so the queued polish has work to do.
        QVERIFY(!labelIn(second, QStringLiteral("name"))->styleSheet().isEmpty());
        delete second;
        QVERIFY(guard.isNull());
        drainEventLoop();
        QCOMPARE(warnings.count(), 0);
    }
}

QTEST_MAIN(tst_PropertyFormView)
#include "tst_propertyformview.moc"
