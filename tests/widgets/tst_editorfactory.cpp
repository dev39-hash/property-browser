// EditorFactory, built-in editors and EditorDialogScope (see
// qpb/doc/guide/custom-types-and-editors.md).

#include <qpb/qpb.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLocale>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>
#include <QWheelEvent>

#include <memory>

#include "widgets/Int64SpinBox_p.h"
#include "widgets/PathEdit_p.h"
#include "widgets/widgets_p.h"

using namespace qpb;

namespace {

// Creates the editor for property, shows value in it and reads it back.
QVariant roundTrip(QWidget* editor, const Property& property, const QVariant& value)
{
    const EditorHandler* handler = EditorFactory::global().handlerFor(property);
    handler->setEditorData(editor, value, property);
    return handler->editorData(editor, property);
}

} // namespace

class tst_EditorFactory : public QObject
{
    Q_OBJECT

private slots:
    void builtinEditorsAreRegistered();
    void intEditor();
    void int64Editor();
    void doubleEditor();
    void stringEditor();
    void enumEditor();
    void pathEditors();
    void boolEditor();
    void unknownTypeHasNoEditor();
    void editorIdOverridesType();
    void registerAndReplace();
    void dialogScope();

    // Committing without closing, and spin boxes that commit on every step.
    void keepOpenIsANewNameNotAnOverload();
    void keepOpenIgnoresEditorsNoViewOwns();
    void commitOnStepKeyAndSpelling();
    void spinBoxesStepWithoutAViewWhateverCommitOnStepSays_data();
    void spinBoxesStepWithoutAViewWhateverCommitOnStepSays();
    void commitOnStepIsHarmlessForOtherEditors();
    void copiedSpinBoxHandlersAcceptPlainSpinBoxes();

    // Doubles stored without rounding.
    void unroundedDoubleEditorWritesBackTheStoredValue();
    void unroundedDoubleEditorReportsWhatTheUserChanged();
    void unroundedDoubleEditorWithoutAChangeInAnyRange();
    void roundingDoubleEditorIsUnchanged();
};

void tst_EditorFactory::builtinEditorsAreRegistered()
{
    const QList<TypeId> expected {Types::Bool, Types::Int, Types::Double, Types::String,
        Types::Enum, Types::FilePath, Types::DirPath};
    QCOMPARE(EditorFactory::global().editors().mid(0, expected.size()), expected);
    for (const TypeId& id : expected)
        QVERIFY(EditorFactory::global().contains(id));
}

void tst_EditorFactory::intEditor()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->addInt(QStringLiteral("i"), 5)
                      .range(-3, 30)
                      .step(3)
                      .prefix(QStringLiteral("#"))
                      .suffix(QStringLiteral(" px"));
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
    auto* spinBox = qobject_cast<QSpinBox*>(editor.get());
    QVERIFY(spinBox);
    QCOMPARE(spinBox->minimum(), -3);
    QCOMPARE(spinBox->maximum(), 30);
    QCOMPARE(spinBox->singleStep(), 3);
    QCOMPARE(spinBox->prefix(), QStringLiteral("#"));
    QCOMPARE(spinBox->suffix(), QStringLiteral(" px"));
    QVERIFY(!spinBox->keyboardTracking());
    QCOMPARE(roundTrip(spinBox, p, 12), QVariant(12));
}

