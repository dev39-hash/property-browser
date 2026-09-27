// API compatibility test for qpb 1.0 (docs/SPEC.md §9.5, PLAN M4.4).
//
// Exercises every public 1.0 API the way client code would. Once 1.0.0 is
// released this file is FROZEN: never edit it. Every later 1.x release must
// compile it (deprecation warnings allowed) and pass it unchanged.

#include <qpb/qpb.h>

#include <QAbstractItemModelTester>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPainter>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStyleOptionViewItem>
#include <QTest>

#include <memory>
#include <type_traits>

namespace {

// --- extension points: subclassing the widget classes ---------------------------

class CustomDelegate : public qpb::PropertyDelegate
{
public:
    using qpb::PropertyDelegate::PropertyDelegate;
    int events = 0;

protected:
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
        const QModelIndex& index) override
    {
        ++events;
        return qpb::PropertyDelegate::editorEvent(event, model, option, index);
    }
    bool eventFilter(QObject* object, QEvent* event) override
    {
        return qpb::PropertyDelegate::eventFilter(object, event);
    }
};

class CustomView : public qpb::PropertyTreeView
{
public:
    using qpb::PropertyTreeView::PropertyTreeView;
    QModelIndex nextEditable()
    {
        return moveCursor(MoveNext, Qt::NoModifier);
    }

protected:
    void contextMenuEvent(QContextMenuEvent* event) override
    {
        qpb::PropertyTreeView::contextMenuEvent(event);
    }
    QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) override
    {
        return qpb::PropertyTreeView::moveCursor(action, modifiers);
    }
};

} // namespace

class tst_ApiCompat_1_0 : public QObject
{
    Q_OBJECT

private slots:
    void globals();
    void typesAndAttributes();
    void validationResult();
    void propertyApi();
    void builders();
    void groupApi();
    void typeRegistry();
    void modelApi();
    void editorFactory();
    void delegateAndView();
};

void tst_ApiCompat_1_0::globals()
{
    static_assert(QPB_VERSION_CHECK(1, 0, 0) == 0x010000, "QPB_VERSION_CHECK");
    static_assert(QPB_VERSION >= QPB_VERSION_CHECK(0, 0, 0), "QPB_VERSION");
    static_assert(QPB_VERSION_MAJOR >= 0 && QPB_VERSION_MINOR >= 0 && QPB_VERSION_PATCH >= 0,
        "QPB_VERSION_* components");
    const char* runtime = qpb::version();
    QCOMPARE(QByteArray(runtime), QByteArray(QPB_VERSION_STR));
}

void tst_ApiCompat_1_0::typesAndAttributes()
{
    const qpb::TypeId ids[]
        = {qpb::Types::Bool, qpb::Types::Int, qpb::Types::Double, qpb::Types::String,
            qpb::Types::Enum, qpb::Types::FilePath, qpb::Types::DirPath, qpb::Types::Group};
    QCOMPARE(int(std::size(ids)), 8);
    QCOMPARE(ids[1], QStringLiteral("int"));

    const QString keys[] = {qpb::Attr::Minimum, qpb::Attr::Maximum, qpb::Attr::Step,
        qpb::Attr::Decimals, qpb::Attr::Prefix, qpb::Attr::Suffix, qpb::Attr::MaxLength,
        qpb::Attr::Placeholder, qpb::Attr::RegularExpression, qpb::Attr::Options, qpb::Attr::Filter,
        qpb::Attr::DialogMode, qpb::Attr::DefaultDir, qpb::Attr::MustExist, qpb::Attr::EditorId};
    QCOMPARE(int(std::size(keys)), 15);

    qpb::EnumOption option;
    option.label = QStringLiteral("One");
    option.value = 1;
    const qpb::EnumOption same {QStringLiteral("One"), 1};
    QVERIFY(option == same);
    QVERIFY(!(option != same));
    const QVariant stored = QVariant::fromValue(QList<qpb::EnumOption> {option});
    QCOMPARE(stored.value<QList<qpb::EnumOption>>().size(), 1);

    qpb::FileMode mode = qpb::FileMode::Open;
    mode = qpb::FileMode::Save;
    QVERIFY(mode != qpb::FileMode::Open);
}

