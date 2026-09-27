// PropertyModel (docs/PLAN.md M2.5-M2.8, docs/SPEC.md §4.6).

#include <qpb/qpbcore.h>

#include <QAbstractItemModelTester>
#include <QLocale>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

using namespace qpb;

namespace {

// Camera
// ├── name        string
// ├── visible     bool
// ├── Transform
// │   ├── x       double
// │   ├── y       double
// │   └── Pivot
// │       └── z   double
// └── fov         int (10..170)
std::unique_ptr<PropertyGroup> createTree()
{
    auto root = PropertyGroup::create(QStringLiteral("Camera"));
    root->addString(QStringLiteral("name"), QStringLiteral("Main"))
        .toolTip(QStringLiteral("Object name"));
    root->addBool(QStringLiteral("visible"), true);
    PropertyGroup& transform = root->addGroup(QStringLiteral("Transform"));
    transform.addDouble(QStringLiteral("x"), 0.0).range(-10.0, 10.0);
    transform.addDouble(QStringLiteral("y"), 0.0);
    transform.addGroup(QStringLiteral("Pivot")).addDouble(QStringLiteral("z"), 0.0);
    root->addInt(QStringLiteral("fov"), 60).range(10, 170).suffix(QStringLiteral("°"));
    return root;
}

QModelIndex valueIndex(const PropertyModel& model, const QString& path)
{
    return model.indexOf(model.find(path), PropertyModel::ValueColumn);
}

} // namespace

class tst_PropertyModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void structure();
    void emptyModel();
    void indexOfAndPropertyAt();
    void dataRoles();
    void flags();
    void unregisteredTypeIsReadOnly();
    void headerAndRoleNames();

    void setDataEmitsSignals();
    void setDataUnchangedValueIsSilent();
    void setDataRejected();
    void setDataCheckState();
    void userVersusApplicationWrites();
    void propertySetValueNotifiesModel();
    void setValueByPath();

    void metadataChangesEmitDataChanged();
    void flagChangesReachDescendants();
    void runtimeInsertAndRemove();
    void setRootResetsModel();

    void batches();
    void resetToDefault();

    void modelTesterWholeLifecycle();
};

void tst_PropertyModel::initTestCase()
{
    QLocale::setDefault(QLocale::c());
}

void tst_PropertyModel::structure()
{
    PropertyModel model(createTree());
    QCOMPARE(model.columnCount(), 2);
    QCOMPARE(model.rowCount(), 4);
    QVERIFY(model.hasChildren());

    const QModelIndex transform = model.index(2, 0);
    QCOMPARE(transform.data().toString(), QStringLiteral("Transform"));
    QCOMPARE(model.rowCount(transform), 3);
    QCOMPARE(model.rowCount(model.index(2, 1)), 0); // only column 0 has children
    const QModelIndex pivot = model.index(2, 0, transform);
    const QModelIndex z = model.index(0, 0, pivot);
    QCOMPARE(z.data().toString(), QStringLiteral("z"));
    QCOMPARE(z.parent(), pivot);
    QCOMPARE(pivot.parent(), transform);
    QCOMPARE(transform.parent(), QModelIndex());
    QVERIFY(!model.index(4, 0).isValid());
    QVERIFY(!model.index(0, 2).isValid());
    QVERIFY(!model.hasChildren(model.index(0, 0)));
}

void tst_PropertyModel::emptyModel()
{
    PropertyModel model;
    QCOMPARE(model.root(), nullptr);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.find(QStringLiteral("x")), nullptr);
    QVERIFY(!model.setValue(QStringLiteral("x"), 1));
    QVERIFY(!model.resetToDefault(QModelIndex()));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
}