// Since 1.2: values beyond the range of int.
void tst_EditorFactory::int64Editor()
{
    QVERIFY(EditorFactory::global().contains(Types::Int64));
    const qint64 big = qint64(1) << 40;
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->addInt64(QStringLiteral("n"), 5)
                      .range(-big, big)
                      .step(big / 2)
                      .suffix(QStringLiteral(" B"));
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
    auto* spinBox = qobject_cast<detail::Int64SpinBox*>(editor.get());
    QVERIFY(spinBox);
    QCOMPARE(spinBox->minimum(), -big);
    QCOMPARE(spinBox->maximum(), big);
    QCOMPARE(spinBox->singleStep(), big / 2);
    QCOMPARE(spinBox->suffix(), QStringLiteral(" B"));
    QVERIFY(!spinBox->keyboardTracking());
    QCOMPARE(roundTrip(spinBox, p, big - 1), QVariant::fromValue(big - 1));

    // Steps stop at the limits.
    spinBox->setValue(0);
    spinBox->stepBy(3);
    QCOMPARE(spinBox->value(), big);
    spinBox->stepBy(-5);
    QCOMPARE(spinBox->value(), -big);

    // Typed text is parsed when the value is read (with the suffix, clamped).
    auto* lineEdit = spinBox->findChild<QLineEdit*>();
    lineEdit->setText(QStringLiteral("123456789012 B"));
    QCOMPARE(EditorFactory::global().handlerFor(p)->editorData(spinBox, p),
        QVariant::fromValue(qint64(123456789012)));
    lineEdit->setText(QStringLiteral("99999999999999 B"));
    QCOMPARE(
        EditorFactory::global().handlerFor(p)->editorData(spinBox, p), QVariant::fromValue(big));

    int position = 0;
    QString text = QStringLiteral("12x");
    QCOMPARE(spinBox->validate(text, position), QValidator::Invalid);
    text = QStringLiteral("-");
    QCOMPARE(spinBox->validate(text, position), QValidator::Intermediate);
    text = QStringLiteral("42 B");
    QCOMPARE(spinBox->validate(text, position), QValidator::Acceptable);

    // No limits at all: the full qint64 range without overflow.
    Property& full = root->addInt64(QStringLiteral("full"), 0);
    std::unique_ptr<QWidget> fullEditor(EditorFactory::global().createEditor(nullptr, full));
    auto* fullBox = static_cast<detail::Int64SpinBox*>(fullEditor.get());
    fullBox->setValue(std::numeric_limits<qint64>::max() - 1);
    fullBox->stepBy(10);
    QCOMPARE(fullBox->value(), std::numeric_limits<qint64>::max());
}

void tst_EditorFactory::doubleEditor()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p
        = root->addDouble(QStringLiteral("d"), 0.0).range(-1.0, 1.0).decimals(3).step(0.125);
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
    auto* spinBox = qobject_cast<QDoubleSpinBox*>(editor.get());
    QVERIFY(spinBox);
    QCOMPARE(spinBox->decimals(), 3);
    QCOMPARE(spinBox->minimum(), -1.0);
    QCOMPARE(spinBox->maximum(), 1.0);
    QCOMPARE(spinBox->singleStep(), 0.125);
    QCOMPARE(roundTrip(spinBox, p, 0.25), QVariant(0.25));
}

void tst_EditorFactory::stringEditor()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->addString(QStringLiteral("s"), QString())
                      .maxLength(5)
                      .placeholder(QStringLiteral("name"))
                      .regularExpression(QStringLiteral("[a-z]*"));
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
    auto* lineEdit = qobject_cast<QLineEdit*>(editor.get());
    QVERIFY(lineEdit);
    QCOMPARE(lineEdit->maxLength(), 5);
    QCOMPARE(lineEdit->placeholderText(), QStringLiteral("name"));
    QVERIFY(lineEdit->validator());
    QCOMPARE(roundTrip(lineEdit, p, QStringLiteral("abc")), QVariant(QStringLiteral("abc")));

    // The validator blocks typing text that cannot match.
    lineEdit->clear();
    QTest::keyClicks(lineEdit, QStringLiteral("ab1c"));
    QCOMPARE(lineEdit->text(), QStringLiteral("abc"));
}

void tst_EditorFactory::enumEditor()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->addEnum(QStringLiteral("e"),
        {EnumOption {QStringLiteral("English"), QStringLiteral("en")},
            EnumOption {QStringLiteral("Tiếng Việt"), QStringLiteral("vi")}},
        QStringLiteral("en"));
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
    auto* comboBox = qobject_cast<QComboBox*>(editor.get());
    QVERIFY(comboBox);
    QCOMPARE(comboBox->count(), 2);
    QCOMPARE(comboBox->itemText(1), QStringLiteral("Tiếng Việt"));
    QCOMPARE(roundTrip(comboBox, p, QStringLiteral("vi")), QVariant(QStringLiteral("vi")));
    QCOMPARE(comboBox->currentIndex(), 1);
}