void tst_ApiCompat_1_0::validationResult()
{
    qpb::ValidationResult result = qpb::ValidationResult::valid();
    QVERIFY(result.ok);
    QVERIFY(bool(result));
    result = qpb::ValidationResult::error(QStringLiteral("bad"));
    QVERIFY(!result);
    QCOMPARE(result.message, QStringLiteral("bad"));
    qpb::ValidationResult assigned;
    assigned.ok = false;
    assigned.message = QStringLiteral("x");
    QVERIFY(!assigned);
}

void tst_ApiCompat_1_0::propertyApi()
{
    static_assert(!std::is_copy_constructible_v<qpb::Property>, "Property is not copyable");

    std::unique_ptr<qpb::Property> detached
        = qpb::Property::create(qpb::Types::Int, QStringLiteral("detached"), 3);
    std::unique_ptr<qpb::Property> defaulted
        = qpb::Property::create(qpb::Types::String, QStringLiteral("empty"));
    QVERIFY(!defaulted->value().isValid() || defaulted->value().toString().isEmpty());

    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    qpb::Property& p = root->add(std::move(detached));
    const qpb::Property& cp = p;

    QCOMPARE(p.id(), QStringLiteral("detached"));
    QCOMPARE(p.path(), QStringLiteral("detached"));
    QCOMPARE(p.typeId(), qpb::TypeId(qpb::Types::Int));
    QVERIFY(!p.isGroup());
    QCOMPARE(p.parent(), root.get());
    QCOMPARE(p.toGroup(), nullptr);
    QCOMPARE(cp.toGroup(), nullptr);
    QCOMPARE(root->toGroup(), root.get());

    p.setDisplayName(QStringLiteral("Detached"));
    QCOMPARE(p.displayName(), QStringLiteral("Detached"));
    p.setToolTip(QStringLiteral("tip"));
    QCOMPARE(p.toolTip(), QStringLiteral("tip"));

    QVERIFY(p.setValue(4));
    QCOMPARE(p.value(), QVariant(4));
    QCOMPARE(p.defaultValue(), QVariant(3));
    QVERIFY(p.isModified());
    QVERIFY(p.resetToDefault());
    p.setDefaultValue(5);
    QCOMPARE(p.defaultValue(), QVariant(5));

    qpb::Property::Validator validator = [](const QVariant& value, const qpb::Property&) {
        return value.toInt() >= 0 ? qpb::ValidationResult::valid()
                                  : qpb::ValidationResult::error(QStringLiteral("neg"));
    };
    p.setValidator(validator);
    QVERIFY(bool(p.validator()));
    QVERIFY(!p.setValue(-1));

    p.setAttribute(QStringLiteral("app.key"), 1);
    QVERIFY(p.hasAttribute(QStringLiteral("app.key")));
    QCOMPARE(p.attribute(QStringLiteral("app.key")), QVariant(1));
    QCOMPARE(p.attribute(QStringLiteral("app.none"), 2), QVariant(2));
    QVERIFY(p.attributes().contains(QStringLiteral("app.key")));
    p.removeAttribute(QStringLiteral("app.key"));

    qpb::Property::Flags flags = qpb::Property::Flag::ReadOnly | qpb::Property::Flag::Hidden;
    flags |= qpb::Property::Flag::Disabled;
    p.setFlags(flags);
    QCOMPARE(p.flags(), flags);
    p.setFlag(qpb::Property::Flag::Disabled, false);
    p.setFlag(qpb::Property::Flag::Hidden);
    p.setReadOnly(false);
    p.setEnabled(true);
    p.setVisible(true);
    QVERIFY(!p.isReadOnly());
    QVERIFY(p.isEnabled());
    QVERIFY(p.isVisible());
}