void tst_PropertyModel::indexOfAndPropertyAt()
{
    PropertyModel model(createTree());
    Property* x = model.find(QStringLiteral("Transform/x"));
    QVERIFY(x);
    const QModelIndex index = model.indexOf(x, PropertyModel::ValueColumn);
    QCOMPARE(index.column(), 1);
    QCOMPARE(index.row(), 0);
    QCOMPARE(model.propertyAt(index), x);
    QCOMPARE(model.propertyAt(index.siblingAtColumn(0)), x);
    QCOMPARE(model.propertyAt(QModelIndex()), nullptr);

    QVERIFY(!model.indexOf(model.root()).isValid());
    QVERIFY(!model.indexOf(nullptr).isValid());
    QVERIFY(!model.indexOf(x, 2).isValid());
    auto foreign = PropertyGroup::create(QStringLiteral("other"));
    QVERIFY(!model.indexOf(&foreign->addInt(QStringLiteral("i"), 1).property()).isValid());

    PropertyModel other(createTree());
    QCOMPARE(model.propertyAt(other.index(0, 0)), nullptr);
}

void tst_PropertyModel::dataRoles()
{
    PropertyModel model(createTree());
    const QModelIndex name = model.indexOf(model.find(QStringLiteral("name")));
    const QModelIndex nameValue = name.siblingAtColumn(1);
    QCOMPARE(name.data().toString(), QStringLiteral("name"));
    QCOMPARE(name.data(Qt::ToolTipRole).toString(), QStringLiteral("Object name"));
    QCOMPARE(nameValue.data().toString(), QStringLiteral("Main"));
    QCOMPARE(nameValue.data(Qt::EditRole), QVariant(QStringLiteral("Main")));
    QCOMPARE(nameValue.data(PropertyModel::PathRole).toString(), QStringLiteral("name"));
    QCOMPARE(nameValue.data(PropertyModel::TypeIdRole).toString(), TypeId(Types::String));
    QCOMPARE(nameValue.data(PropertyModel::IsGroupRole).toBool(), false);
    QCOMPARE(nameValue.data(PropertyModel::IsModifiedRole).toBool(), false);
    QCOMPARE(nameValue.data(PropertyModel::IsVisibleRole).toBool(), true);
    QCOMPARE(nameValue.data(PropertyModel::PropertyRole).value<const Property*>(),
        model.find(QStringLiteral("name")));

    const QModelIndex fov = valueIndex(model, QStringLiteral("fov"));
    QCOMPARE(fov.data().toString(), QStringLiteral("60°"));
    QCOMPARE(fov.data(Qt::ToolTipRole).toString(), QStringLiteral("60°"));
    QCOMPARE(fov.data(PropertyModel::AttributesRole).toMap().value(Attr::Maximum), QVariant(170));

    const QModelIndex visible = valueIndex(model, QStringLiteral("visible"));
    QCOMPARE(visible.data().toString(), QString());
    QCOMPARE(visible.data(Qt::CheckStateRole).toInt(), int(Qt::Checked));
    QVERIFY(!fov.data(Qt::CheckStateRole).isValid());

    const QModelIndex transformValue = model.index(2, 1);
    QVERIFY(!transformValue.data().isValid());
    QCOMPARE(transformValue.data(PropertyModel::IsGroupRole).toBool(), true);

    model.find(QStringLiteral("fov"))->setVisible(false);
    QCOMPARE(fov.data(PropertyModel::IsVisibleRole).toBool(), false);
    model.find(QStringLiteral("Transform"))->setVisible(false);
    QCOMPARE(valueIndex(model, QStringLiteral("Transform/x"))
                 .data(PropertyModel::IsVisibleRole)
                 .toBool(),
        false);
}