void tst_EditorFactory::pathEditors()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& file = root->addFilePath(QStringLiteral("f"), QString())
                         .filter(QStringLiteral("Text (*.txt)"))
                         .dialogMode(FileMode::Save)
                         .defaultDir(QStringLiteral("/tmp"));
    std::unique_ptr<QWidget> fileEditor(EditorFactory::global().createEditor(nullptr, file));
    auto* filePath = qobject_cast<detail::PathEdit*>(fileEditor.get());
    QVERIFY(filePath);
    QCOMPARE(filePath->kind(), detail::PathEdit::Kind::File);
    QCOMPARE(filePath->fileMode(), FileMode::Save);
    QCOMPARE(filePath->filter(), QStringLiteral("Text (*.txt)"));
    QCOMPARE(filePath->defaultDir(), QStringLiteral("/tmp"));
    QCOMPARE(roundTrip(filePath, file, QStringLiteral("/a/b.txt")),
        QVariant(QStringLiteral("/a/b.txt")));

    Property& dir = root->addDirPath(QStringLiteral("d"), QString());
    std::unique_ptr<QWidget> dirEditor(EditorFactory::global().createEditor(nullptr, dir));
    auto* dirPath = qobject_cast<detail::PathEdit*>(dirEditor.get());
    QVERIFY(dirPath);
    QCOMPARE(dirPath->kind(), detail::PathEdit::Kind::Directory);
}

void tst_EditorFactory::boolEditor()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->addBool(QStringLiteral("b"), false);
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
    QVERIFY(qobject_cast<QCheckBox*>(editor.get()));
    QCOMPARE(roundTrip(editor.get(), p, true), QVariant(true));
}

void tst_EditorFactory::unknownTypeHasNoEditor()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->add(QStringLiteral("test.noeditor"), QStringLiteral("p"), 1);
    QCOMPARE(EditorFactory::global().handlerFor(p), nullptr);
    QCOMPARE(EditorFactory::global().createEditor(nullptr, p), nullptr);
}

void tst_EditorFactory::editorIdOverridesType()
{
    EditorHandler slider = *EditorFactory::global().handler(Types::Int);
    QVERIFY(EditorFactory::global().registerEditor(QStringLiteral("test.intslider"), slider));

    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->addInt(QStringLiteral("i"), 1).editor(QStringLiteral("test.intslider"));
    QCOMPARE(EditorFactory::global().handlerFor(p),
        EditorFactory::global().handler(QStringLiteral("test.intslider")));

    // An editorId that is not registered falls back to the type's editor.
    Property& q = root->addInt(QStringLiteral("q"), 1).editor(QStringLiteral("test.missing"));
    QCOMPARE(EditorFactory::global().handlerFor(q), EditorFactory::global().handler(Types::Int));
}

void tst_EditorFactory::registerAndReplace()
{
    EditorFactory& factory = EditorFactory::global();
    EditorHandler incomplete;
    incomplete.createEditor = [](QWidget* parent, const Property&) { return new QWidget(parent); };
    QVERIFY(!factory.registerEditor(QStringLiteral("test.incomplete"), incomplete));
    QVERIFY(!factory.registerEditor(QString(), *factory.handler(Types::Int)));
    QVERIFY(!factory.registerEditor(Types::Int, *factory.handler(Types::String)));

    const TypeId id = QStringLiteral("test.replaceable");
    QVERIFY(factory.registerEditor(id, *factory.handler(Types::Int)));
    const EditorHandler* pointer = factory.handler(id);
    QVERIFY(factory.replaceEditor(id, *factory.handler(Types::String)));
    QCOMPARE(factory.handler(id), pointer);
    QVERIFY(!factory.replaceEditor(id, incomplete));
    QVERIFY(!factory.replaceEditor(QStringLiteral("test.none"), *factory.handler(Types::Int)));
    QCOMPARE(factory.editors().last(), id);
}

void tst_EditorFactory::dialogScope()
{
    auto editor = std::make_unique<QWidget>();
    auto* child = new QWidget(editor.get());
    QCOMPARE(editor->property("_qpb_dialogDepth").toInt(), 0);
    {
        EditorDialogScope outer(editor.get());
        QCOMPARE(editor->property("_qpb_dialogDepth").toInt(), 1);
        {
            EditorDialogScope inner(child);
            QCOMPARE(child->property("_qpb_dialogDepth").toInt(), 1);
        }
        QCOMPARE(child->property("_qpb_dialogDepth").toInt(), 0);
    }
    QCOMPARE(editor->property("_qpb_dialogDepth").toInt(), 0);

    // The editor may be destroyed while a scope is alive.
    auto* doomed = new QWidget;
    {
        EditorDialogScope scope(doomed);
        delete doomed;
    }
    EditorDialogScope nullScope(nullptr);
}