void tst_ApiCompat_1_0::builders()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));

    qpb::BoolBuilder b = root->addBool(QStringLiteral("b"), true);
    b.displayName(QStringLiteral("B"))
        .toolTip(QStringLiteral("t"))
        .readOnly()
        .readOnly(false)
        .enabled()
        .enabled(true)
        .visible()
        .visible(true)
        .attribute(QStringLiteral("app.k"), 1)
        .editor(qpb::Types::Bool)
        .validator([](const QVariant&, const qpb::Property&) { return qpb::ValidationResult(); });
    qpb::Property& fromBuilder = b;
    QCOMPARE(&fromBuilder, &b.property());

    qpb::IntBuilder i = root->addInt(QStringLiteral("i"), 1);
    i.range(0, 10)
        .minimum(0)
        .maximum(10)
        .step(1)
        .prefix(QStringLiteral("#"))
        .suffix(QStringLiteral("px"));

    qpb::DoubleBuilder d = root->addDouble(QStringLiteral("d"), 1.0);
    d.range(0.0, 1.0).minimum(0.0).maximum(1.0).step(0.1).decimals(2).prefix(QString()).suffix(
        QStringLiteral("m"));

    qpb::StringBuilder s = root->addString(QStringLiteral("s"), QStringLiteral("x"));
    s.maxLength(10).placeholder(QStringLiteral("p")).regularExpression(QStringLiteral("x*"));

    qpb::EnumBuilder e = root->addEnum(QStringLiteral("e"), QStringList {QStringLiteral("A")}, 0);
    e.displayName(QStringLiteral("E"));

    qpb::FilePathBuilder f = root->addFilePath(QStringLiteral("f"), QString());
    f.filter(QStringLiteral("*.txt"))
        .dialogMode(qpb::FileMode::Save)
        .defaultDir(QStringLiteral("/"))
        .mustExist()
        .mustExist(false);

    qpb::DirPathBuilder p = root->addDirPath(QStringLiteral("p"), QString());
    p.defaultDir(QStringLiteral("/")).mustExist().mustExist(false);

    QCOMPARE(root->childCount(), 7);
}

void tst_ApiCompat_1_0::groupApi()
{
    std::unique_ptr<qpb::PropertyGroup> root = qpb::PropertyGroup::create(QStringLiteral("root"));
    qpb::PropertyGroup& group = root->addGroup(QStringLiteral("g"));
    group.addEnum(QStringLiteral("e"),
        QList<qpb::EnumOption> {{QStringLiteral("A"), QStringLiteral("a")}}, QStringLiteral("a"));
    qpb::Property& custom = group.add(QStringLiteral("app.custom"), QStringLiteral("c"), 1);
    group.add(qpb::PropertyGroup::create(QStringLiteral("sub")));

    const qpb::PropertyGroup& constGroup = group;
    QCOMPARE(constGroup.childCount(), 3);
    QCOMPARE(constGroup.child(1), &custom);
    QCOMPARE(constGroup.child(QStringLiteral("c")), &custom);
    QCOMPARE(constGroup.children().size(), 3);
    QCOMPARE(constGroup.indexOf(&custom), 1);
    QCOMPARE(root->find(QStringLiteral("g/c")), &custom);
    QVERIFY(group.remove(QStringLiteral("c")));
    QCOMPARE(group.childCount(), 2);
}

void tst_ApiCompat_1_0::typeRegistry()
{
    qpb::TypeRegistry& registry = qpb::TypeRegistry::global();
    static_assert(!std::is_copy_constructible_v<qpb::TypeRegistry>, "TypeRegistry not copyable");

    qpb::TypeHandler handler;
    handler.storageType = QMetaType::fromType<int>();
    handler.displayText
        = [](const QVariant& value, const qpb::Property&) { return value.toString(); };
    handler.normalize = [](const QVariant& value, const qpb::Property&) { return value; };
    handler.validate
        = [](const QVariant&, const qpb::Property&) { return qpb::ValidationResult::valid(); };
    QVERIFY(registry.registerType(QStringLiteral("app.api1"), handler));
    QVERIFY(registry.registerType<QString>(QStringLiteral("app.api2"), qpb::TypeHandler()));
    QVERIFY(registry.replaceType(QStringLiteral("app.api1"), handler));
    QVERIFY(registry.contains(QStringLiteral("app.api1")));
    const qpb::TypeHandler* found = registry.handler(QStringLiteral("app.api2"));
    QVERIFY(found);
    QCOMPARE(found->storageType, QMetaType::fromType<QString>());
    QVERIFY(registry.types().contains(QStringLiteral("app.api1")));
}

