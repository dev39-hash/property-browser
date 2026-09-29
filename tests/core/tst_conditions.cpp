// Conditions between properties (docs/PLAN.md M8.3, SPEC 4.2). Since 1.4.

#include <qpb/qpbcore.h>

#include <QAbstractItemModelTester>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

using namespace qpb;

namespace {

// Settings
// |-- General
// |   |-- autosave        bool
// |   |-- minutes         int, enabled when autosave
// |   |-- mode            enum Fast/Quality (0/1)
// |   `-- quality         int, visible when mode == 1
// `-- Limits
//     |-- kind            string "none"/"size"
//     `-- size            int, enabled when kind != "none" (predicate)
std::unique_ptr<PropertyGroup> createTree()
{
    auto root = PropertyGroup::create(QStringLiteral("Settings"));
    PropertyGroup& general = root->addGroup(QStringLiteral("General"));
    general.addBool(QStringLiteral("autosave"), true);
    general.addInt(QStringLiteral("minutes"), 5).enabledWhen(QStringLiteral("General/autosave"));
    general.addEnum(QStringLiteral("mode"), {QStringLiteral("Fast"), QStringLiteral("Quality")}, 0);
    general.addInt(QStringLiteral("quality"), 50).visibleWhen(QStringLiteral("General/mode"), 1);
    PropertyGroup& limits = root->addGroup(QStringLiteral("Limits"));
    limits.addString(QStringLiteral("kind"), QStringLiteral("none"));
    limits.addInt(QStringLiteral("size"), 10)
        .enabledWhen(QStringLiteral("Limits/kind"),
            [](const QVariant& value) { return value.toString() != QStringLiteral("none"); });
    return root;
}

} // namespace

class tst_Conditions : public QObject
{
    Q_OBJECT

private slots:
    void initialEvaluation();
    void followsSourceValues();
    void truthiness_data();
    void truthiness();
    void combinedWithOwnFlags();
    void groupConditionsReachDescendants();
    void userEditsBlockedByCondition();
    void notifiesViews();
    void structureChanges();
    void missingSourceCountsAsMet();
    void outsideAModel();
    void clearAndReplace();
    void stateSeenByValueChangedHandlers();
};

void tst_Conditions::initialEvaluation()
{
    auto root = createTree();
    QVERIFY(root->find(QStringLiteral("General/quality"))->isVisible()); // no model: met
    PropertyModel model(std::move(root));
    QVERIFY(model.find(QStringLiteral("General/minutes"))->isEnabled());
    QVERIFY(!model.find(QStringLiteral("General/quality"))->isVisible());
    QVERIFY(!model.find(QStringLiteral("Limits/size"))->isEnabled());
    // Own flags are untouched.
    QVERIFY(!model.find(QStringLiteral("Limits/size"))->flags().testFlag(Property::Flag::Disabled));
}

void tst_Conditions::followsSourceValues()
{
    PropertyModel model(createTree());
    Property* minutes = model.find(QStringLiteral("General/minutes"));
    QVERIFY(model.setValue(QStringLiteral("General/autosave"), false));
    QVERIFY(!minutes->isEnabled());
    QVERIFY(model.setValue(QStringLiteral("General/autosave"), true));
    QVERIFY(minutes->isEnabled());

    QVERIFY(model.setValue(QStringLiteral("General/mode"), 1));
    QVERIFY(model.find(QStringLiteral("General/quality"))->isVisible());
    QVERIFY(model.setValue(QStringLiteral("Limits/kind"), QStringLiteral("size")));
    QVERIFY(model.find(QStringLiteral("Limits/size"))->isEnabled());

    // Resets go through the same path.
    QVERIFY(model.root()->resetToDefault());
    QVERIFY(!model.find(QStringLiteral("General/quality"))->isVisible());
    QVERIFY(!model.find(QStringLiteral("Limits/size"))->isEnabled());
}

void tst_Conditions::truthiness_data()
{
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<bool>("met");
    QTest::newRow("true") << QVariant(true) << true;
    QTest::newRow("false") << QVariant(false) << false;
    QTest::newRow("int 0") << QVariant(0) << false;
    QTest::newRow("int 3") << QVariant(3) << true;
    QTest::newRow("double 0.5") << QVariant(0.5) << true;
    QTest::newRow("empty string") << QVariant(QString()) << false;
    QTest::newRow("string") << QVariant(QStringLiteral("x")) << true;
    QTest::newRow("string false") << QVariant(QStringLiteral("false")) << true; // non-empty
}

void tst_Conditions::truthiness()
{
    QFETCH(QVariant, value);
    QFETCH(bool, met);
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->add(QStringLiteral("test.any"), QStringLiteral("source"), value);
    root->addInt(QStringLiteral("target"), 0).enabledWhen(QStringLiteral("source"));
    PropertyModel model(std::move(root));
    QCOMPARE(model.find(QStringLiteral("target"))->isEnabled(), met);
}

void tst_Conditions::combinedWithOwnFlags()
{
    PropertyModel model(createTree());
    Property* minutes = model.find(QStringLiteral("General/minutes"));
    minutes->setEnabled(false); // the application's own choice
    QVERIFY(!minutes->isEnabled());
    QVERIFY(model.setValue(QStringLiteral("General/autosave"), false));
    QVERIFY(model.setValue(QStringLiteral("General/autosave"), true));
    QVERIFY(!minutes->isEnabled()); // the condition did not overwrite it
    minutes->setEnabled(true);
    QVERIFY(minutes->isEnabled());
}