namespace {

enum class Spin { Int, Int64, Double };
enum class Commit { Unset, Off, On };

// A spin-box property: 500 in 0..1000, step 5.
Property& addSpinProperty(PropertyGroup& group, Spin type, Commit commit)
{
    Property* property = nullptr;
    switch (type) {
    case Spin::Int:
        property = &group.addInt(QStringLiteral("n"), 500).range(0, 1000).step(5).property();
        break;
    case Spin::Int64:
        property = &group.addInt64(QStringLiteral("n"), 500).range(0, 1000).step(5).property();
        break;
    case Spin::Double:
        property = &group.addDouble(QStringLiteral("n"), 500.0).range(0.0, 1000.0).step(5.0).property();
        break;
    }
    if (commit != Commit::Unset)
        property->setAttribute(Attr::CommitOnStep, commit == Commit::On);
    return *property;
}

// A number as the editor of type reports it.
QVariant spinValue(Spin type, int number)
{
    switch (type) {
    case Spin::Int:
        return QVariant(number);
    case Spin::Int64:
        return QVariant::fromValue(qint64(number));
    case Spin::Double:
        break;
    }
    return QVariant(double(number));
}

void wheelOnce(QWidget* widget, int notches)
{
    const QPointF position = QPointF(widget->rect().center());
    QWheelEvent event(position, QPointF(widget->mapToGlobal(position.toPoint())), QPoint(),
        QPoint(0, 120 * notches), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(widget, &event);
}

} // namespace

// notifyCommit keeps its single signature: its address can still be taken
// without a cast, and the new request is a function of its own.
void tst_EditorFactory::keepOpenIsANewNameNotAnOverload()
{
    auto commit = &EditorFactory::notifyCommit; // does not compile if overloaded
    void (*keepOpen)(QWidget*) = &EditorFactory::notifyCommitKeepOpen;
    void (*plain)(QWidget*) = commit;
    QVERIFY(plain);
    QVERIFY(keepOpen);
    QVERIFY(plain != keepOpen);
}

// An editor that no view owns, and no editor at all, are ignored.
void tst_EditorFactory::keepOpenIgnoresEditorsNoViewOwns()
{
    EditorFactory::notifyCommitKeepOpen(nullptr);
    QWidget plain;
    EditorFactory::notifyCommitKeepOpen(&plain);

    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->addInt(QStringLiteral("i"), 5);
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
    auto* spinBox = qobject_cast<QSpinBox*>(editor.get());
    QVERIFY(spinBox);
    spinBox->setValue(7);
    EditorFactory::notifyCommitKeepOpen(spinBox);
    EditorFactory::notifyCommit(spinBox);
    QCOMPARE(spinBox->value(), 7);
    QCOMPARE(p.value(), QVariant(5)); // nothing listens: nothing is written
}

void tst_EditorFactory::commitOnStepKeyAndSpelling()
{
    QCOMPARE(QString(Attr::CommitOnStep), QStringLiteral("commitOnStep"));

    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->addInt(QStringLiteral("i"), 5);
    QVERIFY(!p.attribute(Attr::CommitOnStep).toBool()); // off unless asked for
    p.setAttribute(QStringLiteral("commitOnStep"), true);
    QVERIFY(p.attribute(Attr::CommitOnStep).toBool());
}

void tst_EditorFactory::spinBoxesStepWithoutAViewWhateverCommitOnStepSays_data()
{
    QTest::addColumn<int>("type");
    QTest::addColumn<int>("commit");
    const char* const types[] = {"int", "int64", "double"};
    const char* const modes[] = {"unset", "off", "on"};
    for (int type = 0; type < 3; ++type) {
        for (int mode = 0; mode < 3; ++mode) {
            QTest::newRow(qPrintable(QString::fromLatin1(types[type]) + QLatin1Char('/')
                              + QLatin1String(modes[mode])))
                << type << mode;
        }
    }
}

// Guard (passes without the new attribute): with no view to commit to, steps
// by method, key and wheel just move the value, the attribute set or not.
void tst_EditorFactory::spinBoxesStepWithoutAViewWhateverCommitOnStepSays()
{
    QFETCH(int, type);
    QFETCH(int, commit);
    const Spin spin = Spin(type);
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = addSpinProperty(*root, spin, Commit(commit));
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
    auto* spinBox = qobject_cast<QAbstractSpinBox*>(editor.get());
    QVERIFY(spinBox);
    const EditorHandler* handler = EditorFactory::global().handlerFor(p);
    const auto shown = [&] { return handler->editorData(spinBox, p); };

    handler->setEditorData(spinBox, p.value(), p);
    QCOMPARE(shown(), spinValue(spin, 500));
    spinBox->stepBy(1);
    QCOMPARE(shown(), spinValue(spin, 505));
    spinBox->stepBy(-2);
    QCOMPARE(shown(), spinValue(spin, 495));
    QTest::keyClick(spinBox, Qt::Key_Up);
    QCOMPARE(shown(), spinValue(spin, 500));
    QTest::keyClick(spinBox, Qt::Key_Down);
    QCOMPARE(shown(), spinValue(spin, 495));
    wheelOnce(spinBox, 1);
    QCOMPARE(shown(), spinValue(spin, 500));
    QCOMPARE(p.value(), spinValue(spin, 500)); // the property was never written
    handler->setEditorData(spinBox, spinValue(spin, 500), p);
    QCOMPARE(shown(), spinValue(spin, 500));
}

// The attribute means nothing to editors that are not spin boxes.
void tst_EditorFactory::commitOnStepIsHarmlessForOtherEditors()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    root->addString(QStringLiteral("s"), QStringLiteral("abc"))
        .attribute(Attr::CommitOnStep, true);
    root->addBool(QStringLiteral("b"), true).attribute(Attr::CommitOnStep, true);
    root->addEnum(QStringLiteral("e"), {QStringLiteral("A"), QStringLiteral("B")}, 1)
        .attribute(Attr::CommitOnStep, true);
    root->addFilePath(QStringLiteral("f"), QStringLiteral("/a.txt"))
        .attribute(Attr::CommitOnStep, true);
    for (const QString& id : {QStringLiteral("s"), QStringLiteral("b"), QStringLiteral("e"),
             QStringLiteral("f")}) {
        const Property& p = *root->child(id);
        std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
        QVERIFY2(editor, qPrintable(id));
        QCOMPARE(roundTrip(editor.get(), p, p.value()), p.value());
    }
}