void tst_ApiCompat_1_0::modelApi()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("i"), 1);
    root->addBool(QStringLiteral("b"), false);
    root->addGroup(QStringLiteral("g")).addString(QStringLiteral("s"), QStringLiteral("x"));

    qpb::PropertyModel empty;
    qpb::PropertyModel parented(nullptr);
    Q_UNUSED(parented);
    empty.setRoot(qpb::PropertyGroup::create(QStringLiteral("other")));
    QVERIFY(empty.root());

    qpb::PropertyModel model(std::move(root));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);

    QSignalSpy values(&model, &qpb::PropertyModel::valueChanged);
    QSignalSpy failures(&model, &qpb::PropertyModel::validationFailed);
    QSignalSpy batches(&model, &qpb::PropertyModel::batchValueChanged);

    qpb::Property* i = model.find(QStringLiteral("i"));
    const QModelIndex name = model.indexOf(i);
    const QModelIndex value = model.indexOf(i, qpb::PropertyModel::ValueColumn);
    QCOMPARE(name.column(), int(qpb::PropertyModel::NameColumn));
    QCOMPARE(model.propertyAt(value), i);

    model.beginBatch();
    QVERIFY(model.setValue(QStringLiteral("i"), 2));
    QVERIFY(model.setData(value, 3, Qt::EditRole));
    model.endBatch();
    QCOMPARE(values.count(), 2);
    QCOMPARE(batches.count(), 1);
    QVERIFY(!model.setData(value, QStringLiteral("x")));
    QCOMPARE(failures.count(), 1);
    QVERIFY(model.resetToDefault(name));

    const int roles[] = {qpb::PropertyModel::PropertyRole, qpb::PropertyModel::TypeIdRole,
        qpb::PropertyModel::PathRole, qpb::PropertyModel::IsGroupRole,
        qpb::PropertyModel::IsModifiedRole, qpb::PropertyModel::AttributesRole,
        qpb::PropertyModel::IsVisibleRole, qpb::PropertyModel::UserRole};
    QCOMPARE(int(std::size(roles)), 8);
    QCOMPARE(int(qpb::PropertyModel::UserRole), Qt::UserRole + 100);
    QCOMPARE(value.data(qpb::PropertyModel::PropertyRole).value<const qpb::Property*>(), i);

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.columnCount(), 2);
    QVERIFY(model.hasChildren());
    QVERIFY(model.index(2, 0).isValid());
    QCOMPARE(model.parent(model.index(0, 0, model.index(2, 0))), model.index(2, 0));
    QVERIFY(model.data(name).isValid());
    QVERIFY(model.flags(value).testFlag(Qt::ItemIsEditable));
    QVERIFY(model.headerData(0, Qt::Horizontal).isValid());
    const QHash<int, QByteArray> names = model.roleNames();
    QCOMPARE(names.value(qpb::PropertyModel::PropertyRole), QByteArray("property"));
    QCOMPARE(names.value(qpb::PropertyModel::TypeIdRole), QByteArray("typeId"));
    QCOMPARE(names.value(qpb::PropertyModel::PathRole), QByteArray("path"));
    QCOMPARE(names.value(qpb::PropertyModel::IsGroupRole), QByteArray("isGroup"));
    QCOMPARE(names.value(qpb::PropertyModel::IsModifiedRole), QByteArray("isModified"));
    QCOMPARE(names.value(qpb::PropertyModel::AttributesRole), QByteArray("attributes"));
    QCOMPARE(names.value(qpb::PropertyModel::IsVisibleRole), QByteArray("isVisible"));
}

