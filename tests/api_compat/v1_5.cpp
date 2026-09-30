// API compatibility test for qpb 1.5 (docs/SPEC.md §9.5, PLAN M9.4).
//
// Exercises every public API added in 1.5 the way client code would. Once
// 1.5.0 is released this file is FROZEN: never edit it. Every later 1.x
// release must compile it (deprecation warnings allowed) and pass it unchanged.

#include <qpb/qpb.h>

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <type_traits>

namespace {

// Applications may subclass the view and still reach the base behaviour.
class View : public qpb::PropertyTreeView
{
public:
    bool nextChild(bool next) { return focusNextPrevChild(next); }
};

} // namespace

class tst_ApiCompat_1_5 : public QObject
{
    Q_OBJECT

private slots:
    void resetAllToDefault();
    void tabStopsOnCheckBoxes();
};

void tst_ApiCompat_1_5::resetAllToDefault()
{
    static_assert(std::is_same_v<decltype(&qpb::PropertyModel::resetAllToDefault),
        bool (qpb::PropertyModel::*)()>);
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addGroup(QStringLiteral("g")).addInt(QStringLiteral("x"), 1).readOnly();
    root->addBool(QStringLiteral("on"), false);
    qpb::PropertyModel model(std::move(root));
    QVERIFY(model.setValue(QStringLiteral("g/x"), 5));
    QVERIFY(model.setValue(QStringLiteral("on"), true));

    // Connectable like a slot, e.g. to a "Restore Defaults" button.
    QPushButton button;
    QObject::connect(&button, &QPushButton::clicked, &model, &qpb::PropertyModel::resetAllToDefault);
    QSignalSpy batch(&model, &qpb::PropertyModel::batchValueChanged);
    button.click();
    QCOMPARE(batch.count(), 1);
    QCOMPARE(model.find(QStringLiteral("g/x"))->value(), QVariant(1));
    QCOMPARE(model.find(QStringLiteral("on"))->value(), QVariant(false));
    QVERIFY(model.resetAllToDefault());

    qpb::PropertyModel empty;
    QVERIFY(empty.resetAllToDefault());
}

void tst_ApiCompat_1_5::tabStopsOnCheckBoxes()
{
    View view;
    const qpb::PropertyTreeView& constView = view;
    QVERIFY(!constView.tabStopsOnCheckBoxes());
    view.setTabStopsOnCheckBoxes(true);
    QVERIFY(view.tabStopsOnCheckBoxes());
    QCOMPARE(view.property("tabStopsOnCheckBoxes"), QVariant(true));
    QVERIFY(view.setProperty("tabStopsOnCheckBoxes", false));
    QVERIFY(!view.tabStopsOnCheckBoxes());
    view.nextChild(true); // no model, not shown: falls back to QTreeView
}

QTEST_MAIN(tst_ApiCompat_1_5)
#include "v1_5.moc"
