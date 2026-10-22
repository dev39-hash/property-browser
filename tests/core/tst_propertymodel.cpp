// PropertyModel.

#include <qpb/qpbcore.h>

#include <QAbstractItemModelTester>
#include <QLocale>
#include <QPointer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

#include <functional>
#include <memory>
#include <string>

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

// Reports when a property is destroyed: its validator owns a token that the
// returned pointer watches.
std::weak_ptr<int> trackLifetime(Property& property)
{
    auto token = std::make_shared<int>(0);
    property.setValidator([token](const QVariant&, const Property&) {
        return *token == 0 ? ValidationResult::valid() : ValidationResult::error(QString());
    });
    return token;
}

enum class TreeChange {
    ReplaceTree,
    RemoveGroup,
    RemoveProperty,
};

enum class Write {
    ModelSetValue,
    PropertySetValue,
    ModelSetData,
    ModelGroupReset,
    PropertyGroupReset,
    ResetAll,
    ExplicitBatch,
};

const QString XPath = QStringLiteral("Transform/x");

// Changes the tree createTree() made the way an application handler would.
void changeTree(PropertyModel& model, TreeChange change)
{
    switch (change) {
    case TreeChange::ReplaceTree:
        model.setRoot(createTree());
        break;
    case TreeChange::RemoveGroup:
        model.root()->remove(QStringLiteral("Transform"));
        break;
    case TreeChange::RemoveProperty:
        model.find(QStringLiteral("Transform"))->toGroup()->remove(QStringLiteral("x"));
        break;
    }
}

// A model over createTree() with "Transform/x" at 1.0 and "Transform/Pivot/z"
// at 2.0, and a handler that changes the tree when "Transform/x" changes (or
// when a batch ends). A second handler, connected after it, records what it
// receives.
struct HandlerScenario
{
    HandlerScenario()
        : model(createTree())
        , tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal)
    {
        model.find(XPath)->setValue(1.0);
        model.find(QStringLiteral("Transform/Pivot/z"))->setValue(2.0);
        lifetime = trackLifetime(*model.find(XPath));
    }

    // The tree changes once, from the first valueChanged() of "Transform/x".
    void armValueHandlers(TreeChange change)
    {
        QObject::connect(&model, &PropertyModel::valueChanged, &model,
            [this, change](const QString& path) {
                if (path == XPath && !treeChanged) {
                    treeChanged = true;
                    changeTree(model, change);
                }
            });
        QObject::connect(&model, &PropertyModel::valueChanged, &model,
            [this](const QString& path, const QVariant& newValue, const QVariant& oldValue) {
                if (calls++ > 0)
                    return;
                propertyAlive = !lifetime.expired();
                if (!propertyAlive)
                    return; // the values would refer to a destroyed property
                firstPath = path;
                firstNewValue = newValue;
                firstOldValue = oldValue;
            });
        model.onValueChanged(XPath, &model, [this](const QVariant& value) {
            if (callbackCalls++ == 0 && !lifetime.expired())
                callbackValue = value;
        });
    }

    // The tree changes once, from the first batchValueChanged().
    void armBatchHandlers(TreeChange change)
    {
        QObject::connect(&model, &PropertyModel::batchValueChanged, &model,
            [this, change](const QStringList&) {
                if (!treeChanged) {
                    treeChanged = true;
                    changeTree(model, change);
                }
            });
        QObject::connect(&model, &PropertyModel::batchValueChanged, &model,
            [this](const QStringList& paths) {
                ++batchCalls;
                batchPaths = paths;
            });
    }

    bool write(Write how)
    {
        Property* x = model.find(XPath);
        switch (how) {
        case Write::ModelSetValue:
            return model.setValue(XPath, 5.0);
        case Write::PropertySetValue:
            return x->setValue(5.0);
        case Write::ModelSetData:
            return model.setData(model.indexOf(x, PropertyModel::ValueColumn), 5.0);
        case Write::ModelGroupReset:
            return model.resetToDefault(model.indexOf(model.find(QStringLiteral("Transform"))));
        case Write::PropertyGroupReset:
            return model.find(QStringLiteral("Transform"))->resetToDefault();
        case Write::ResetAll:
            return model.resetAllToDefault();
        case Write::ExplicitBatch: {
            model.beginBatch();
            const bool ok = model.setValue(XPath, 5.0) && model.setValue(QStringLiteral("fov"), 90);
            model.endBatch();
            return ok;
        }
        }
        return false;
    }

    PropertyModel model;
    QAbstractItemModelTester tester;
    std::weak_ptr<int> lifetime;
    bool treeChanged = false;

    int calls = 0;
    bool propertyAlive = false;
    QString firstPath;
    QVariant firstNewValue;
    QVariant firstOldValue;
    int callbackCalls = 0;
    QVariant callbackValue;

    int batchCalls = 0;
    QStringList batchPaths;
};

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
    void resetAllToDefault();
    void liveIsNeverModified();
    void onValueChangedByPath();
    void onValueChangedForGroups();

    void modelTesterWholeLifecycle();

    void treeChangeFromValueHandler_data();
    void treeChangeFromValueHandler();
    void treeChangeFromBatchHandler_data();
    void treeChangeFromBatchHandler();
    void signalSpyAfterTreeChange();
    void detachedTreeLivesUntilTheOutermostAssignmentReturns();
    void detachedPartsLiveUntilTheAssignmentReturns_data();
    void detachedPartsLiveUntilTheAssignmentReturns();
    void detachedTreeIsDestroyedAtOnceOutsideHandlers();
    void treeChangeFromRejectionHandler();
    void repeatedTreeChangesInOneWrite();
    void modelDeletedByAValueHandler_data();
    void modelDeletedByAValueHandler();
    void destroyingADetachedPropertyDetachesMore();
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
// (an application can update its own read-only status field).
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