void tst_ApiCompat_1_0::editorFactory()
{
    qpb::EditorFactory& factory = qpb::EditorFactory::global();
    static_assert(!std::is_copy_constructible_v<qpb::EditorFactory>, "EditorFactory not copyable");

    qpb::EditorHandler handler;
    handler.createEditor
        = [](QWidget* parent, const qpb::Property&) { return new QLineEdit(parent); };
    handler.setEditorData = [](QWidget* editor, const QVariant& value, const qpb::Property&) {
        static_cast<QLineEdit*>(editor)->setText(value.toString());
    };
    handler.editorData = [](QWidget* editor, const qpb::Property&) {
        return QVariant(static_cast<QLineEdit*>(editor)->text());
    };
    handler.paint
        = [](QPainter*, const QStyleOptionViewItem&, const QVariant&, const qpb::Property&) {};
    handler.applyAttributes = [](QWidget*, const qpb::Property&) {};
    QVERIFY(factory.registerEditor(QStringLiteral("app.editor"), handler));
    QVERIFY(factory.replaceEditor(QStringLiteral("app.editor"), handler));
    QVERIFY(factory.contains(QStringLiteral("app.editor")));
    QVERIFY(factory.handler(QStringLiteral("app.editor")));
    QVERIFY(factory.editors().contains(QStringLiteral("app.editor")));

    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    qpb::Property& p
        = root->addString(QStringLiteral("s"), QString()).editor(QStringLiteral("app.editor"));
    QCOMPARE(factory.handlerFor(p), factory.handler(QStringLiteral("app.editor")));
    std::unique_ptr<QWidget> editor(factory.createEditor(nullptr, p));
    QVERIFY(qobject_cast<QLineEdit*>(editor.get()));
    qpb::EditorFactory::notifyCommit(editor.get());
    {
        qpb::EditorDialogScope scope(editor.get());
        static_assert(!std::is_copy_constructible_v<qpb::EditorDialogScope>, "scope not copyable");
    }
}

void tst_ApiCompat_1_0::delegateAndView()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("a"), 1);
    root->addGroup(QStringLiteral("g")).addInt(QStringLiteral("b"), 2);
    qpb::PropertyModel model(std::move(root));

    qpb::PropertyDelegate delegate;
    qpb::PropertyDelegate parentedDelegate(&model);
    Q_UNUSED(parentedDelegate);
    QWidget host;
    const QModelIndex value = model.indexOf(model.find(QStringLiteral("a")), 1);
    QStyleOptionViewItem option;
    std::unique_ptr<QWidget> editor(delegate.createEditor(&host, option, value));
    QVERIFY(qobject_cast<QSpinBox*>(editor.get()));
    delegate.setEditorData(editor.get(), value);
    static_cast<QSpinBox*>(editor.get())->setValue(7);
    delegate.setModelData(editor.get(), &model, value);
    QCOMPARE(model.find(QStringLiteral("a"))->value(), QVariant(7));
    delegate.updateEditorGeometry(editor.get(), option, value);
    QVERIFY(delegate.sizeHint(option, value).isValid());
    QImage image(100, 30, QImage::Format_ARGB32);
    QPainter painter(&image);
    option.rect = image.rect();
    delegate.paint(&painter, option, value);
    painter.end();

    CustomDelegate custom;
    Q_UNUSED(custom.events);

    qpb::PropertyTreeView view;
    qpb::PropertyTreeView parentedView(&host);
    Q_UNUSED(parentedView);
    view.setModel(&model);
    QVERIFY(view.propertyDelegate());
    view.setMode(qpb::PropertyTreeView::Mode::List);
    QCOMPARE(view.mode(), qpb::PropertyTreeView::Mode::List);
    QVERIFY(view.setProperty("mode", QVariant::fromValue(qpb::PropertyTreeView::Mode::Tree)));
    QCOMPARE(view.mode(), qpb::PropertyTreeView::Mode::Tree);
    view.setNameColumnWidth(120);
    QCOMPARE(view.nameColumnWidth(), 120);

    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    CustomView customView;
    customView.setModel(&proxy);
    customView.setItemDelegate(&custom);
    customView.setCurrentIndex(proxy.mapFromSource(value));
    QCOMPARE(customView.nextEditable(),
        proxy.mapFromSource(model.indexOf(model.find(QStringLiteral("g/b")), 1)));
}

QTEST_MAIN(tst_ApiCompat_1_0)
#include "v1_0.moc"
