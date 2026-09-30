// API compatibility test for qpb 1.6 (docs/SPEC.md §9.5, PLAN M10.4).
//
// Exercises every public API added in 1.6 the way client code would. Once
// 1.6.0 is released this file is FROZEN: never edit it. Every later 1.x
// release must compile it (deprecation warnings allowed) and pass it unchanged.

#include <qpb/qpb.h>

#include <QApplication>
#include <QLabel>
#include <QTest>
#include <QToolButton>

#include <type_traits>

class tst_ApiCompat_1_6 : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void treeViewColours();
    void treeViewColoursFromStyleSheet();
    void formViewSelectors();
};

void tst_ApiCompat_1_6::cleanup()
{
    qApp->setStyleSheet(QString());
}

void tst_ApiCompat_1_6::treeViewColours()
{
    static_assert(std::is_same_v<decltype(&qpb::PropertyTreeView::groupBackground),
        QBrush (qpb::PropertyTreeView::*)() const>);
    static_assert(std::is_same_v<decltype(&qpb::PropertyTreeView::modifiedForeground),
        QColor (qpb::PropertyTreeView::*)() const>);
    qpb::PropertyTreeView view;
    const qpb::PropertyTreeView& constView = view;
    QCOMPARE(constView.groupBackground().style(), Qt::NoBrush);
    QVERIFY(!constView.groupForeground().isValid());
    QVERIFY(!constView.modifiedForeground().isValid());
    QVERIFY(!constView.readOnlyForeground().isValid());
    view.setGroupBackground(QBrush(QColor(0x2c, 0x30, 0x38)));
    view.setGroupForeground(QColor(0xe8, 0x7c, 0x00));
    view.setModifiedForeground(QColor(0xff, 0xa0, 0x40));
    view.setReadOnlyForeground(QColor(0x7a, 0x88, 0x98));
    QCOMPARE(view.groupBackground().color(), QColor(0x2c, 0x30, 0x38));
    QCOMPARE(view.property("groupForeground").value<QColor>(), QColor(0xe8, 0x7c, 0x00));
    QVERIFY(view.setProperty("modifiedForeground", QColor(Qt::red)));
    QCOMPARE(view.modifiedForeground(), QColor(Qt::red));
    QVERIFY(view.setProperty("readOnlyForeground", QColor()));
    QVERIFY(!view.readOnlyForeground().isValid());
}

void tst_ApiCompat_1_6::treeViewColoursFromStyleSheet()
{
    qApp->setStyleSheet(QStringLiteral("qpb--PropertyTreeView {"
                                       "  qproperty-groupBackground: #2c3038;"
                                       "  qproperty-groupForeground: #e87c00;"
                                       "  qproperty-modifiedForeground: #ffa040;"
                                       "  qproperty-readOnlyForeground: #7a8898; }"));
    qpb::PropertyTreeView view;
    view.ensurePolished();
    QCOMPARE(view.groupBackground().color(), QColor(0x2c, 0x30, 0x38));
    QCOMPARE(view.groupForeground(), QColor(0xe8, 0x7c, 0x00));
    QCOMPARE(view.modifiedForeground(), QColor(0xff, 0xa0, 0x40));
    QCOMPARE(view.readOnlyForeground(), QColor(0x7a, 0x88, 0x98));
}

void tst_ApiCompat_1_6::formViewSelectors()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addGroup(QStringLiteral("g")).addInt(QStringLiteral("n"), 1);
    root->addFilePath(QStringLiteral("file"), QString());
    qpb::PropertyModel model(std::move(root));
    qpb::PropertyFormView view;
    view.setModel(&model);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    const auto parts = [&view](const QString& part) {
        int count = 0;
        for (const QWidget* widget : view.findChildren<QWidget*>()) {
            if (widget->property("qpbPart").toString() == part)
                ++count;
        }
        return count;
    };
    QCOMPARE(parts(QStringLiteral("group")), 1);
    QCOMPARE(parts(QStringLiteral("groupTitle")), 1);
    QCOMPARE(parts(QStringLiteral("groupBody")), 1);
    QCOMPARE(parts(QStringLiteral("label")), 2);
    QCOMPARE(parts(QStringLiteral("browse")), 1);

    QLabel* label = nullptr;
    for (QLabel* candidate : view.findChildren<QLabel*>()) {
        if (candidate->buddy() == view.editor(QStringLiteral("g/n")))
            label = candidate;
    }
    QVERIFY(label);
    QCOMPARE(label->property("qpbModified"), QVariant(false));
    QVERIFY(model.setValue(QStringLiteral("g/n"), 2));
    QCOMPARE(label->property("qpbModified"), QVariant(true));
    QVERIFY(label->font().bold());
}

QTEST_MAIN(tst_ApiCompat_1_6)
#include "v1_6.moc"