// Since 1.5: the whole tree, as application code (the root has no index, so
// resetToDefault(QModelIndex()) keeps returning false).
void tst_PropertyModel::resetAllToDefault()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    Property* name = model.find(QStringLiteral("name"));
    Property* x = model.find(QStringLiteral("Transform/x"));
    Property* z = model.find(QStringLiteral("Transform/Pivot/z"));
    Property* fov = model.find(QStringLiteral("fov"));
    Property& used = model.root()->addInt(QStringLiteral("used"), 0);
    QVERIFY(name->setValue(QStringLiteral("Other")));
    QVERIFY(x->setValue(1.0));
    QVERIFY(z->setValue(2.0));
    QVERIFY(fov->setValue(90));
    QVERIFY(used.setValue(5));
    used.setLive(true);
    x->setReadOnly(true);
    model.find(QStringLiteral("Transform/Pivot"))->setEnabled(false);

    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy batch(&model, &PropertyModel::batchValueChanged);
    QVERIFY(model.resetAllToDefault());
    QCOMPARE(name->value(), QVariant(QStringLiteral("Main")));
    QCOMPARE(x->value(), QVariant(0.0)); // read-only: reset by application code
    QCOMPARE(z->value(), QVariant(0.0)); // disabled ancestor: the same
    QCOMPARE(fov->value(), QVariant(60));
    QCOMPARE(used.value(), QVariant(5)); // live values are never user edits: left alone
    QCOMPARE(values.count(), 4);
    QCOMPARE(batch.count(), 1);
    QCOMPARE(batch.at(0).at(0).toStringList().size(), 4);

    // Nothing modified: no signal at all.
    QVERIFY(model.resetAllToDefault());
    QCOMPARE(batch.count(), 1);

    // A rejected reset makes it false; the other properties are still reset.
    QVERIFY(name->setValue(QStringLiteral("Other")));
    QVERIFY(fov->setValue(90));
    name->setValidator([](const QVariant& value, const Property&) {
        return value.toString() == QStringLiteral("Main")
            ? ValidationResult::error(QStringLiteral("Main is taken"))
            : ValidationResult::valid();
    });
    QSignalSpy failed(&model, &PropertyModel::validationFailed);
    QVERIFY(!model.resetAllToDefault());
    QCOMPARE(failed.count(), 1);
    QCOMPARE(name->value(), QVariant(QStringLiteral("Other")));
    QCOMPARE(fov->value(), QVariant(60));

    // The invalid index keeps its 1.0 meaning.
    QVERIFY(!model.resetToDefault(QModelIndex()));

    PropertyModel empty;
    QVERIFY(empty.resetAllToDefault());
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

