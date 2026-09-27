// API compatibility test for qpb 1.1 (docs/SPEC.md §9.5, PLAN M5.5).
//
// Exercises every public API added in 1.1 the way client code would. Once
// 1.1.0 is released this file is FROZEN: never edit it. Every later 1.x
// release must compile it (deprecation warnings allowed) and pass it unchanged.

#include <qpb/qpb.h>

#include <QAbstractItemModelTester>
#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

#include <type_traits>

namespace {

// Extension point: a filter that also hides read-only properties.
class EditableOnlyFilter : public qpb::PropertyFilterProxyModel
{
public:
    using qpb::PropertyFilterProxyModel::PropertyFilterProxyModel;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        const QModelIndex value
            = sourceModel()->index(sourceRow, qpb::PropertyModel::ValueColumn, sourceParent);
        if (!value.data(qpb::PropertyModel::IsGroupRole).toBool()
            && !value.flags().testFlag(Qt::ItemIsEditable))
            return false;
        return qpb::PropertyFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }
};

class CustomFormView : public qpb::PropertyFormView
{
public:
    using qpb::PropertyFormView::PropertyFormView;
};

std::unique_ptr<qpb::PropertyGroup> createTree()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addString(QStringLiteral("notes"), QStringLiteral("a\nb")).multiline().maxLength(100);
    qpb::PropertyGroup& group = root->addGroup(QStringLiteral("group"));
    group.addInt(QStringLiteral("count"), 1).range(0, 10);
    group.addInt(QStringLiteral("fixed"), 2).readOnly();
    return root;
}

} // namespace

class tst_ApiCompat_1_1 : public QObject
{
    Q_OBJECT

private slots:
    void multilineAttribute();
    void filterProxyModel();
    void formView();
};

void tst_ApiCompat_1_1::multilineAttribute()
{
    QCOMPARE(QString(qpb::Attr::Multiline), QStringLiteral("multiline"));
    auto root = createTree();
    qpb::Property* notes = root->child(QStringLiteral("notes"));
    QCOMPARE(notes->attribute(qpb::Attr::Multiline).toBool(), true);

    qpb::StringBuilder builder(*notes);
    qpb::StringBuilder& chained = builder.multiline(false);
    QCOMPARE(&chained, &builder);
    QCOMPARE(notes->attribute(qpb::Attr::Multiline).toBool(), false);
}

void tst_ApiCompat_1_1::filterProxyModel()
{
    static_assert(std::is_base_of_v<QSortFilterProxyModel, qpb::PropertyFilterProxyModel>);
    qpb::PropertyModel model(createTree());
    qpb::PropertyFilterProxyModel proxy(&model);
    proxy.setSourceModel(&model);
    QAbstractItemModelTester tester(&proxy);
    proxy.setFilterFixedString(QStringLiteral("COUNT"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.rowCount(proxy.index(0, 0)), 1);

    EditableOnlyFilter editable;
    editable.setSourceModel(&model);
    QCOMPARE(editable.rowCount(editable.index(1, 0)), 1); // "fixed" is read-only
}

void tst_ApiCompat_1_1::formView()
{
    static_assert(std::is_base_of_v<QScrollArea, qpb::PropertyFormView>);
    qpb::PropertyModel model(createTree());
    CustomFormView view;
    QCOMPARE(view.model(), nullptr);
    view.setModel(&model);
    QCOMPARE(view.model(), &model);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QVERIFY(qobject_cast<QPlainTextEdit*>(view.editor(QStringLiteral("notes"))));
    auto* count = qobject_cast<QSpinBox*>(view.editor(QStringLiteral("group/count")));
    QVERIFY(count);
    QCOMPARE(view.editor(QStringLiteral("group")), nullptr);

    QVERIFY(view.isExpanded(QStringLiteral("group")));
    view.setExpanded(QStringLiteral("group"), false);
    QVERIFY(!view.isExpanded(QStringLiteral("group")));
    view.setExpanded(QStringLiteral("group"), true);

    QSignalSpy changed(&model, &qpb::PropertyModel::valueChanged);
    QVERIFY(model.setValue(QStringLiteral("group/count"), 5));
    QCOMPARE(count->value(), 5);
    count->setFocus();
    count->setValue(7);
    QTest::keyClick(count, Qt::Key_Return);
    QTRY_COMPARE(model.find(QStringLiteral("group/count"))->value(), QVariant(7));
    QCOMPARE(changed.count(), 2);

    qpb::PropertyFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    view.setModel(&proxy);
    proxy.setFilterFixedString(QStringLiteral("notes"));
    QVERIFY(view.editor(QStringLiteral("notes")));
    QCOMPARE(view.editor(QStringLiteral("group/count")), nullptr);
    view.setModel(nullptr);
}

QTEST_MAIN(tst_ApiCompat_1_1)
#include "v1_1.moc"
