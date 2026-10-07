// PropertyFilterProxyModel (docs/PLAN.md M5.3, SPEC 5.7).

#include <qpb/qpbcore.h>

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

using namespace qpb;

namespace {

// Scene
// |-- name
// |-- Transform
// |   |-- x
// |   |-- y
// |   `-- hiddenX     (hidden)
// `-- Camera
//     |-- fov         displayed as "Field of view"
//     `-- exposure
std::unique_ptr<PropertyGroup> createTree()
{
    auto root = PropertyGroup::create(QStringLiteral("Scene"));
    root->addString(QStringLiteral("name"), QStringLiteral("Main"));
    PropertyGroup& transform = root->addGroup(QStringLiteral("Transform"));
    transform.addDouble(QStringLiteral("x"), 0.0);
    transform.addDouble(QStringLiteral("y"), 0.0);
    transform.addDouble(QStringLiteral("hiddenX"), 0.0).visible(false);
    PropertyGroup& camera = root->addGroup(QStringLiteral("Camera"));
    camera.addInt(QStringLiteral("fov"), 60).displayName(QStringLiteral("Field of view"));
    camera.addDouble(QStringLiteral("exposure"), 1.0);
    return root;
}

// Paths of all rows the proxy shows, depth first.
QStringList shownPaths(const QAbstractItemModel& model, const QModelIndex& parent = {})
{
    QStringList paths;
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex index = model.index(row, 0, parent);
        paths << index.data(PropertyModel::PathRole).toString();
        paths << shownPaths(model, index);
    }
    return paths;
}

} // namespace

class tst_PropertyFilterProxyModel : public QObject
{
    Q_OBJECT

private slots:
    void defaults();
    void emptyFilterShowsEverything();
    void matchesDisplayNameIgnoringCase();
    void groupMatchShowsDescendants();
    void hiddenPropertiesNeverMatch();
    void followsModelChanges();
    void editsThroughProxy();
};

void tst_PropertyFilterProxyModel::defaults()
{
    PropertyFilterProxyModel proxy;
    QCOMPARE(proxy.filterKeyColumn(), int(PropertyModel::NameColumn));
    QCOMPARE(proxy.filterCaseSensitivity(), Qt::CaseInsensitive);
    QVERIFY(proxy.isRecursiveFilteringEnabled());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) // the property is new in Qt 6
    QVERIFY(!proxy.autoAcceptChildRows()); // subclasses see every row
#endif
}

void tst_PropertyFilterProxyModel::emptyFilterShowsEverything()
{
    PropertyModel model(createTree());
    PropertyFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    QAbstractItemModelTester tester(&proxy, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QCOMPARE(shownPaths(proxy),
        QStringList({"name", "Transform", "Transform/x", "Transform/y", "Transform/hiddenX",
            "Camera", "Camera/fov", "Camera/exposure"}));
}

void tst_PropertyFilterProxyModel::matchesDisplayNameIgnoringCase()
{
    PropertyModel model(createTree());
    PropertyFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    QAbstractItemModelTester tester(&proxy, QAbstractItemModelTester::FailureReportingMode::Fatal);

    proxy.setFilterFixedString(QStringLiteral("VIEW"));
    QCOMPARE(shownPaths(proxy), QStringList({"Camera", "Camera/fov"}));

    proxy.setFilterFixedString(QStringLiteral("fov")); // the id is not the display name
    QCOMPARE(shownPaths(proxy), QStringList());
}

void tst_PropertyFilterProxyModel::groupMatchShowsDescendants()
{
    PropertyModel model(createTree());
    PropertyFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setFilterFixedString(QStringLiteral("camera"));
    QCOMPARE(shownPaths(proxy), QStringList({"Camera", "Camera/fov", "Camera/exposure"}));
}

void tst_PropertyFilterProxyModel::hiddenPropertiesNeverMatch()
{
    PropertyModel model(createTree());
    PropertyFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setFilterFixedString(QStringLiteral("hidden"));
    QCOMPARE(shownPaths(proxy), QStringList());

    model.find(QStringLiteral("Transform/hiddenX"))->setVisible(true);
    QCOMPARE(shownPaths(proxy), QStringList({"Transform", "Transform/hiddenX"}));
}

void tst_PropertyFilterProxyModel::followsModelChanges()
{
    PropertyModel model(createTree());
    PropertyFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    QAbstractItemModelTester tester(&proxy, QAbstractItemModelTester::FailureReportingMode::Fatal);
    proxy.setFilterFixedString(QStringLiteral("exp"));
    QCOMPARE(shownPaths(proxy), QStringList({"Camera", "Camera/exposure"}));

    model.find(QStringLiteral("Transform"))->toGroup()->addBool(QStringLiteral("expanded"), true);
    QCOMPARE(shownPaths(proxy),
        QStringList({"Transform", "Transform/expanded", "Camera", "Camera/exposure"}));

    model.find(QStringLiteral("Camera/exposure"))->setDisplayName(QStringLiteral("Brightness"));
    QCOMPARE(shownPaths(proxy), QStringList({"Transform", "Transform/expanded"}));
}

void tst_PropertyFilterProxyModel::editsThroughProxy()
{
    PropertyModel model(createTree());
    PropertyFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setFilterFixedString(QStringLiteral("x"));
    QSignalSpy changed(&model, &PropertyModel::valueChanged);

    const QModelIndex transform = proxy.index(0, 0);
    QCOMPARE(transform.data(PropertyModel::PathRole).toString(), QStringLiteral("Transform"));
    const QModelIndex x = proxy.index(0, PropertyModel::ValueColumn, transform);
    QVERIFY(proxy.setData(x, 4.5, Qt::EditRole));
    QCOMPARE(model.find(QStringLiteral("Transform/x"))->value().toDouble(), 4.5);
    QCOMPARE(changed.count(), 1);
}

QTEST_GUILESS_MAIN(tst_PropertyFilterProxyModel)
#include "tst_propertyfilterproxymodel.moc"