// Since 1.3: views get IsModifiedRole false for live properties, and turning
// the flag on or off refreshes the row.
void tst_PropertyModel::liveIsNeverModified()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& used = root->addInt(QStringLiteral("used"), 0);
    PropertyModel model(std::move(root));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    const QModelIndex name = model.indexOf(&used);
    QVERIFY(model.setValue(QStringLiteral("used"), 5));
    QCOMPARE(name.data(PropertyModel::IsModifiedRole).toBool(), true);

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    used.setLive(true);
    QVERIFY(changed.count() >= 1);
    QCOMPARE(name.data(PropertyModel::IsModifiedRole).toBool(), false);
}

// Since 1.4: a callback per path instead of comparing paths in valueChanged.
void tst_PropertyModel::onValueChangedByPath()
{
    auto makeTree = [](int x) {
        auto root = PropertyGroup::create(QStringLiteral("root"));
        root->addGroup(QStringLiteral("t")).addInt(QStringLiteral("x"), x);
        root->addInt(QStringLiteral("other"), 0);
        return root;
    };
    PropertyModel model(makeTree(0));
    QVariantList received;
    const QMetaObject::Connection connection = model.onValueChanged(
        QStringLiteral("t/x"), this, [&received](const QVariant& value) { received << value; });
    QVERIFY(connection);

    QVERIFY(model.setValue(QStringLiteral("t/x"), 3)); // application
    QVERIFY(model.setValue(QStringLiteral("other"), 9)); // another path
    const QModelIndex x
        = model.indexOf(model.find(QStringLiteral("t/x")), PropertyModel::ValueColumn);
    QVERIFY(model.setData(x, 4)); // user
    QCOMPARE(received, QVariantList({3, 4}));

    // Follows the path across setRoot() and remove/add.
    model.setRoot(makeTree(0));
    QVERIFY(model.setValue(QStringLiteral("t/x"), 5));
    model.find(QStringLiteral("t"))->toGroup()->remove(QStringLiteral("x"));
    model.find(QStringLiteral("t"))->toGroup()->addInt(QStringLiteral("x"), 0);
    QVERIFY(model.setValue(QStringLiteral("t/x"), 6));
    QCOMPARE(received, QVariantList({3, 4, 5, 6}));

    // Disconnect, and the context ends the connection.
    QVERIFY(QObject::disconnect(connection));
    QVERIFY(model.setValue(QStringLiteral("t/x"), 7));
    QCOMPARE(received.size(), 4);

    auto context = std::make_unique<QObject>();
    int calls = 0;
    model.onValueChanged(
        QStringLiteral("t/x"), context.get(), [&calls](const QVariant&) { ++calls; });
    QVERIFY(model.setValue(QStringLiteral("t/x"), 8));
    context.reset();
    QVERIFY(model.setValue(QStringLiteral("t/x"), 9));
    QCOMPARE(calls, 1);

    // No context: lives as long as the model. An empty handler does nothing.
    int modelCalls = 0;
    QVERIFY(model.onValueChanged(
        QStringLiteral("other"), nullptr, [&modelCalls](const QVariant&) { ++modelCalls; }));
    QVERIFY(model.setValue(QStringLiteral("other"), 1));
    QCOMPARE(modelCalls, 1);
    QVERIFY(!model.onValueChanged(
        QStringLiteral("other"), this, std::function<void(const QVariant&)>()));
}