void tst_PropertyModel::flags()
{
    PropertyModel model(createTree());
    const QModelIndex fovName = model.indexOf(model.find(QStringLiteral("fov")));
    const QModelIndex fov = fovName.siblingAtColumn(1);
    QVERIFY(model.flags(fov).testFlag(Qt::ItemIsEditable));
    QVERIFY(model.flags(fov).testFlag(Qt::ItemIsEnabled));
    QVERIFY(!model.flags(fovName).testFlag(Qt::ItemIsEditable));

    const QModelIndex visible = valueIndex(model, QStringLiteral("visible"));
    QVERIFY(model.flags(visible).testFlag(Qt::ItemIsUserCheckable));
    QVERIFY(!model.flags(visible).testFlag(Qt::ItemIsEditable));

    const QModelIndex transform = model.index(2, 1);
    QVERIFY(!model.flags(transform).testFlag(Qt::ItemIsEditable));
    QVERIFY(!model.flags(transform).testFlag(Qt::ItemNeverHasChildren));

    model.find(QStringLiteral("fov"))->setReadOnly(true);
    QVERIFY(!model.flags(fov).testFlag(Qt::ItemIsEditable));
    QVERIFY(model.flags(fov).testFlag(Qt::ItemIsEnabled));

    model.root()->setEnabled(false);
    QVERIFY(!model.flags(fov).testFlag(Qt::ItemIsEnabled));
    QVERIFY(!model.flags(visible).testFlag(Qt::ItemIsUserCheckable));
    QCOMPARE(model.flags(QModelIndex()), Qt::NoItemFlags);
}

void tst_PropertyModel::unregisteredTypeIsReadOnly()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    root->add(QStringLiteral("test.unregistered"), QStringLiteral("u"), 5);
    PropertyModel model(std::move(root));
    const QModelIndex value = model.index(0, 1);
    QCOMPARE(value.data().toString(), QStringLiteral("5"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("is not registered")));
    QVERIFY(!model.flags(value).testFlag(Qt::ItemIsEditable));
    QVERIFY(!model.flags(value).testFlag(Qt::ItemIsEditable)); // warns only once
    QVERIFY(!model.setData(value, 6));
}

void tst_PropertyModel::headerAndRoleNames()
{
    PropertyModel model;
    QCOMPARE(model.headerData(0, Qt::Horizontal).toString(), QStringLiteral("Property"));
    QCOMPARE(model.headerData(1, Qt::Horizontal).toString(), QStringLiteral("Value"));
    QVERIFY(!model.headerData(0, Qt::Vertical).isValid());
    const QHash<int, QByteArray> names = model.roleNames();
    QCOMPARE(names.value(PropertyModel::PathRole), QByteArray("path"));
    QCOMPARE(names.value(PropertyModel::PropertyRole), QByteArray("property"));
    QCOMPARE(names.value(Qt::DisplayRole), QByteArray("display"));
    QCOMPARE(int(PropertyModel::UserRole), Qt::UserRole + 100);
}

void tst_PropertyModel::setDataEmitsSignals()
{
    PropertyModel model(createTree());
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy data(&model, &QAbstractItemModel::dataChanged);

    const QModelIndex x = valueIndex(model, QStringLiteral("Transform/x"));
    QVERIFY(model.setData(x, 3.5));
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(0).toString(), QStringLiteral("Transform/x"));
    QCOMPARE(values.at(0).at(1), QVariant(3.5));
    QCOMPARE(values.at(0).at(2), QVariant(0.0));
    QCOMPARE(data.count(), 1);
    QCOMPARE(data.at(0).at(0).value<QModelIndex>(), x.siblingAtColumn(0));
    QCOMPARE(data.at(0).at(1).value<QModelIndex>(), x);
    QCOMPARE(x.data(PropertyModel::IsModifiedRole).toBool(), true);

    // Normalized before storing: clamped to the range.
    QVERIFY(model.setData(x, 99.0));
    QCOMPARE(values.last().at(1), QVariant(10.0));
}

void tst_PropertyModel::setDataUnchangedValueIsSilent()
{
    PropertyModel model(createTree());
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy data(&model, &QAbstractItemModel::dataChanged);
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("fov")), 60));
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("fov")), QStringLiteral("60")));
    QCOMPARE(values.count(), 0);
    QCOMPARE(data.count(), 0);
}

