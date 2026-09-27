// EditorFactory, built-in editors and EditorDialogScope (docs/PLAN.md M3.1, SPEC §5.1-5.3).

#include <qpb/qpb.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTest>

#include <memory>

#include "widgets/Int64SpinBox_p.h"
#include "widgets/PathEdit_p.h"

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

QTEST_MAIN(tst_EditorFactory)
#include "tst_editorfactory.moc"