// An unrounded double: the spin box shows the decimals, but when the user
// changed nothing the editor reports the value the property holds.
void tst_EditorFactory::unroundedDoubleEditorWritesBackTheStoredValue()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p
        = root->addDouble(QStringLiteral("d"), 0.0).attribute(Attr::RoundToDecimals, false);
    QVERIFY(p.setValue(0.0125));
    QCOMPARE(p.value(), QVariant(0.0125));
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
    auto* spinBox = qobject_cast<QDoubleSpinBox*>(editor.get());
    QVERIFY(spinBox);
    QCOMPARE(spinBox->decimals(), 2);
    const EditorHandler* handler = EditorFactory::global().handlerFor(p);

    handler->setEditorData(spinBox, p.value(), p);
    QCOMPARE(spinBox->value(), 0.01); // the display is rounded
    QCOMPARE(handler->editorData(spinBox, p), QVariant(0.0125));
    QCOMPARE(handler->editorData(spinBox, p), QVariant(0.0125)); // asking again changes nothing
    QCOMPARE(spinBox->value(), 0.01);

    QVERIFY(p.setValue(1234.5678));
    handler->setEditorData(spinBox, p.value(), p);
    QCOMPARE(spinBox->value(), 1234.57);
    QCOMPARE(handler->editorData(spinBox, p), QVariant(1234.5678));
}