void tst_Conditions::groupConditionsReachDescendants()
{
    PropertyModel model(createTree());
    PropertyGroup* limits = model.find(QStringLiteral("Limits"))->toGroup();
    limits->setVisibleWhen(QStringLiteral("General/autosave"));
    QVERIFY(model.find(QStringLiteral("Limits/kind"))->isVisible());
    QVERIFY(model.setValue(QStringLiteral("General/autosave"), false));
    QVERIFY(!limits->isVisible());
    QVERIFY(!model.find(QStringLiteral("Limits/kind"))->isVisible());
    QCOMPARE(model.indexOf(model.find(QStringLiteral("Limits/kind")))
                 .data(PropertyModel::IsVisibleRole)
                 .toBool(),
        false);
}

void tst_Conditions::userEditsBlockedByCondition()
{
    PropertyModel model(createTree());
    QVERIFY(model.setValue(QStringLiteral("General/autosave"), false));
    const QModelIndex minutes
        = model.indexOf(model.find(QStringLiteral("General/minutes")), PropertyModel::ValueColumn);
    QVERIFY(!minutes.flags().testFlag(Qt::ItemIsEditable));
    QVERIFY(!model.setData(minutes, 30)); // disabled for the user
    QVERIFY(model.setValue(QStringLiteral("General/minutes"), 30)); // D34: application writes
}

void tst_Conditions::notifiesViews()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    QVERIFY(model.setValue(QStringLiteral("General/autosave"), false));
    const QModelIndex minutes = model.indexOf(model.find(QStringLiteral("General/minutes")));
    bool minutesNotified = false;
    for (const QList<QVariant>& arguments : std::as_const(changed)) {
        const QModelIndex topLeft = arguments[0].value<QModelIndex>();
        const QModelIndex bottomRight = arguments[1].value<QModelIndex>();
        if (topLeft.parent() == minutes.parent() && topLeft.row() <= minutes.row()
            && bottomRight.row() >= minutes.row())
            minutesNotified = true;
    }
    QVERIFY(minutesNotified);

    // A change that does not flip the condition does not notify the dependent.
    changed.clear();
    QVERIFY(model.setValue(QStringLiteral("Limits/kind"), QStringLiteral("other")));
    changed.clear();
    QVERIFY(model.setValue(QStringLiteral("Limits/kind"), QStringLiteral("size")));
    QCOMPARE(changed.count(), 1); // only the source row
}

void tst_Conditions::structureChanges()
{
    PropertyModel model(createTree());
    PropertyGroup* general = model.find(QStringLiteral("General"))->toGroup();
    // A dependent added at run time is evaluated at once.
    Property& interval = general->addInt(QStringLiteral("interval"), 1);
    interval.setVisibleWhen(QStringLiteral("General/autosave"), false);
    QVERIFY(!interval.isVisible());

    // The source removed: conditions count as met; added again: evaluated.
    QVERIFY(model.setValue(QStringLiteral("General/autosave"), false));
    QVERIFY(!model.find(QStringLiteral("General/minutes"))->isEnabled());
    general->remove(QStringLiteral("autosave"));
    QVERIFY(model.find(QStringLiteral("General/minutes"))->isEnabled());
    general->addBool(QStringLiteral("autosave"), false);
    QVERIFY(!model.find(QStringLiteral("General/minutes"))->isEnabled());

    // setRoot(): the new tree is evaluated before views see it.
    model.setRoot(createTree());
    QVERIFY(!model.find(QStringLiteral("General/quality"))->isVisible());
}

void tst_Conditions::missingSourceCountsAsMet()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("x"), 0).enabledWhen(QStringLiteral("no/such"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(".*\"no/such\", which does not exist.*"));
    PropertyModel model(std::move(root));
    QVERIFY(model.find(QStringLiteral("x"))->isEnabled());
    model.setRoot(nullptr); // no second warning for the same condition afterwards
}

void tst_Conditions::outsideAModel()
{
    auto root = createTree();
    Property* minutes = root->find(QStringLiteral("General/minutes"));
    QVERIFY(root->find(QStringLiteral("General/autosave"))->setValue(false));
    QVERIFY(minutes->isEnabled()); // not evaluated without a model

    // A property removed from a model counts as met again.
    PropertyModel model(std::move(root));
    QVERIFY(!model.find(QStringLiteral("General/minutes"))->isEnabled());
    std::unique_ptr<Property> detached = Property::create(Types::Int, QStringLiteral("d"), 0);
    detached->setEnabledWhen(QStringLiteral("General/autosave"));
    Property& attached = model.find(QStringLiteral("General"))->toGroup()->add(std::move(detached));
    QVERIFY(!attached.isEnabled());
}

void tst_Conditions::clearAndReplace()
{
    PropertyModel model(createTree());
    Property* quality = model.find(QStringLiteral("General/quality"));
    QVERIFY(!quality->isVisible());
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    quality->clearVisibleWhen();
    QVERIFY(quality->isVisible());
    QVERIFY(changed.count() >= 1);

    quality->setVisibleWhen(QStringLiteral("General/mode"), 0); // replaces
    QVERIFY(quality->isVisible());
    QVERIFY(model.setValue(QStringLiteral("General/mode"), 1));
    QVERIFY(!quality->isVisible());
}

// valueChanged handlers already see the new dependent states.
void tst_Conditions::stateSeenByValueChangedHandlers()
{
    PropertyModel model(createTree());
    bool enabledInHandler = true;
    model.onValueChanged(QStringLiteral("General/autosave"), this, [&](const QVariant&) {
        enabledInHandler = model.find(QStringLiteral("General/minutes"))->isEnabled();
    });
    QVERIFY(model.setValue(QStringLiteral("General/autosave"), false));
    QVERIFY(!enabledInHandler);
}

QTEST_GUILESS_MAIN(tst_Conditions)
#include "tst_conditions.moc"