void tst_PropertyModel::setDataRejected()
{
    PropertyModel model(createTree());
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy failures(&model, &PropertyModel::validationFailed);

    const QModelIndex fov = valueIndex(model, QStringLiteral("fov"));
    QVERIFY(!model.setData(fov, QStringLiteral("wide")));
    QCOMPARE(failures.count(), 1);
    QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("fov"));
    QCOMPARE(failures.at(0).at(1), QVariant(QStringLiteral("wide")));
    QVERIFY(!failures.at(0).at(2).toString().isEmpty());

    model.find(QStringLiteral("fov"))->setReadOnly(true);
    QVERIFY(!model.setData(fov, 90));
    QCOMPARE(failures.count(), 1); // read-only is not a validation failure

    QVERIFY(!model.setData(fov.siblingAtColumn(0), 90)); // name column
    QVERIFY(!model.setData(model.index(2, 1), 1)); // group
    QVERIFY(!model.setData(fov, 90, Qt::DisplayRole));
    QCOMPARE(values.count(), 0);
    QCOMPARE(model.find(QStringLiteral("fov"))->value(), QVariant(60));
}

void tst_PropertyModel::setDataCheckState()
{
    PropertyModel model(createTree());
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    const QModelIndex visible = valueIndex(model, QStringLiteral("visible"));
    QVERIFY(model.setData(visible, Qt::Unchecked, Qt::CheckStateRole));
    QCOMPARE(model.find(QStringLiteral("visible"))->value(), QVariant(false));
    QCOMPARE(visible.data(Qt::CheckStateRole).toInt(), int(Qt::Unchecked));
    QCOMPARE(values.count(), 1);
    QVERIFY(
        !model.setData(valueIndex(model, QStringLiteral("fov")), Qt::Checked, Qt::CheckStateRole));
}

// Views (setData) respect read-only and disabled; application code does not
// (SPEC D34, found in the RC trial).
void tst_PropertyModel::userVersusApplicationWrites()
{
    PropertyModel model(createTree());
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    Property* fov = model.find(QStringLiteral("fov"));
    Property* visible = model.find(QStringLiteral("visible"));
    fov->setReadOnly(true);
    visible->setEnabled(false);

    QVERIFY(!model.setData(valueIndex(model, QStringLiteral("fov")), 90));
    QVERIFY(!model.setData(
        valueIndex(model, QStringLiteral("visible")), Qt::Unchecked, Qt::CheckStateRole));
    QCOMPARE(values.count(), 0);

    QVERIFY(model.setValue(QStringLiteral("fov"), 90));
    QVERIFY(visible->setValue(false));
    QCOMPARE(values.count(), 2);
    QCOMPARE(fov->value(), QVariant(90));
    QVERIFY(model.resetToDefault(model.indexOf(fov)));
    QCOMPARE(fov->value(), QVariant(60));
}

void tst_PropertyModel::propertySetValueNotifiesModel()
{
    PropertyModel model(createTree());
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy failures(&model, &PropertyModel::validationFailed);
    Property* name = model.find(QStringLiteral("name"));
    QVERIFY(name->setValue(QStringLiteral("Other")));
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(0).toString(), QStringLiteral("name"));

    name->setValidator([](const QVariant& value, const Property&) {
        return value.toString().isEmpty() ? ValidationResult::error(QStringLiteral("empty"))
                                          : ValidationResult::valid();
    });
    QVERIFY(!name->setValue(QString()));
    QCOMPARE(failures.count(), 1);
    QCOMPARE(failures.at(0).at(2).toString(), QStringLiteral("empty"));
}

void tst_PropertyModel::setValueByPath()
{
    PropertyModel model(createTree());
    QVERIFY(model.setValue(QStringLiteral("Transform/Pivot/z"), 2.0));
    QCOMPARE(model.find(QStringLiteral("Transform/Pivot/z"))->value(), QVariant(2.0));
    QVERIFY(!model.setValue(QStringLiteral("Transform/missing"), 2.0));
}

