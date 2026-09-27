// PropertyFormView (docs/PLAN.md M5.1, M5.2, SPEC 5.6).

#include <qpb/qpb.h>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
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
    void commitThatChangesTheTree();
    void typeWithoutEditorIsShownAsText();
    void modelDestroyedFirst();

private:
    std::unique_ptr<Fixture> f;
};

void tst_PropertyFormView::init()
{
    f = std::make_unique<Fixture>();
    QVERIFY(QTest::qWaitForWindowExposed(&f->window));
}

void tst_PropertyFormView::cleanup()
{
    detail::PathEdit::setDialogProviderForTesting({});
    f.reset();
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
        QObject::connect(button, &QPushButton::clicked, button, [button, dialogField] {
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

    // Read-only properties are left alone (D34).
    QVERIFY(f->model.setValue(QStringLiteral("Transform/locked"), 5.0));
    QVERIFY(f->model.setValue(QStringLiteral("Transform/x"), 5.0));
    QVERIFY(resetFromMenu(f->title(QStringLiteral("Transform"))));
    QCOMPARE(f->stored(QStringLiteral("Transform/x")), QVariant(0.0));
    QCOMPARE(f->stored(QStringLiteral("Transform/locked")), QVariant(5.0));
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

QTEST_MAIN(tst_PropertyFormView)
#include "tst_propertyformview.moc"