// The user's change wins: a different value, a step, a typed number.
void tst_EditorFactory::unroundedDoubleEditorReportsWhatTheUserChanged()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p
        = root->addDouble(QStringLiteral("d"), 0.0).attribute(Attr::RoundToDecimals, false);
    QVERIFY(p.setValue(0.0125));
    QCOMPARE(p.value(), QVariant(0.0125));
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
    auto* spinBox = qobject_cast<QDoubleSpinBox*>(editor.get());
    QVERIFY(spinBox);
    const EditorHandler* handler = EditorFactory::global().handlerFor(p);
    const auto pristine = [&] { handler->setEditorData(spinBox, p.value(), p); };

    pristine();
    spinBox->setValue(0.5);
    QCOMPARE(handler->editorData(spinBox, p).toDouble(), 0.5);

    // Rounded to the spin box's decimals, since that is what the user chose.
    pristine();
    spinBox->setValue(0.0375);
    QCOMPARE(handler->editorData(spinBox, p).toDouble(), 0.04);

    pristine();
    spinBox->stepBy(1); // 0.01 + the step of 1
    QCOMPARE(handler->editorData(spinBox, p).toDouble(), 1.01);

    pristine();
    auto* lineEdit = spinBox->findChild<QLineEdit*>();
    QVERIFY(lineEdit);
    lineEdit->setText(QLocale().toString(0.3, 'f', 2));
    QCOMPARE(handler->editorData(spinBox, p).toDouble(), 0.3);

    // Showing the stored value again goes back to "nothing changed".
    pristine();
    QCOMPARE(handler->editorData(spinBox, p), QVariant(0.0125));
}

// Whatever the decimals and the value, a spin box that shows the stored value
// rounded and was not changed reports the stored value.
void tst_EditorFactory::unroundedDoubleEditorWithoutAChangeInAnyRange()
{
    struct Case
    {
        double value;
        int decimals;
    };
    const Case cases[] = {{0.0125, 2}, {-0.0125, 2}, {0.001, 2}, {0.0049, 2}, {1234.5678, 2},
        {123456.789, 2}, {1e-9, 2}, {0.0, 2}, {2.4, 0}, {3.7, 0}, {0.01234567, 4},
        {0.012345678, 6}, {12.3456789, 3}, {-7.77777, 1}};
    for (const Case& c : cases) {
        auto root = PropertyGroup::create(QStringLiteral("r"));
        Property& p = root->addDouble(QStringLiteral("d"), 0.0)
                          .decimals(c.decimals)
                          .attribute(Attr::RoundToDecimals, false);
        QVERIFY(p.setValue(c.value));
        QCOMPARE(p.value(), QVariant(c.value));
        std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, p));
        auto* spinBox = qobject_cast<QDoubleSpinBox*>(editor.get());
        QVERIFY(spinBox);
        const EditorHandler* handler = EditorFactory::global().handlerFor(p);
        handler->setEditorData(spinBox, p.value(), p);
        QVERIFY2(handler->editorData(spinBox, p) == QVariant(c.value),
            qPrintable(QStringLiteral("value %1, decimals %2").arg(c.value).arg(c.decimals)));
    }

    // With a range, the stored value sits on its limit.
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& limited = root->addDouble(QStringLiteral("d"), 0.0)
                            .range(-1.0, 0.0125)
                            .attribute(Attr::RoundToDecimals, false);
    QVERIFY(limited.setValue(5.0));
    QCOMPARE(limited.value(), QVariant(0.0125));
    std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, limited));
    auto* spinBox = qobject_cast<QDoubleSpinBox*>(editor.get());
    QVERIFY(spinBox);
    const EditorHandler* handler = EditorFactory::global().handlerFor(limited);
    handler->setEditorData(spinBox, limited.value(), limited);
    QCOMPARE(handler->editorData(spinBox, limited), QVariant(0.0125));
}