void tst_PropertyModel::onValueChangedForGroups()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& t = root->addGroup(QStringLiteral("t"));
    t.addInt(QStringLiteral("x"), 0);
    t.addGroup(QStringLiteral("deep")).addInt(QStringLiteral("y"), 0);
    root->addInt(QStringLiteral("tx"), 0); // "t" is a prefix of "tx", not its parent
    PropertyModel model(std::move(root));

    QStringList underT;
    QStringList all;
    model.onValueChanged(QStringLiteral("t"), this,
        [&underT](const QString& path, const QVariant&) { underT << path; });
    model.onValueChanged(
        QString(), this, [&all](const QString& path, const QVariant&) { all << path; });
    QVERIFY(model.setValue(QStringLiteral("t/x"), 1));
    QVERIFY(model.setValue(QStringLiteral("t/deep/y"), 2));
    QVERIFY(model.setValue(QStringLiteral("tx"), 3));
    QCOMPARE(underT, QStringList({"t/x", "t/deep/y"}));
    QCOMPARE(all, QStringList({"t/x", "t/deep/y", "tx"}));
}

namespace {

struct ChangeRow
{
    const char* name;
    TreeChange change;
};

struct WriteRow
{
    const char* name;
    Write write;
};

const ChangeRow changeRows[] = {
    {"replaceTree", TreeChange::ReplaceTree},
    {"removeGroup", TreeChange::RemoveGroup},
    {"removeProperty", TreeChange::RemoveProperty},
};

void checkTreeAfter(PropertyModel& model, TreeChange change)
{
    switch (change) {
    case TreeChange::ReplaceTree:
        QVERIFY(model.find(XPath));
        QCOMPARE(model.find(XPath)->value(), QVariant(0.0));
        QCOMPARE(model.find(QStringLiteral("Transform/Pivot/z"))->value(), QVariant(0.0));
        QCOMPARE(model.find(QStringLiteral("fov"))->value(), QVariant(60));
        break;
    case TreeChange::RemoveGroup:
        QVERIFY(!model.find(QStringLiteral("Transform")));
        QVERIFY(model.find(QStringLiteral("fov")));
        break;
    case TreeChange::RemoveProperty:
        QVERIFY(model.find(QStringLiteral("Transform")));
        QVERIFY(!model.find(XPath));
        break;
    }
}

} // namespace

void tst_PropertyModel::treeChangeFromValueHandler_data()
{
    QTest::addColumn<int>("change");
    QTest::addColumn<int>("write");
    const WriteRow writes[] = {
        {"modelSetValue", Write::ModelSetValue},
        {"propertySetValue", Write::PropertySetValue},
        {"modelSetData", Write::ModelSetData},
        {"modelGroupReset", Write::ModelGroupReset},
        {"propertyGroupReset", Write::PropertyGroupReset},
        {"resetAll", Write::ResetAll},
    };
    for (const ChangeRow& change : changeRows) {
        for (const WriteRow& write : writes) {
            const std::string name = std::string(change.name) + "/" + write.name;
            QTest::newRow(name.c_str()) << int(change.change) << int(write.write);
        }
    }
}

// A handler of valueChanged() may replace the tree or remove the assigned
// property's group (or the property): handlers connected after it, and the
// write that triggered the change, carry on with intact values.
void tst_PropertyModel::treeChangeFromValueHandler()
{
    QFETCH(int, change);
    QFETCH(int, write);
    HandlerScenario s;
    s.armValueHandlers(TreeChange(change));

    QVERIFY(s.write(Write(write)));

    QVERIFY(s.treeChanged);
    QVERIFY(s.calls >= 1);
    QVERIFY2(s.propertyAlive,
        "the assigned property was destroyed while a later handler of the same change ran");
    const bool isReset = Write(write) == Write::ModelGroupReset
        || Write(write) == Write::PropertyGroupReset || Write(write) == Write::ResetAll;
    QCOMPARE(s.firstPath, XPath);
    QCOMPARE(s.firstNewValue, QVariant(isReset ? 0.0 : 5.0));
    QCOMPARE(s.firstOldValue, QVariant(1.0));
    QCOMPARE(s.callbackValue, QVariant(isReset ? 0.0 : 5.0));
    QVERIFY2(s.lifetime.expired(), "the detached property is destroyed once the write returned");
    checkTreeAfter(s.model, TreeChange(change));
}