void tst_PropertyModel::metadataChangesEmitDataChanged()
{
    PropertyModel model(createTree());
    QSignalSpy data(&model, &QAbstractItemModel::dataChanged);
    Property* fov = model.find(QStringLiteral("fov"));
    const QModelIndex fovName = model.indexOf(fov);

    fov->setDisplayName(QStringLiteral("Field of view"));
    QCOMPARE(data.count(), 1);
    QCOMPARE(data.at(0).at(0).value<QModelIndex>(), fovName);
    QCOMPARE(fovName.data().toString(), QStringLiteral("Field of view"));

    fov->setAttribute(Attr::Suffix, QStringLiteral(" deg"));
    QCOMPARE(data.count(), 2);
    QCOMPARE(fovName.siblingAtColumn(1).data().toString(), QStringLiteral("60 deg"));

    fov->setDisplayName(QStringLiteral("Field of view")); // unchanged: no signal
    QCOMPARE(data.count(), 2);
}

void tst_PropertyModel::flagChangesReachDescendants()
{
    PropertyModel model(createTree());
    QSignalSpy data(&model, &QAbstractItemModel::dataChanged);
    Property* transform = model.find(QStringLiteral("Transform"));
    transform->setReadOnly(true);

    // The group row itself, its children, and its nested group's children.
    const QModelIndex transformIndex = model.indexOf(transform);
    const QModelIndex pivotIndex = model.indexOf(model.find(QStringLiteral("Transform/Pivot")));
    bool groupRow = false;
    bool childRows = false;
    bool nestedRows = false;
    for (const QList<QVariant>& args : data) {
        const auto topLeft = args.at(0).value<QModelIndex>();
        const auto bottomRight = args.at(1).value<QModelIndex>();
        groupRow |= topLeft == transformIndex;
        childRows
            |= topLeft.parent() == transformIndex && topLeft.row() == 0 && bottomRight.row() == 2;
        nestedRows |= topLeft.parent() == pivotIndex;
    }
    QVERIFY(groupRow);
    QVERIFY(childRows);
    QVERIFY(nestedRows);
    QVERIFY(!model.flags(valueIndex(model, QStringLiteral("Transform/Pivot/z")))
                 .testFlag(Qt::ItemIsEditable));
}

void tst_PropertyModel::runtimeInsertAndRemove()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

    // Top level.
    model.root()->addInt(QStringLiteral("iso"), 100);
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(inserted.at(0).at(0).value<QModelIndex>(), QModelIndex());
    QCOMPARE(inserted.at(0).at(1).toInt(), 4);
    QCOMPARE(model.rowCount(), 5);

    // Nested, including a whole subtree added at once.
    auto* pivot = model.find(QStringLiteral("Transform/Pivot"))->toGroup();
    auto extra = PropertyGroup::create(QStringLiteral("Extra"));
    extra->addBool(QStringLiteral("on"), true);
    pivot->add(std::move(extra));
    QCOMPARE(inserted.count(), 2);
    QCOMPARE(inserted.at(1).at(0).value<QModelIndex>(), model.indexOf(pivot));
    QVERIFY(valueIndex(model, QStringLiteral("Transform/Pivot/Extra/on")).isValid());

    // Values inside the new subtree notify the model.
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QVERIFY(model.find(QStringLiteral("Transform/Pivot/Extra/on"))->setValue(false));
    QCOMPARE(values.count(), 1);

    QVERIFY(pivot->remove(QStringLiteral("Extra")));
    QCOMPARE(removed.count(), 1);
    QCOMPARE(removed.at(0).at(0).value<QModelIndex>(), model.indexOf(pivot));
    QCOMPARE(removed.at(0).at(1).toInt(), 1);
    QVERIFY(model.root()->remove(QStringLiteral("Transform")));
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.find(QStringLiteral("Transform/x")), nullptr);
}