// An application may copy a built-in handler and replace only createEditor. The
// copied applyAttributes then meets a plain spin box: it applies the ranges and
// steps; commitOnStep has no effect on a spin box that is not qpb's own.
void tst_EditorFactory::copiedSpinBoxHandlersAcceptPlainSpinBoxes()
{
    QSignalSpy keepOpen(
        detail::CommitNotifier::instance(), &detail::CommitNotifier::commitKeepOpenRequested);
    for (const bool commitOnStep : {true, false}) {
        auto root = PropertyGroup::create(QStringLiteral("r"));
        Property& i = root->addInt(QStringLiteral("i"), 4).range(0, 10).step(2);
        Property& d = root->addDouble(QStringLiteral("d"), 4.0).range(0.0, 10.0).step(2.0);
        i.setAttribute(Attr::CommitOnStep, commitOnStep);
        d.setAttribute(Attr::CommitOnStep, commitOnStep);

        EditorHandler plainInt = *EditorFactory::global().handler(Types::Int);
        plainInt.createEditor
            = [](QWidget* parent, const Property&) { return new QSpinBox(parent); };
        std::unique_ptr<QWidget> intEditor(plainInt.createEditor(nullptr, i));
        plainInt.applyAttributes(intEditor.get(), i);
        auto* intBox = qobject_cast<QSpinBox*>(intEditor.get());
        QVERIFY(intBox);
        QCOMPARE(intBox->minimum(), 0);
        QCOMPARE(intBox->maximum(), 10);
        QCOMPARE(intBox->singleStep(), 2);
        plainInt.setEditorData(intBox, i.value(), i);
        intBox->stepBy(1);
        QCOMPARE(intBox->value(), 6);

        EditorHandler plainDouble = *EditorFactory::global().handler(Types::Double);
        plainDouble.createEditor
            = [](QWidget* parent, const Property&) { return new QDoubleSpinBox(parent); };
        std::unique_ptr<QWidget> doubleEditor(plainDouble.createEditor(nullptr, d));
        plainDouble.applyAttributes(doubleEditor.get(), d);
        auto* doubleBox = qobject_cast<QDoubleSpinBox*>(doubleEditor.get());
        QVERIFY(doubleBox);
        QCOMPARE(doubleBox->maximum(), 10.0);
        QCOMPARE(doubleBox->singleStep(), 2.0);
        plainDouble.setEditorData(doubleBox, d.value(), d);
        doubleBox->stepBy(1);
        QCOMPARE(doubleBox->value(), 6.0);

        // The 64-bit spin box is internal; a copied handler can only be given one.
        auto root64 = PropertyGroup::create(QStringLiteral("r64"));
        Property& n = root64->addInt64(QStringLiteral("n"), 4).range(0, 10).step(2);
        n.setAttribute(Attr::CommitOnStep, commitOnStep);
        EditorHandler copy64 = *EditorFactory::global().handler(Types::Int64);
        std::unique_ptr<QWidget> editor64(copy64.createEditor(nullptr, n));
        copy64.applyAttributes(editor64.get(), n);
        auto* box64 = static_cast<detail::Int64SpinBox*>(editor64.get());
        QCOMPARE(box64->maximum(), qint64(10));
        QCOMPARE(box64->singleStep(), qint64(2));
        // The 64-bit spin box is one of qpb's own: with commitOnStep a step commits.
        copy64.setEditorData(box64, n.value(), n);
        const int before = int(keepOpen.count());
        box64->stepBy(1);
        QCOMPARE(box64->value(), qint64(6));
        QCOMPARE(int(keepOpen.count()) - before, commitOnStep ? 1 : 0);
    }
    // The plain spin boxes never commit a step, whatever the attribute says.
    QCOMPARE(int(keepOpen.count()), 1);
}

// Guard (passes without the new attribute): without the attribute, or with
// true, the editor reports what the spin box shows.
void tst_EditorFactory::roundingDoubleEditorIsUnchanged()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& plain = root->addDouble(QStringLiteral("plain"), 0.0);
    Property& asked
        = root->addDouble(QStringLiteral("asked"), 0.0).attribute(Attr::RoundToDecimals, true);
    for (Property* p : {&plain, &asked}) {
        QVERIFY(p->setValue(0.0125));
        QCOMPARE(p->value(), QVariant(0.01));
        std::unique_ptr<QWidget> editor(EditorFactory::global().createEditor(nullptr, *p));
        auto* spinBox = qobject_cast<QDoubleSpinBox*>(editor.get());
        QVERIFY(spinBox);
        QCOMPARE(roundTrip(spinBox, *p, p->value()), QVariant(0.01));
        QCOMPARE(roundTrip(spinBox, *p, 0.5), QVariant(0.5));
        QCOMPARE(roundTrip(spinBox, *p, 0.0375), QVariant(0.04));
    }
}

QTEST_MAIN(tst_EditorFactory)
#include "tst_editorfactory.moc"