void tst_PropertyModel::treeChangeFromBatchHandler_data()
{
    QTest::addColumn<int>("change");
    QTest::addColumn<int>("write");
    const WriteRow writes[] = {
        {"modelGroupReset", Write::ModelGroupReset},
        {"propertyGroupReset", Write::PropertyGroupReset},
        {"resetAll", Write::ResetAll},
        {"explicitBatch", Write::ExplicitBatch},
    };
    for (const ChangeRow& change : changeRows) {
        for (const WriteRow& write : writes) {
            const std::string name = std::string(change.name) + "/" + write.name;
            QTest::newRow(name.c_str()) << int(change.change) << int(write.write);
        }
    }
}

// The same for a handler of batchValueChanged().
void tst_PropertyModel::treeChangeFromBatchHandler()
{
    QFETCH(int, change);
    QFETCH(int, write);
    HandlerScenario s;
    s.armBatchHandlers(TreeChange(change));

    QVERIFY(s.write(Write(write)));

    QVERIFY(s.treeChanged);
    QCOMPARE(s.batchCalls, 1);
    const QStringList expected = Write(write) == Write::ExplicitBatch
        ? QStringList {XPath, QStringLiteral("fov")}
        : QStringList {XPath, QStringLiteral("Transform/Pivot/z")};
    QCOMPARE(s.batchPaths, expected);
    QVERIFY2(s.lifetime.expired(), "the detached property is destroyed once the write returned");
    checkTreeAfter(s.model, TreeChange(change));
}

// A signal spy connected after the handler that replaces the tree stores the
// signal's arguments.
void tst_PropertyModel::signalSpyAfterTreeChange()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObject::connect(&model, &PropertyModel::valueChanged, &model, [&model](const QString& path) {
        if (path == XPath)
            model.setRoot(createTree());
    });
    QSignalSpy spy(&model, &PropertyModel::valueChanged);

    QVERIFY(model.setValue(XPath, 5.0));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), XPath);
    QCOMPARE(spy.at(0).at(1), QVariant(5.0));
    QCOMPARE(spy.at(0).at(2), QVariant(0.0));
}

// A tree detached by a handler stays alive until the outermost write returns,
// even when the handler runs inside a nested write.
void tst_PropertyModel::detachedTreeLivesUntilTheOutermostAssignmentReturns()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    const std::weak_ptr<int> fovLife = trackLifetime(*model.find(QStringLiteral("fov")));
    const std::weak_ptr<int> groupLife = trackLifetime(*model.find(QStringLiteral("Transform")));
    const std::weak_ptr<int> xLife = trackLifetime(*model.find(XPath));

    bool aliveAfterReplace = false;
    bool aliveAfterNestedWrite = false;
    QObject::connect(&model, &PropertyModel::valueChanged, &model,
        [&](const QString& path) {
            if (path == QStringLiteral("fov")) {
                model.setValue(XPath, 5.0); // nested: its handler replaces the tree
                aliveAfterNestedWrite
                    = !fovLife.expired() && !groupLife.expired() && !xLife.expired();
            } else if (path == XPath) {
                model.setRoot(createTree());
                aliveAfterReplace = !fovLife.expired() && !xLife.expired();
            }
        });

    QVERIFY(model.setValue(QStringLiteral("fov"), 90));

    QVERIFY2(aliveAfterReplace, "the old tree was destroyed inside the nested write");
    QVERIFY2(aliveAfterNestedWrite, "the old tree was destroyed before the outer write returned");
    QVERIFY(fovLife.expired());
    QVERIFY(groupLife.expired());
    QVERIFY(xLife.expired());
    QCOMPARE(model.find(QStringLiteral("fov"))->value(), QVariant(60));

    // The model is back to destroying at once.
    const std::weak_ptr<int> later = trackLifetime(*model.find(QStringLiteral("fov")));
    model.setRoot(createTree());
    QVERIFY(later.expired());
}