void tst_PropertyModel::setRootResetsModel()
{
    PropertyModel model(createTree());
    QSignalSpy aboutToReset(&model, &QAbstractItemModel::modelAboutToBeReset);
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

    auto next = PropertyGroup::create(QStringLiteral("Next"));
    next->addInt(QStringLiteral("only"), 1);
    PropertyGroup* nextRaw = next.get();
    model.setRoot(std::move(next));
    QCOMPARE(aboutToReset.count(), 1);
    QCOMPARE(reset.count(), 1);
    QCOMPARE(model.root(), nextRaw);
    QCOMPARE(model.rowCount(), 1);

    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QVERIFY(model.setValue(QStringLiteral("only"), 2));
    QCOMPARE(values.count(), 1);

    model.setRoot(nullptr);
    QCOMPARE(model.rowCount(), 0);
}

void tst_PropertyModel::batches()
{
    PropertyModel model(createTree());
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy batch(&model, &PropertyModel::batchValueChanged);

    model.beginBatch();
    model.beginBatch();
    QVERIFY(model.setValue(QStringLiteral("fov"), 90));
    QVERIFY(model.setValue(QStringLiteral("Transform/x"), 1.0));
    model.endBatch();
    QCOMPARE(batch.count(), 0); // still inside the outer batch
    QVERIFY(model.setValue(QStringLiteral("fov"), 100));
    model.endBatch();

    QCOMPARE(values.count(), 3);
    QCOMPARE(batch.count(), 1);
    QCOMPARE(batch.at(0).at(0).toStringList(),
        (QStringList {QStringLiteral("fov"), QStringLiteral("Transform/x")}));

    // An empty batch emits nothing.
    model.beginBatch();
    model.endBatch();
    QCOMPARE(batch.count(), 1);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("without beginBatch")));
    model.endBatch();
}

void tst_PropertyModel::resetToDefault()
{
    PropertyModel model(createTree());
    QVERIFY(model.setValue(QStringLiteral("Transform/x"), 1.0));
    QVERIFY(model.setValue(QStringLiteral("Transform/Pivot/z"), 2.0));
    QVERIFY(model.setValue(QStringLiteral("fov"), 90));

    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy batch(&model, &PropertyModel::batchValueChanged);
    QVERIFY(model.resetToDefault(model.indexOf(model.find(QStringLiteral("Transform")))));
    QCOMPARE(values.count(), 2);
    QCOMPARE(batch.count(), 1);
    QCOMPARE(model.find(QStringLiteral("Transform/Pivot/z"))->value(), QVariant(0.0));
    QCOMPARE(model.find(QStringLiteral("fov"))->value(), QVariant(90));

    QVERIFY(model.resetToDefault(valueIndex(model, QStringLiteral("fov"))));
    QCOMPARE(model.find(QStringLiteral("fov"))->value(), QVariant(60));
    QCOMPARE(batch.count(), 2);
}

void tst_PropertyModel::modelTesterWholeLifecycle()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);

    QVERIFY(model.setValue(QStringLiteral("fov"), 30));
    model.find(QStringLiteral("Transform"))->setVisible(false);
    model.find(QStringLiteral("Transform"))->setEnabled(false);
    PropertyGroup& added = model.root()->addGroup(QStringLiteral("Added"));
    for (int i = 0; i < 5; ++i)
        added.addInt(QStringLiteral("i%1").arg(i), i);
    QVERIFY(added.remove(QStringLiteral("i2")));
    QVERIFY(model.resetToDefault(model.indexOf(model.root()->child(0))));
    model.setRoot(createTree());
    QVERIFY(model.root()->remove(QStringLiteral("name")));
}

QTEST_APPLESS_MAIN(tst_PropertyModel)
#include "tst_propertymodel.moc"