void tst_PropertyModel::detachedPartsLiveUntilTheAssignmentReturns_data()
{
    QTest::addColumn<int>("change");
    for (const ChangeRow& change : changeRows)
        QTest::newRow(change.name) << int(change.change);
}

void tst_PropertyModel::detachedPartsLiveUntilTheAssignmentReturns()
{
    QFETCH(int, change);
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    const std::weak_ptr<int> xLife = trackLifetime(*model.find(XPath));
    const std::weak_ptr<int> groupLife = trackLifetime(*model.find(QStringLiteral("Transform")));
    const PropertyGroup* oldRoot = model.root();

    bool structureChangedAtOnce = false;
    bool aliveInHandler = false;
    QObject::connect(&model, &PropertyModel::valueChanged, &model, [&](const QString& path) {
        if (path != XPath)
            return;
        changeTree(model, TreeChange(change));
        structureChangedAtOnce = TreeChange(change) == TreeChange::ReplaceTree
            ? model.root() != oldRoot
            : model.find(XPath) == nullptr;
        aliveInHandler = !xLife.expired() && !groupLife.expired();
    });

    QVERIFY(model.setValue(XPath, 5.0));

    QVERIFY2(structureChangedAtOnce, "the tree change is visible to the handler at once");
    QVERIFY2(aliveInHandler, "the detached part was destroyed inside the handler");
    QVERIFY(xLife.expired());
    QVERIFY2(groupLife.expired() == (TreeChange(change) != TreeChange::RemoveProperty),
        "the detached group is destroyed with the write; a group that stays is not");
}

void tst_PropertyModel::detachedTreeIsDestroyedAtOnceOutsideHandlers()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);

    std::weak_ptr<int> life = trackLifetime(*model.find(XPath));
    model.setRoot(createTree());
    QVERIFY(life.expired());

    life = trackLifetime(*model.find(QStringLiteral("Transform")));
    QVERIFY(model.root()->remove(QStringLiteral("Transform")));
    QVERIFY(life.expired());

    // Handlers that do not change the tree do not delay later removals.
    QObject::connect(&model, &PropertyModel::valueChanged, &model, [](const QString&) { });
    QVERIFY(model.setValue(QStringLiteral("fov"), 70));
    life = trackLifetime(*model.find(QStringLiteral("fov")));
    QVERIFY(model.root()->remove(QStringLiteral("fov")));
    QVERIFY(life.expired());
}

// A handler of validationFailed() may replace the tree as well; the rejected
// property stays alive until the write returns.
void tst_PropertyModel::treeChangeFromRejectionHandler()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    auto token = std::make_shared<int>(0);
    const std::weak_ptr<int> life = token;
    model.find(XPath)->setValidator([token](const QVariant& value, const Property&) {
        return *token == 0 && value.toDouble() > 5.0
            ? ValidationResult::error(QStringLiteral("too big"))
                                        : ValidationResult::valid();
    });
    token.reset();
    bool aliveInLaterHandler = false;
    QString message;
    QObject::connect(&model, &PropertyModel::validationFailed, &model,
        [&model](const QString&) { model.setRoot(createTree()); });
    QObject::connect(&model, &PropertyModel::validationFailed, &model,
        [&](const QString&, const QVariant&, const QString& text) {
            aliveInLaterHandler = !life.expired();
            message = text;
        });

    QVERIFY(!model.setValue(XPath, 8.0));

    QVERIFY2(aliveInLaterHandler, "the rejected property was destroyed inside the signal");
    QCOMPARE(message, QStringLiteral("too big"));
    QVERIFY(life.expired());
    QVERIFY(model.find(XPath));
}

// Several tree changes in one write, from different handlers and a handler
// that removes a part of the group a reset is walking.
void tst_PropertyModel::repeatedTreeChangesInOneWrite()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    model.find(XPath)->setValue(1.0);
    model.find(QStringLiteral("fov"))->setValue(90);
    const std::weak_ptr<int> life = trackLifetime(*model.find(XPath));
    int replacements = 0;
    const auto replace = [&](const QString& path) {
        if (path == XPath) {
            ++replacements;
            model.setRoot(createTree());
        }
    };
    QObject::connect(&model, &PropertyModel::valueChanged, &model, replace);
    QObject::connect(&model, &PropertyModel::valueChanged, &model, replace);

    QVERIFY(model.setValue(XPath, 5.0));
    QCOMPARE(replacements, 2);
    QVERIFY(life.expired());
    QCOMPARE(model.find(XPath)->value(), QVariant(0.0));

    // A reset walking a group while a handler removes a sibling subtree.
    PropertyModel other(createTree());
    QAbstractItemModelTester otherTester(
        &other, QAbstractItemModelTester::FailureReportingMode::Fatal);
    other.find(XPath)->setValue(1.0);
    other.find(QStringLiteral("Transform/Pivot/z"))->setValue(2.0);
    other.find(QStringLiteral("fov"))->setValue(90);
    QObject::connect(&other, &PropertyModel::valueChanged, &other, [&other](const QString& path) {
        if (path == XPath)
            other.find(QStringLiteral("Transform"))->toGroup()->remove(QStringLiteral("Pivot"));
    });
    QVERIFY(other.resetAllToDefault());
    QVERIFY(!other.find(QStringLiteral("Transform/Pivot")));
    QCOMPARE(other.find(XPath)->value(), QVariant(0.0));
    QCOMPARE(other.find(QStringLiteral("fov"))->value(), QVariant(60));
}

void tst_PropertyModel::modelDeletedByAValueHandler_data()
{
    QTest::addColumn<bool>("throughSetData");
    QTest::newRow("setValue") << false;
    QTest::newRow("setData") << true;
}

// A slot may delete the model that is notifying it; the write returns without
// touching the model again.
void tst_PropertyModel::modelDeletedByAValueHandler()
{
    QFETCH(bool, throughSetData);
    auto* model = new PropertyModel(createTree());
    const QPointer<PropertyModel> guard(model);
    int calls = 0;
    QObject::connect(model, &PropertyModel::valueChanged, [&calls, model] {
        ++calls;
        delete model;
    });
    bool ok = false;
    if (throughSetData)
        ok = model->setData(valueIndex(*model, XPath), 5.0, Qt::EditRole);
    else
        ok = model->setValue(XPath, 5.0);

    QVERIFY(ok);
    QCOMPARE(calls, 1);
    QVERIFY(guard.isNull());
}

namespace {

// Runs an action when it is destroyed.
struct DestructorAction
{
    ~DestructorAction()
    {
        if (action)
            action();
    }
    std::function<void()> action;
};

} // namespace

// A detached property is destroyed when the outermost write returns; its
// destructor may detach more of the tree, which is destroyed at once.
void tst_PropertyModel::destroyingADetachedPropertyDetachesMore()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    const std::weak_ptr<int> fovLife = trackLifetime(*model.find(QStringLiteral("fov")));
    {
        auto remover = std::make_shared<DestructorAction>();
        remover->action = [&model] { model.root()->remove(QStringLiteral("fov")); };
        model.find(XPath)->setValidator([remover](const QVariant&, const Property&) {
            return ValidationResult::valid();
        });
    }
    QObject::connect(&model, &PropertyModel::valueChanged, &model, [&model](const QString& path) {
        if (path == XPath)
            model.root()->remove(QStringLiteral("Transform"));
    });

    QVERIFY(model.setValue(XPath, 5.0));

    QVERIFY(!model.find(QStringLiteral("Transform")));
    QVERIFY(!model.find(QStringLiteral("fov")));
    QVERIFY(fovLife.expired());
    QVERIFY(model.find(QStringLiteral("name")));
}

QTEST_APPLESS_MAIN(tst_PropertyModel)
#include "tst_propertymodel.moc"
