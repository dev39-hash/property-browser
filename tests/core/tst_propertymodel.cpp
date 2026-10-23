// PropertyModel.

#include <qpb/qpbcore.h>

#include <QAbstractItemModelTester>
#include <QLocale>
#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
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

    // A handler deletes the model while a group walk is running.
    void modelDeletedDuringAGroupWalk_data();
    void modelDeletedDuringAGroupWalk();
    void modelDeletedInANestedWalk();
    void modelDeletedByADataChangedHandler_data();
    void modelDeletedByADataChangedHandler();

    // Edits made by the user are announced apart from application writes.
    void setDataEmitsUserValueChanged();
    void checkStateEmitsUserValueChanged();
    void applicationWritesEmitNoUserValueChanged();
    void rejectedAndUnchangedSetDataEmitNoUserValueChanged();
    void userValueChangedInsideABatch();
    void userValueChangedSlotMayWriteBack();
    void onUserValueChangedByPath();
    void onUserValueChangedForGroups();
    void treeChangeFromUserValueHandler_data();
    void treeChangeFromUserValueHandler();
    void userValueChangedFollowsATreeChangeInValueChanged_data();
    void userValueChangedFollowsATreeChangeInValueChanged();
    void signalSpyAfterTreeChangeInUserValueChanged();
    void modelDeletedByAUserValueHandler();

    // Normalising values and making them the defaults.
    void normalizeValueBringsAValueInsideItsRange();
    void normalizeValueRoundsAndReportsRejections();
    void normalizeValueOnAGroupRunsInOneBatch();
    void normalizeAllValuesCoversTheWholeTree();
    void makeValueDefaultNotifiesTheRow();
    void makeValueDefaultOnGroupsAndTheModel();
    void attributeChangesNeverMoveValues();

    // Doubles stored without rounding.
    void unroundedDoubleKeepsEveryDigitThroughTheModel();
    void unroundedDoubleStillClampsThroughTheModel();
    void unroundedDoubleResetsToItsFullDefault();
    void roundingDoubleResetsToItsRoundedDefault();
    void unroundedDoubleSurvivesNormalizingAndDefaults();
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

// --- User edits ----------------------------------------------------------------------

namespace {

// Records, in order, which value signals of a model arrive.
struct SignalLog
{
    explicit SignalLog(PropertyModel& model)
    {
        QObject::connect(&model, &PropertyModel::valueChanged, &model,
            [this](const QString& path) { events << QStringLiteral("value:") + path; });
        QObject::connect(&model, &PropertyModel::userValueChanged, &model,
            [this](const QString& path) { events << QStringLiteral("user:") + path; });
    }

    QStringList events;
};

} // namespace

// An edit through setData() is announced by valueChanged() and then by
// userValueChanged(), with the stored (converted, clamped) values.
void tst_PropertyModel::setDataEmitsUserValueChanged()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    SignalLog log(model);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);

    const QModelIndex x = valueIndex(model, QStringLiteral("Transform/x"));
    QVERIFY(model.setData(x, 3.5));
    QCOMPARE(log.events,
        QStringList({QStringLiteral("value:Transform/x"), QStringLiteral("user:Transform/x")}));
    QCOMPARE(user.count(), 1);
    QCOMPARE(user.at(0).at(0).toString(), QStringLiteral("Transform/x"));
    QCOMPARE(user.at(0).at(1), QVariant(3.5));
    QCOMPARE(user.at(0).at(2), QVariant(0.0));

    // The value after normalisation, and the value before the edit.
    QVERIFY(model.setData(x, 99.0));
    QCOMPARE(user.count(), 2);
    QCOMPARE(user.at(1).at(1), QVariant(10.0));
    QCOMPARE(user.at(1).at(2), QVariant(3.5));

    // The value after conversion.
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("fov")), QStringLiteral("75")));
    QCOMPARE(user.count(), 3);
    QCOMPARE(user.at(2).at(0).toString(), QStringLiteral("fov"));
    QCOMPARE(user.at(2).at(1), QVariant(75));
    QCOMPARE(user.at(2).at(2), QVariant(60));

    QVERIFY(model.setData(valueIndex(model, QStringLiteral("name")), QStringLiteral("Rim")));
    QCOMPARE(user.count(), 4);
    QCOMPARE(user.at(3).at(1), QVariant(QStringLiteral("Rim")));
    QCOMPARE(user.at(3).at(2), QVariant(QStringLiteral("Main")));
    QCOMPARE(log.events.size(), 8);
}

void tst_PropertyModel::checkStateEmitsUserValueChanged()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    SignalLog log(model);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    const QModelIndex visible = valueIndex(model, QStringLiteral("visible"));

    QVERIFY(model.setData(visible, Qt::Unchecked, Qt::CheckStateRole));
    QCOMPARE(log.events,
        QStringList({QStringLiteral("value:visible"), QStringLiteral("user:visible")}));
    QCOMPARE(user.count(), 1);
    QCOMPARE(user.at(0).at(0).toString(), QStringLiteral("visible"));
    QCOMPARE(user.at(0).at(1), QVariant(false));
    QCOMPARE(user.at(0).at(2), QVariant(true));

    // The same state again is not an edit.
    QVERIFY(model.setData(visible, Qt::Unchecked, Qt::CheckStateRole));
    QCOMPARE(user.count(), 1);

    QVERIFY(model.setData(visible, Qt::Checked, Qt::CheckStateRole));
    QCOMPARE(user.count(), 2);
    QCOMPARE(user.at(1).at(1), QVariant(true));
    QCOMPARE(user.at(1).at(2), QVariant(false));
}

// Guard: application code writes through setValue(), the reset functions and
// Property::setValue(); none of them is a user edit. The value count proves
// the writes happened.
void tst_PropertyModel::applicationWritesEmitNoUserValueChanged()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);

    QVERIFY(model.setValue(QStringLiteral("fov"), 90)); // 1
    QVERIFY(model.find(QStringLiteral("name"))->setValue(QStringLiteral("Other"))); // 2
    QVERIFY(model.find(QStringLiteral("Transform/x"))->setValue(1.0)); // 3
    QVERIFY(model.find(QStringLiteral("Transform/Pivot/z"))->setValue(2.0)); // 4
    QVERIFY(model.resetToDefault(valueIndex(model, QStringLiteral("fov")))); // 5
    QVERIFY(model.resetToDefault(model.indexOf(model.find(QStringLiteral("Transform"))))); // 6, 7
    QVERIFY(model.resetAllToDefault()); // 8: the name
    QVERIFY(model.find(QStringLiteral("Transform"))->resetToDefault()); // nothing left to reset

    // Read-only and disabled properties accept application writes as well.
    model.find(QStringLiteral("visible"))->setReadOnly(true);
    QVERIFY(model.setValue(QStringLiteral("visible"), false)); // 9

    QCOMPARE(values.count(), 9);
    QCOMPARE(user.count(), 0);
}

// A rejected edit, an unchanged edit and an edit refused as read-only or
// disabled change nothing and announce nothing; the edit that follows does.
void tst_PropertyModel::rejectedAndUnchangedSetDataEmitNoUserValueChanged()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    const QModelIndex fov = valueIndex(model, QStringLiteral("fov"));

    QVERIFY(model.setData(fov, 60));
    QVERIFY(model.setData(fov, QStringLiteral("60")));
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("Transform/x")), 0.0));
    QVERIFY(!model.setData(fov, QStringLiteral("wide")));
    QVERIFY(!model.setData(fov, 90, Qt::DisplayRole));
    QVERIFY(!model.setData(fov.siblingAtColumn(0), 90));
    QVERIFY(!model.setData(model.index(2, 1), 1)); // a group
    model.find(QStringLiteral("name"))->setReadOnly(true);
    QVERIFY(!model.setData(valueIndex(model, QStringLiteral("name")), QStringLiteral("Rim")));
    model.find(QStringLiteral("visible"))->setEnabled(false);
    QVERIFY(!model.setData(
        valueIndex(model, QStringLiteral("visible")), Qt::Unchecked, Qt::CheckStateRole));
    QCOMPARE(values.count(), 0);
    QCOMPARE(user.count(), 0);

    QVERIFY(model.setData(fov, 70));
    QCOMPARE(values.count(), 1);
    QCOMPARE(user.count(), 1);
    QCOMPARE(user.at(0).at(0).toString(), QStringLiteral("fov"));
}

// Inside a batch, valueChanged() and userValueChanged() still come per change;
// batchValueChanged() lists every written path once the batch ends.
void tst_PropertyModel::userValueChangedInsideABatch()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    QSignalSpy batch(&model, &PropertyModel::batchValueChanged);

    model.beginBatch();
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("fov")), 90));
    QVERIFY(model.setValue(QStringLiteral("Transform/y"), 2.0));
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("Transform/x")), 1.0));
    QCOMPARE(user.count(), 2); // already delivered, not held back until the end
    QCOMPARE(batch.count(), 0);
    model.endBatch();

    QCOMPARE(values.count(), 3);
    QCOMPARE(user.count(), 2);
    QCOMPARE(user.at(0).at(0).toString(), QStringLiteral("fov"));
    QCOMPARE(user.at(0).at(1), QVariant(90));
    QCOMPARE(user.at(0).at(2), QVariant(60));
    QCOMPARE(user.at(1).at(0).toString(), QStringLiteral("Transform/x"));
    QCOMPARE(user.at(1).at(1), QVariant(1.0));
    QCOMPARE(user.at(1).at(2), QVariant(0.0));
    QCOMPARE(batch.count(), 1);
    QCOMPARE(batch.at(0).at(0).toStringList(),
        (QStringList {QStringLiteral("fov"), QStringLiteral("Transform/y"),
            QStringLiteral("Transform/x")}));
}

// The typical use: an application vetoes a user edit by writing the old value
// back. The rollback is an application write, so it does not echo.
void tst_PropertyModel::userValueChangedSlotMayWriteBack()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    int slotCalls = 0;
    QObject::connect(&model, &PropertyModel::userValueChanged, &model,
        [&model, &slotCalls](const QString& path, const QVariant&, const QVariant& oldValue) {
            ++slotCalls;
            model.setValue(path, oldValue);
        });

    QVERIFY(model.setData(valueIndex(model, QStringLiteral("fov")), 90));

    QCOMPARE(slotCalls, 1);
    QCOMPARE(user.count(), 1);
    QCOMPARE(values.count(), 2); // the edit and the rollback
    QCOMPARE(values.at(1).at(1), QVariant(60));
    QCOMPARE(values.at(1).at(2), QVariant(90));
    QCOMPARE(model.find(QStringLiteral("fov"))->value(), QVariant(60));
}

// A callback per path for user edits only.
void tst_PropertyModel::onUserValueChangedByPath()
{
    auto makeTree = [](int x) {
        auto root = PropertyGroup::create(QStringLiteral("root"));
        root->addGroup(QStringLiteral("t")).addInt(QStringLiteral("x"), x);
        root->addInt(QStringLiteral("other"), 0);
        return root;
    };
    PropertyModel model(makeTree(0));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    const auto x = [&model] {
        return model.indexOf(model.find(QStringLiteral("t/x")), PropertyModel::ValueColumn);
    };
    QVariantList received;
    const QMetaObject::Connection connection = model.onUserValueChanged(
        QStringLiteral("t/x"), this, [&received](const QVariant& value) { received << value; });
    QVERIFY(connection);

    QVERIFY(model.setValue(QStringLiteral("t/x"), 3)); // application: ignored
    QVERIFY(model.find(QStringLiteral("t/x"))->setValue(4)); // application: ignored
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("other")), 9)); // another path
    QVERIFY(model.setData(x(), 5)); // user
    QVERIFY(model.setData(x(), 5)); // unchanged: nothing
    QCOMPARE(received, (QVariantList {5}));

    // Follows the path across setRoot() and remove/add.
    model.setRoot(makeTree(0));
    QVERIFY(model.setData(x(), 6));
    model.find(QStringLiteral("t"))->toGroup()->remove(QStringLiteral("x"));
    model.find(QStringLiteral("t"))->toGroup()->addInt(QStringLiteral("x"), 0);
    QVERIFY(model.setData(x(), 7));
    QCOMPARE(received, QVariantList({5, 6, 7}));

    // Disconnect, and the context ends the connection.
    QVERIFY(QObject::disconnect(connection));
    QVERIFY(model.setData(x(), 8));
    QCOMPARE(received.size(), 3);

    auto context = std::make_unique<QObject>();
    int calls = 0;
    QVERIFY(model.onUserValueChanged(
        QStringLiteral("t/x"), context.get(), [&calls](const QVariant&) { ++calls; }));
    QVERIFY(model.setData(x(), 9));
    context.reset();
    QVERIFY(model.setData(x(), 10));
    QCOMPARE(calls, 1);

    // No context: lives as long as the model. An empty handler does nothing.
    int modelCalls = 0;
    QVERIFY(model.onUserValueChanged(
        QStringLiteral("other"), nullptr, [&modelCalls](const QVariant&) { ++modelCalls; }));
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("other")), 1));
    QVERIFY(model.setValue(QStringLiteral("other"), 2));
    QCOMPARE(modelCalls, 1);
    QVERIFY(!model.onUserValueChanged(
        QStringLiteral("other"), this, std::function<void(const QVariant&)>()));
    QVERIFY(!model.onUserValueChanged(
        QStringLiteral("other"), this, std::function<void(const QString&, const QVariant&)>()));
}

// The (path, value) form covers a path and its descendants.
void tst_PropertyModel::onUserValueChangedForGroups()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& t = root->addGroup(QStringLiteral("t"));
    t.addInt(QStringLiteral("x"), 0);
    t.addGroup(QStringLiteral("deep")).addInt(QStringLiteral("y"), 0);
    root->addInt(QStringLiteral("tx"), 0); // "t" is a prefix of "tx", not its parent
    PropertyModel model(std::move(root));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);

    QStringList underT;
    QStringList all;
    QStringList exact;
    model.onUserValueChanged(QStringLiteral("t"), this,
        [&underT](const QString& path, const QVariant& value) {
            underT << path + QLatin1Char('=') + value.toString();
        });
    model.onUserValueChanged(
        QString(), this, [&all](const QString& path, const QVariant&) { all << path; });
    model.onUserValueChanged(QStringLiteral("tx"), this,
        [&exact](const QString& path, const QVariant&) { exact << path; });

    QVERIFY(model.setValue(QStringLiteral("t/x"), 1)); // application: ignored
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("t/x")), 2));
    QVERIFY(model.setValue(QStringLiteral("t/deep/y"), 3)); // application: ignored
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("t/deep/y")), 4));
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("tx")), 5));
    QCOMPARE(underT, QStringList({"t/x=2", "t/deep/y=4"}));
    QCOMPARE(all, QStringList({"t/x", "t/deep/y", "tx"}));
    QCOMPARE(exact, QStringList {QStringLiteral("tx")});
}

void tst_PropertyModel::treeChangeFromUserValueHandler_data()
{
    QTest::addColumn<int>("change");
    for (const ChangeRow& change : changeRows)
        QTest::newRow(change.name) << int(change.change);
}

// A slot of userValueChanged() may replace the tree or remove the edited
// property's group (or the property): slots connected after it, and the write
// that triggered the change, carry on with intact values.
void tst_PropertyModel::treeChangeFromUserValueHandler()
{
    QFETCH(int, change);
    HandlerScenario s;
    QObject::connect(&s.model, &PropertyModel::userValueChanged, &s.model,
        [&s, change](const QString& path) {
            if (path == XPath && !s.treeChanged) {
                s.treeChanged = true;
                changeTree(s.model, TreeChange(change));
            }
        });
    QObject::connect(&s.model, &PropertyModel::userValueChanged, &s.model,
        [&s](const QString& path, const QVariant& newValue, const QVariant& oldValue) {
            if (s.calls++ > 0)
                return;
            s.propertyAlive = !s.lifetime.expired();
            if (!s.propertyAlive)
                return; // the values would refer to a destroyed property
            s.firstPath = path;
            s.firstNewValue = newValue;
            s.firstOldValue = oldValue;
        });
    s.model.onUserValueChanged(XPath, &s.model, [&s](const QVariant& value) {
        if (s.callbackCalls++ == 0 && !s.lifetime.expired())
            s.callbackValue = value;
    });

    QVERIFY(s.model.setData(valueIndex(s.model, XPath), 5.0));

    QVERIFY(s.treeChanged);
    QCOMPARE(s.calls, 1);
    QVERIFY2(s.propertyAlive,
        "the edited property was destroyed while a later slot of the same change ran");
    QCOMPARE(s.firstPath, XPath);
    QCOMPARE(s.firstNewValue, QVariant(5.0));
    QCOMPARE(s.firstOldValue, QVariant(1.0));
    QCOMPARE(s.callbackCalls, 1);
    QCOMPARE(s.callbackValue, QVariant(5.0));
    QVERIFY2(s.lifetime.expired(), "the detached property is destroyed once the write returned");
    checkTreeAfter(s.model, TreeChange(change));
}

void tst_PropertyModel::userValueChangedFollowsATreeChangeInValueChanged_data()
{
    QTest::addColumn<int>("change");
    for (const ChangeRow& change : changeRows)
        QTest::newRow(change.name) << int(change.change);
}

// A slot of valueChanged() that changes the tree does not swallow the
// userValueChanged() of the same edit, and its arguments stay valid.
void tst_PropertyModel::userValueChangedFollowsATreeChangeInValueChanged()
{
    QFETCH(int, change);
    HandlerScenario s;
    s.armValueHandlers(TreeChange(change));
    int userCalls = 0;
    bool userAlive = false;
    QString userPath;
    QVariant userNew;
    QVariant userOld;
    QVariant userCallbackValue;
    QObject::connect(&s.model, &PropertyModel::userValueChanged, &s.model,
        [&](const QString& path, const QVariant& newValue, const QVariant& oldValue) {
            if (userCalls++ > 0)
                return;
            userAlive = !s.lifetime.expired();
            if (!userAlive)
                return;
            userPath = path;
            userNew = newValue;
            userOld = oldValue;
        });
    s.model.onUserValueChanged(
        XPath, &s.model, [&](const QVariant& value) { userCallbackValue = value; });

    QVERIFY(s.model.setData(valueIndex(s.model, XPath), 5.0));

    QVERIFY(s.treeChanged);
    QCOMPARE(userCalls, 1);
    QVERIFY2(userAlive, "the edited property was destroyed before userValueChanged() was delivered");
    QCOMPARE(userPath, XPath);
    QCOMPARE(userNew, QVariant(5.0));
    QCOMPARE(userOld, QVariant(1.0));
    QCOMPARE(userCallbackValue, QVariant(5.0));
    QVERIFY(s.lifetime.expired());
    checkTreeAfter(s.model, TreeChange(change));
}

// A signal spy connected after the slot that replaces the tree stores the
// signal's arguments.
void tst_PropertyModel::signalSpyAfterTreeChangeInUserValueChanged()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObject::connect(
        &model, &PropertyModel::userValueChanged, &model, [&model](const QString& path) {
            if (path == XPath)
                model.setRoot(createTree());
        });
    QSignalSpy spy(&model, &PropertyModel::userValueChanged);

    QVERIFY(model.setData(valueIndex(model, XPath), 5.0));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), XPath);
    QCOMPARE(spy.at(0).at(1), QVariant(5.0));
    QCOMPARE(spy.at(0).at(2), QVariant(0.0));
    QCOMPARE(model.find(XPath)->value(), QVariant(0.0)); // the new tree
}

// A slot may delete the model that is notifying it; the write returns without
// touching the model again.
void tst_PropertyModel::modelDeletedByAUserValueHandler()
{
    auto* model = new PropertyModel(createTree());
    const QPointer<PropertyModel> guard(model);
    int calls = 0;
    QObject::connect(model, &PropertyModel::userValueChanged, [&calls, model] {
        ++calls;
        delete model;
    });

    const bool ok = model->setData(valueIndex(*model, XPath), 5.0, Qt::EditRole);

    QVERIFY(ok);
    QCOMPARE(calls, 1);
    QVERIFY(guard.isNull());
}

// --- Normalising values and making them the defaults ---------------------------------------

// A value stored before a range changed is brought inside it again; the
// change is an application write.
void tst_PropertyModel::normalizeValueBringsAValueInsideItsRange()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    Property* fov = model.find(QStringLiteral("fov"));
    QVERIFY(fov->setValue(150));
    fov->setAttribute(Attr::Maximum, 100);
    QCOMPARE(fov->value(), QVariant(150)); // the attribute alone moves nothing

    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    QSignalSpy data(&model, &QAbstractItemModel::dataChanged);

    QVERIFY(fov->normalizeValue());
    QCOMPARE(fov->value(), QVariant(100));
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(0).toString(), QStringLiteral("fov"));
    QCOMPARE(values.at(0).at(1), QVariant(100));
    QCOMPARE(values.at(0).at(2), QVariant(150));
    QCOMPARE(user.count(), 0);
    QVERIFY(data.count() >= 1);
    QCOMPARE(model.indexOf(fov).data(PropertyModel::IsModifiedRole).toBool(), true);

    // Inside the range already: nothing happens.
    values.clear();
    data.clear();
    QVERIFY(fov->normalizeValue());
    QCOMPARE(fov->value(), QVariant(100));
    QCOMPARE(values.count(), 0);
    QCOMPARE(data.count(), 0);

    // A read-only property is normalised too: this is application code.
    fov->setReadOnly(true);
    fov->setAttribute(Attr::Maximum, 80);
    values.clear();
    QVERIFY(fov->normalizeValue());
    QCOMPARE(fov->value(), QVariant(80));
    QCOMPARE(values.count(), 1);
    QCOMPARE(user.count(), 0);
}

// Rounding, 64-bit ranges and custom types go through the same pipeline; a
// value the validator refuses is kept and reported.
void tst_PropertyModel::normalizeValueRoundsAndReportsRejections()
{
    const TypeId capped = QStringLiteral("test.capped");
    TypeHandler handler;
    handler.normalize = [](const QVariant& value, const Property& property) {
        return QVariant(
            qMin(value.toInt(), property.attribute(QStringLiteral("test.cap"), 1000).toInt()));
    };
    QVERIFY(TypeRegistry::global().registerType<int>(capped, handler));

    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& d = root->addDouble(QStringLiteral("d"), 1.234).decimals(3);
    Property& big = root->addInt64(QStringLiteral("big"), 900).range(0, 1000);
    Property& c = root->add(capped, QStringLiteral("c"), 80);
    Property& checked = root->addInt(QStringLiteral("checked"), 5);
    PropertyModel model(std::move(root));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    QSignalSpy failures(&model, &PropertyModel::validationFailed);

    // Double: rounded to the decimals that apply now.
    d.setAttribute(Attr::Decimals, 1);
    QCOMPARE(d.value().toDouble(), 1.234);
    QVERIFY(d.normalizeValue());
    QCOMPARE(d.value().toDouble(), 1.2);
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(0).toString(), QStringLiteral("d"));
    QCOMPARE(values.at(0).at(1).toDouble(), 1.2);
    QCOMPARE(values.at(0).at(2).toDouble(), 1.234);

    // Int64.
    big.setAttribute(Attr::Maximum, qint64(500));
    QCOMPARE(big.value(), QVariant(qint64(900)));
    QVERIFY(big.normalizeValue());
    QCOMPARE(big.value(), QVariant(qint64(500)));
    QCOMPARE(values.count(), 2);
    QCOMPARE(values.at(1).at(2), QVariant(qint64(900)));

    // A custom type with its own normalize handler.
    c.setAttribute(QStringLiteral("test.cap"), 50);
    QCOMPARE(c.value(), QVariant(80));
    QVERIFY(c.normalizeValue());
    QCOMPARE(c.value(), QVariant(50));
    QCOMPARE(values.count(), 3);
    QCOMPARE(values.at(2).at(1), QVariant(50));
    QCOMPARE(values.at(2).at(2), QVariant(80));

    // Each of them is stable once normalised.
    QVERIFY(d.normalizeValue());
    QVERIFY(big.normalizeValue());
    QVERIFY(c.normalizeValue());
    QCOMPARE(values.count(), 3);

    // The validator now refuses the stored value: it stays, and is reported.
    QCOMPARE(failures.count(), 0);
    checked.setValidator([](const QVariant& value, const Property&) {
        return value.toInt() > 3 ? ValidationResult::error(QStringLiteral("too big"))
                                 : ValidationResult::valid();
    });
    QVERIFY(!checked.normalizeValue());
    QCOMPARE(checked.value(), QVariant(5));
    QCOMPARE(failures.count(), 1);
    QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("checked"));
    QCOMPARE(failures.at(0).at(1), QVariant(5));
    QCOMPARE(failures.at(0).at(2).toString(), QStringLiteral("too big"));
    QCOMPARE(values.count(), 3);
    QCOMPARE(user.count(), 0);
}

// On a group the descendants are normalised in one batch; live ones are left
// alone, read-only and disabled ones are not.
void tst_PropertyModel::normalizeValueOnAGroupRunsInOneBatch()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& g = root->addGroup(QStringLiteral("g"));
    Property& a = g.addInt(QStringLiteral("a"), 50).range(0, 100);
    Property& b = g.addGroup(QStringLiteral("h")).addInt(QStringLiteral("b"), 60).range(0, 100);
    Property& r = g.addInt(QStringLiteral("r"), 70).range(0, 100).readOnly();
    Property& d = g.addInt(QStringLiteral("d"), 80).range(0, 100);
    d.setEnabled(false);
    Property& live = g.addInt(QStringLiteral("live"), 90).range(0, 100).live();
    Property& other = root->addInt(QStringLiteral("other"), 95).range(0, 100);
    PropertyModel model(std::move(root));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    for (Property* property : {&a, &b, &r, &d, &live, &other})
        property->setAttribute(Attr::Maximum, 40);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    QSignalSpy batch(&model, &PropertyModel::batchValueChanged);

    QVERIFY(g.normalizeValue());

    QCOMPARE(a.value(), QVariant(40));
    QCOMPARE(b.value(), QVariant(40));
    QCOMPARE(r.value(), QVariant(40));
    QCOMPARE(d.value(), QVariant(40));
    QCOMPARE(live.value(), QVariant(90)); // live: left alone
    QCOMPARE(other.value(), QVariant(95)); // outside the group
    QCOMPARE(values.count(), 4);
    QCOMPARE(user.count(), 0);
    QCOMPARE(batch.count(), 1);
    QCOMPARE(batch.at(0).at(0).toStringList(),
        (QStringList {QStringLiteral("g/a"), QStringLiteral("g/h/b"), QStringLiteral("g/r"),
            QStringLiteral("g/d")}));

    // Nothing left to do: no signal at all.
    QVERIFY(g.normalizeValue());
    QCOMPARE(values.count(), 4);
    QCOMPARE(batch.count(), 1);

    // A rejected descendant makes the call false; the others are still done.
    QVERIFY(a.setValue(30));
    a.setAttribute(Attr::Maximum, 20);
    Property& e = g.addInt(QStringLiteral("e"), 5);
    QVERIFY(e.setValue(7));
    e.setValidator([](const QVariant&, const Property&) {
        return ValidationResult::error(QStringLiteral("refused"));
    });
    QSignalSpy failures(&model, &PropertyModel::validationFailed);
    values.clear();
    QVERIFY(!g.normalizeValue());
    QCOMPARE(a.value(), QVariant(20));
    QCOMPARE(e.value(), QVariant(7));
    QCOMPARE(failures.count(), 1);
    QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("g/e"));
    QCOMPARE(values.count(), 1);
    QCOMPARE(batch.count(), 2);
    QCOMPARE(batch.at(1).at(0).toStringList(), QStringList {QStringLiteral("g/a")});
}

void tst_PropertyModel::normalizeAllValuesCoversTheWholeTree()
{
    PropertyModel empty;
    QVERIFY(empty.normalizeAllValues());

    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    Property* x = model.find(QStringLiteral("Transform/x"));
    Property* fov = model.find(QStringLiteral("fov"));
    Property& used = model.root()->addInt(QStringLiteral("used"), 50).range(0, 100);
    used.setLive(true);
    QVERIFY(x->setValue(8.0));
    QVERIFY(fov->setValue(150));
    x->setReadOnly(true);
    x->setAttribute(Attr::Maximum, 5.0);
    fov->setAttribute(Attr::Maximum, 100);
    used.setAttribute(Attr::Maximum, 10);

    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    QSignalSpy batch(&model, &PropertyModel::batchValueChanged);
    QVERIFY(model.normalizeAllValues());
    QCOMPARE(x->value(), QVariant(5.0));
    QCOMPARE(fov->value(), QVariant(100));
    QCOMPARE(used.value(), QVariant(50)); // live
    QCOMPARE(values.count(), 2);
    QCOMPARE(user.count(), 0);
    QCOMPARE(batch.count(), 1);
    QCOMPARE(batch.at(0).at(0).toStringList(),
        (QStringList {QStringLiteral("Transform/x"), QStringLiteral("fov")}));

    QVERIFY(model.normalizeAllValues());
    QCOMPARE(values.count(), 2);
    QCOMPARE(batch.count(), 1);

    // One refusal: false, and the rest of the tree is still normalised.
    fov->setAttribute(Attr::Maximum, 150);
    QVERIFY(fov->setValue(120));
    fov->setAttribute(Attr::Maximum, 100);
    x->setAttribute(Attr::Maximum, 3.0);
    model.find(QStringLiteral("name"))->setValidator([](const QVariant&, const Property&) {
        return ValidationResult::error(QStringLiteral("refused"));
    });
    QSignalSpy failures(&model, &PropertyModel::validationFailed);
    QVERIFY(!model.normalizeAllValues());
    QCOMPARE(x->value(), QVariant(3.0));
    QCOMPARE(fov->value(), QVariant(100));
    QCOMPARE(failures.count(), 1);
}

// The value becomes the default: the row refreshes, no value signal follows.
void tst_PropertyModel::makeValueDefaultNotifiesTheRow()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    Property* fov = model.find(QStringLiteral("fov"));
    QVERIFY(fov->setValue(90));
    QVERIFY(fov->isModified());
    QCOMPARE(model.indexOf(fov).data(PropertyModel::IsModifiedRole).toBool(), true);

    QSignalSpy data(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    QSignalSpy batch(&model, &PropertyModel::batchValueChanged);

    fov->makeValueDefault();

    QCOMPARE(fov->defaultValue(), QVariant(90));
    QCOMPARE(fov->value(), QVariant(90));
    QVERIFY(!fov->isModified());
    QCOMPARE(model.indexOf(fov).data(PropertyModel::IsModifiedRole).toBool(), false);
    QCOMPARE(data.count(), 1);
    QCOMPARE(data.at(0).at(0).value<QModelIndex>(), model.indexOf(fov));
    QCOMPARE(data.at(0).at(1).value<QModelIndex>().row(), model.indexOf(fov).row());
    QCOMPARE(values.count(), 0);
    QCOMPARE(user.count(), 0);
    QCOMPARE(batch.count(), 0);

    // The default already equals the value: no notification.
    fov->makeValueDefault();
    QCOMPARE(data.count(), 1);

    // A reset now returns to the new default.
    QVERIFY(fov->setValue(100));
    QVERIFY(fov->resetToDefault());
    QCOMPARE(fov->value(), QVariant(90));

    // An unmodified property is left as it is.
    Property* name = model.find(QStringLiteral("name"));
    data.clear();
    name->makeValueDefault();
    QCOMPARE(name->defaultValue(), QVariant(QStringLiteral("Main")));
    QCOMPARE(data.count(), 0);
}

// On a group and on the model: every descendant except live ones, and a
// notification only for the rows whose default changed.
void tst_PropertyModel::makeValueDefaultOnGroupsAndTheModel()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& g = root->addGroup(QStringLiteral("g"));
    Property& p = g.addInt(QStringLiteral("p"), 1);
    Property& q = g.addInt(QStringLiteral("q"), 2);
    Property& r = g.addGroup(QStringLiteral("h")).addInt(QStringLiteral("r"), 3);
    Property& ro = g.addInt(QStringLiteral("ro"), 4).readOnly();
    Property& lv = g.addInt(QStringLiteral("lv"), 5).live();
    PropertyGroup& status = g.addGroup(QStringLiteral("status"));
    status.setLive(true);
    Property& counter = status.addInt(QStringLiteral("counter"), 6);
    Property& other = root->addInt(QStringLiteral("other"), 7);
    PropertyModel model(std::move(root));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QVERIFY(p.setValue(10));
    QVERIFY(r.setValue(30));
    QVERIFY(ro.setValue(40));
    QVERIFY(lv.setValue(50));
    QVERIFY(counter.setValue(60));
    QVERIFY(other.setValue(70));

    QSignalSpy data(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy batch(&model, &PropertyModel::batchValueChanged);

    g.makeValueDefault();

    QCOMPARE(p.defaultValue(), QVariant(10));
    QCOMPARE(q.defaultValue(), QVariant(2));
    QCOMPARE(r.defaultValue(), QVariant(30));
    QCOMPARE(ro.defaultValue(), QVariant(40)); // application code: read-only does not matter
    QCOMPARE(lv.defaultValue(), QVariant(5)); // live: left alone
    QCOMPARE(counter.defaultValue(), QVariant(6)); // below a live group
    QCOMPARE(other.defaultValue(), QVariant(7)); // outside the group
    QVERIFY(other.isModified());
    QStringList notified;
    for (const QList<QVariant>& arguments : data)
        notified << model.data(arguments.at(0).value<QModelIndex>(), PropertyModel::PathRole)
                        .toString();
    notified.sort();
    QCOMPARE(notified,
        (QStringList {QStringLiteral("g/h/r"), QStringLiteral("g/p"), QStringLiteral("g/ro")}));
    QCOMPARE(values.count(), 0);
    QCOMPARE(batch.count(), 0);

    // Called on the live property itself, it applies, as a reset does.
    lv.makeValueDefault();
    QCOMPARE(lv.defaultValue(), QVariant(50));

    // The whole tree.
    data.clear();
    model.makeAllValuesDefault();
    QCOMPARE(other.defaultValue(), QVariant(70));
    QCOMPARE(counter.defaultValue(), QVariant(6));
    QVERIFY(!other.isModified());
    QCOMPARE(data.count(), 1);
    QCOMPARE(data.at(0).at(0).value<QModelIndex>(), model.indexOf(&other));
    QCOMPARE(values.count(), 0);

    data.clear();
    model.makeAllValuesDefault();
    QCOMPARE(data.count(), 0);

    PropertyModel empty;
    empty.makeAllValuesDefault(); // nothing to do, and no crash
    QVERIFY(!empty.root());
}

// Guard: changing an attribute never changes a stored value; only the explicit
// calls do.
void tst_PropertyModel::attributeChangesNeverMoveValues()
{
    PropertyModel model(createTree());
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    Property* fov = model.find(QStringLiteral("fov"));
    Property* x = model.find(QStringLiteral("Transform/x"));
    QVERIFY(fov->setValue(150));
    QVERIFY(x->setValue(1.25));
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    QSignalSpy batch(&model, &PropertyModel::batchValueChanged);

    fov->setAttribute(Attr::Maximum, 100);
    x->setAttribute(Attr::Decimals, 0);
    x->setAttribute(Attr::Maximum, 0.5);

    QCOMPARE(fov->value(), QVariant(150));
    QCOMPARE(x->value(), QVariant(1.25));
    QCOMPARE(fov->defaultValue(), QVariant(60));
    QCOMPARE(values.count(), 0);
    QCOMPARE(user.count(), 0);
    QCOMPARE(batch.count(), 0);
}

// A double that keeps its digits: application writes, user writes and the
// cell's roles. The cell shows the decimals; editing and signals carry the value.
void tst_PropertyModel::unroundedDoubleKeepsEveryDigitThroughTheModel()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->addDouble(QStringLiteral("scale"), 0.0).attribute(Attr::RoundToDecimals, false);
    root->addDouble(QStringLiteral("plain"), 0.0);
    PropertyModel model(std::move(root));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);
    const QModelIndex cell = valueIndex(model, QStringLiteral("scale"));

    QVERIFY(model.setValue(QStringLiteral("scale"), 0.0125));
    QCOMPARE(model.find(QStringLiteral("scale"))->value(), QVariant(0.0125));
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(0).toString(), QStringLiteral("scale"));
    QCOMPARE(values.at(0).at(1), QVariant(0.0125));
    QCOMPARE(values.at(0).at(2), QVariant(0.0));
    QCOMPARE(user.count(), 0);
    QCOMPARE(cell.data(Qt::EditRole), QVariant(0.0125));
    QCOMPARE(cell.data(Qt::DisplayRole).toString(), QStringLiteral("0.01"));
    QVERIFY(model.find(QStringLiteral("scale"))->isModified());

    // Writing the same value again, as a user too, is silent.
    QVERIFY(model.setData(cell, 0.0125, Qt::EditRole));
    QCOMPARE(values.count(), 1);
    QCOMPARE(user.count(), 0);

    // A user write keeps the digits as well.
    QVERIFY(model.setData(cell, 0.0375, Qt::EditRole));
    QCOMPARE(values.count(), 2);
    QCOMPARE(values.at(1).at(1), QVariant(0.0375));
    QCOMPARE(values.at(1).at(2), QVariant(0.0125));
    QCOMPARE(user.count(), 1);
    QCOMPARE(user.at(0).at(1), QVariant(0.0375));
    QCOMPARE(cell.data(Qt::EditRole), QVariant(0.0375));
    QCOMPARE(cell.data(Qt::DisplayRole).toString(), QStringLiteral("0.04"));

    // What a rounded editor would send is a different value and is stored as sent.
    QVERIFY(model.setData(cell, 0.04, Qt::EditRole));
    QCOMPARE(model.find(QStringLiteral("scale"))->value(), QVariant(0.04));
    QCOMPARE(values.count(), 3);

    // Guard: a property without the attribute still rounds (user and application).
    QVERIFY(model.setValue(QStringLiteral("plain"), 0.0375));
    QCOMPARE(model.find(QStringLiteral("plain"))->value(), QVariant(0.04));
    QVERIFY(model.setData(valueIndex(model, QStringLiteral("plain")), 1234.5678, Qt::EditRole));
    QCOMPARE(model.find(QStringLiteral("plain"))->value(), QVariant(1234.57));
}

void tst_PropertyModel::unroundedDoubleStillClampsThroughTheModel()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->addDouble(QStringLiteral("level"), 0.5)
        .range(0.0, 10.0)
        .attribute(Attr::RoundToDecimals, false);
    PropertyModel model(std::move(root));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    const QModelIndex cell = valueIndex(model, QStringLiteral("level"));

    QVERIFY(model.setData(cell, 12.3456, Qt::EditRole));
    QCOMPARE(model.find(QStringLiteral("level"))->value(), QVariant(10.0));
    QVERIFY(model.setData(cell, -0.5, Qt::EditRole));
    QCOMPARE(model.find(QStringLiteral("level"))->value(), QVariant(0.0));
    QVERIFY(model.setData(cell, 9.87654321, Qt::EditRole));
    QCOMPARE(model.find(QStringLiteral("level"))->value(), QVariant(9.87654321));
}

// Reset restores the default as it was given, also in a group reset and in
// resetAllToDefault(); the reset is an application write.
void tst_PropertyModel::unroundedDoubleResetsToItsFullDefault()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->addDouble(QStringLiteral("price"), 1234.5678).attribute(Attr::RoundToDecimals, false);
    PropertyGroup& optics = root->addGroup(QStringLiteral("Optics"));
    optics.addDouble(QStringLiteral("scale"), 0.0125).attribute(Attr::RoundToDecimals, false);
    PropertyModel model(std::move(root));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    Property* price = model.find(QStringLiteral("price"));
    Property* scale = model.find(QStringLiteral("Optics/scale"));
    QCOMPARE(price->value(), QVariant(1234.5678));
    QVERIFY(!price->isModified());
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);

    // One property.
    QVERIFY(model.setValue(QStringLiteral("price"), 5.0));
    QVERIFY(price->isModified());
    QVERIFY(model.resetToDefault(valueIndex(model, QStringLiteral("price"))));
    QCOMPARE(price->value(), QVariant(1234.5678));
    QVERIFY(!price->isModified());
    QCOMPARE(values.count(), 2);
    QCOMPARE(values.at(1).at(1), QVariant(1234.5678));
    QCOMPARE(values.at(1).at(2), QVariant(5.0));

    // Property::resetToDefault() directly.
    QVERIFY(price->setValue(6.0));
    QVERIFY(price->resetToDefault());
    QCOMPARE(price->value(), QVariant(1234.5678));

    // A group.
    QVERIFY(scale->setValue(0.5));
    QVERIFY(model.resetToDefault(model.indexOf(model.find(QStringLiteral("Optics")))));
    QCOMPARE(scale->value(), QVariant(0.0125));
    QVERIFY(!scale->isModified());

    // The whole tree.
    QVERIFY(price->setValue(7.0));
    QVERIFY(scale->setValue(0.75));
    QVERIFY(model.resetAllToDefault());
    QCOMPARE(price->value(), QVariant(1234.5678));
    QCOMPARE(scale->value(), QVariant(0.0125));
    QVERIFY(!price->isModified());
    QVERIFY(!scale->isModified());
    QCOMPARE(user.count(), 0);
}

// Guard (passes without the new attribute): a rounding double resets through the
// rounding pipeline, so a default with more digits comes back rounded.
void tst_PropertyModel::roundingDoubleResetsToItsRoundedDefault()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->addDouble(QStringLiteral("price"), 1234.5678);
    PropertyModel model(std::move(root));
    Property* price = model.find(QStringLiteral("price"));
    QVERIFY(price->setValue(5.0));
    QVERIFY(model.resetToDefault(valueIndex(model, QStringLiteral("price"))));
    QCOMPARE(price->value(), QVariant(1234.57));
}

// Normalising, making defaults and changing "decimals" do not take digits away.
void tst_PropertyModel::unroundedDoubleSurvivesNormalizingAndDefaults()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->addDouble(QStringLiteral("scale"), 0.0125).attribute(Attr::RoundToDecimals, false);
    root->addDouble(QStringLiteral("gain"), 0.0375).attribute(Attr::RoundToDecimals, false);
    PropertyModel model(std::move(root));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    Property* scale = model.find(QStringLiteral("scale"));
    Property* gain = model.find(QStringLiteral("gain"));
    QSignalSpy values(&model, &PropertyModel::valueChanged);

    QVERIFY(scale->normalizeValue());
    QVERIFY(model.normalizeAllValues());
    QCOMPARE(scale->value(), QVariant(0.0125));
    QCOMPARE(gain->value(), QVariant(0.0375));
    QCOMPARE(values.count(), 0);

    scale->setAttribute(Attr::Decimals, 1);
    QVERIFY(model.normalizeAllValues());
    QCOMPARE(scale->value(), QVariant(0.0125));
    QCOMPARE(values.count(), 0);
    QCOMPARE(valueIndex(model, QStringLiteral("scale")).data(Qt::DisplayRole).toString(),
        QStringLiteral("0"));

    // The current values become the defaults as they are.
    QVERIFY(scale->setValue(0.0625));
    QVERIFY(gain->setValue(0.0625));
    model.makeAllValuesDefault();
    QCOMPARE(scale->defaultValue(), QVariant(0.0625));
    QVERIFY(!scale->isModified());
    QVERIFY(scale->setValue(9.0));
    QVERIFY(scale->resetToDefault());
    QCOMPARE(scale->value(), QVariant(0.0625));
}

// --- A handler deletes the model during a group walk ---------------------------------------

namespace {

enum class Walk {
    ResetGroup, // PropertyModel::resetToDefault() of the "Transform" group
    ResetLeaf, // PropertyModel::resetToDefault() of "Transform/x"
    ResetAll,
    NormalizeAll,
    MakeAllDefault,
};

enum class Trigger {
    Value, // valueChanged()
    Batch, // batchValueChanged()
    Failure, // validationFailed()
    Data, // dataChanged()
};

// What happened, in order: "visit:<path>" when a validator is asked about a
// value, "deleted" when a handler deletes the model, "destroyed" when the
// property that owns the marker is destroyed (the tree dies with the model's
// tree).
struct WalkLog
{
    QStringList events;
    int deletions = 0;
};

struct DestroyedMarker
{
    explicit DestroyedMarker(WalkLog* walkLog)
        : log(walkLog)
    { }
    ~DestroyedMarker()
    {
        log->events << QStringLiteral("destroyed");
    }
    WalkLog* log;
};

// Logs every value the property's validator is asked about, refuses them all
// if requested, and keeps marker alive for as long as the property lives.
void watch(Property& property, WalkLog& log, bool refuse, std::shared_ptr<DestroyedMarker> marker)
{
    const QString path = property.path();
    property.setValidator([&log, path, refuse, marker](const QVariant&, const Property&) {
        log.events << QStringLiteral("visit:") + path;
        return refuse ? ValidationResult::error(QStringLiteral("refused"))
                      : ValidationResult::valid();
    });
}

// Deletes the model in the first notification of the given kind.
void deleteModelAt(PropertyModel* model, Trigger trigger, WalkLog& log)
{
    const auto remove = [model, &log] {
        if (log.deletions++ > 0)
            return;
        log.events << QStringLiteral("deleted");
        delete model;
    };
    switch (trigger) {
    case Trigger::Value:
        QObject::connect(model, &PropertyModel::valueChanged, remove);
        break;
    case Trigger::Batch:
        QObject::connect(model, &PropertyModel::batchValueChanged, remove);
        break;
    case Trigger::Failure:
        QObject::connect(model, &PropertyModel::validationFailed, remove);
        break;
    case Trigger::Data:
        QObject::connect(model, &QAbstractItemModel::dataChanged, remove);
        break;
    }
}

// Every leaf of createTree() in tree order.
const QStringList AllLeaves {QStringLiteral("name"), QStringLiteral("visible"),
    QStringLiteral("Transform/x"), QStringLiteral("Transform/y"),
    QStringLiteral("Transform/Pivot/z"), QStringLiteral("fov")};

// Values that every walk below has something to do with: not the defaults, and
// outside the ranges that apply now.
void prepareValues(PropertyModel& model)
{
    QVERIFY(model.setValue(QStringLiteral("name"), QStringLiteral("N")));
    QVERIFY(model.setValue(QStringLiteral("visible"), false));
    QVERIFY(model.setValue(XPath, 8.0));
    QVERIFY(model.setValue(QStringLiteral("Transform/y"), 2.0));
    QVERIFY(model.setValue(QStringLiteral("Transform/Pivot/z"), 3.0));
    QVERIFY(model.setValue(QStringLiteral("fov"), 150));
    model.find(XPath)->setAttribute(Attr::Maximum, 5.0);
    model.find(QStringLiteral("fov"))->setAttribute(Attr::Maximum, 100);
}

QStringList visitsIn(const QStringList& events)
{
    QStringList visits;
    for (const QString& event : events) {
        if (event.startsWith(QLatin1String("visit:")))
            visits << event;
    }
    return visits;
}

QStringList visitsOf(const QStringList& paths)
{
    QStringList visits;
    for (const QString& path : paths)
        visits << QStringLiteral("visit:") + path;
    return visits;
}

} // namespace

void tst_PropertyModel::modelDeletedDuringAGroupWalk_data()
{
    QTest::addColumn<int>("walk");
    QTest::addColumn<int>("trigger");
    QTest::newRow("resetGroup/value") << int(Walk::ResetGroup) << int(Trigger::Value);
    QTest::newRow("resetGroup/batch") << int(Walk::ResetGroup) << int(Trigger::Batch);
    QTest::newRow("resetGroup/failure") << int(Walk::ResetGroup) << int(Trigger::Failure);
    QTest::newRow("resetLeaf/value") << int(Walk::ResetLeaf) << int(Trigger::Value);
    QTest::newRow("resetAll/value") << int(Walk::ResetAll) << int(Trigger::Value);
    QTest::newRow("resetAll/batch") << int(Walk::ResetAll) << int(Trigger::Batch);
    QTest::newRow("resetAll/failure") << int(Walk::ResetAll) << int(Trigger::Failure);
    QTest::newRow("normalizeAll/value") << int(Walk::NormalizeAll) << int(Trigger::Value);
    QTest::newRow("normalizeAll/batch") << int(Walk::NormalizeAll) << int(Trigger::Batch);
    QTest::newRow("normalizeAll/failure") << int(Walk::NormalizeAll) << int(Trigger::Failure);
    QTest::newRow("makeAllDefault/data") << int(Walk::MakeAllDefault) << int(Trigger::Data);
}

// A handler deletes the model at the first notification of a walk. The walk
// goes on over the properties it started with, nothing is announced any more,
// and the tree dies once, when the walk has returned.
void tst_PropertyModel::modelDeletedDuringAGroupWalk()
{
    QFETCH(int, walk);
    QFETCH(int, trigger);
    const bool refuses = Trigger(trigger) == Trigger::Failure;

    WalkLog log;
    auto* model = new PropertyModel(createTree());
    const QPointer<PropertyModel> guard(model);
    const auto cleanup = qScopeGuard([&guard] { delete guard.data(); });
    prepareValues(*model);
    // The first leaf a walk reaches refuses every value in the failure rows.
    const QString refusing
        = Walk(walk) == Walk::ResetGroup ? XPath : QStringLiteral("name");
    auto marker = std::make_shared<DestroyedMarker>(&log);
    const std::weak_ptr<DestroyedMarker> alive = marker;
    for (const QString& path : AllLeaves) {
        watch(*model->find(path), log, refuses && path == refusing,
            path == QStringLiteral("fov") ? marker : nullptr);
    }
    marker.reset();
    deleteModelAt(model, Trigger(trigger), log);
    const QModelIndex transform = model->indexOf(model->find(QStringLiteral("Transform")));
    const QModelIndex leaf = model->indexOf(model->find(XPath));

    bool result = true;
    QStringList expectedVisits;
    switch (Walk(walk)) {
    case Walk::ResetGroup:
        result = model->resetToDefault(transform);
        expectedVisits = visitsOf({XPath, QStringLiteral("Transform/y"),
            QStringLiteral("Transform/Pivot/z")});
        break;
    case Walk::ResetLeaf:
        result = model->resetToDefault(leaf);
        expectedVisits = visitsOf({XPath});
        break;
    case Walk::ResetAll:
        result = model->resetAllToDefault();
        expectedVisits = visitsOf(AllLeaves);
        break;
    case Walk::NormalizeAll:
        result = model->normalizeAllValues();
        expectedVisits = visitsOf(AllLeaves);
        break;
    case Walk::MakeAllDefault:
        model->makeAllValuesDefault();
        break;
    }

    // false only when a value was rejected
    QCOMPARE(result, !refuses);
    QCOMPARE(log.deletions, 1);
    QVERIFY(guard.isNull());
    QVERIFY(alive.expired()); // nothing leaks
    QCOMPARE(visitsIn(log.events), expectedVisits);
    QCOMPARE(log.events.count(QStringLiteral("deleted")), 1);
    QCOMPARE(log.events.count(QStringLiteral("destroyed")), 1);
    // The tree is destroyed after everything the walk did.
    QCOMPARE(log.events.last(), QStringLiteral("destroyed"));
    const int deleted = log.events.indexOf(QStringLiteral("deleted"));
    if (Trigger(trigger) == Trigger::Batch) {
        QCOMPARE(deleted, log.events.size() - 2); // announced after the last write
    } else if (expectedVisits.size() > 1) {
        // The first notification came before the last property was reached.
        QVERIFY2(deleted < log.events.size() - 2,
            qPrintable(QStringLiteral("events: ") + log.events.join(QLatin1Char(','))));
        QVERIFY2(deleted < log.events.lastIndexOf(expectedVisits.last()),
            "the walk must go on over the remaining properties after the deletion");
    }
}

// A handler of the first change of a walk starts another walk over a group, and
// a handler of that walk's first change deletes the model: both walks finish,
// and the tree dies once, after the outer one has returned.
void tst_PropertyModel::modelDeletedInANestedWalk()
{
    WalkLog log;
    auto* model = new PropertyModel(createTree());
    const QPointer<PropertyModel> guard(model);
    const auto cleanup = qScopeGuard([&guard] { delete guard.data(); });
    QVERIFY(model->setValue(QStringLiteral("name"), QStringLiteral("N")));
    QVERIFY(model->setValue(XPath, 1.0));
    QVERIFY(model->setValue(QStringLiteral("Transform/y"), 2.0));
    QVERIFY(model->setValue(QStringLiteral("Transform/Pivot/z"), 3.0));
    auto marker = std::make_shared<DestroyedMarker>(&log);
    const std::weak_ptr<DestroyedMarker> alive = marker;
    for (const QString& path : AllLeaves)
        watch(*model->find(path), log, false, path == QStringLiteral("fov") ? marker : nullptr);
    marker.reset();

    PropertyGroup* transform = model->find(QStringLiteral("Transform"))->toGroup();
    int calls = 0;
    bool innerResult = false;
    QObject::connect(model, &PropertyModel::valueChanged, [&] {
        if (++calls == 1) { // "name", written by the outer walk
            log.events << QStringLiteral("inner:begin");
            innerResult = transform->resetToDefault();
            log.events << QStringLiteral("inner:end");
        } else if (calls == 2) { // "Transform/x", written by the inner walk
            log.events << QStringLiteral("deleted");
            delete model;
        }
    });

    const bool outerResult = model->resetAllToDefault();

    QVERIFY(outerResult);
    QVERIFY(innerResult);
    QCOMPARE(calls, 2);
    QVERIFY(guard.isNull());
    QVERIFY(alive.expired());
    const QStringList expected {QStringLiteral("visit:name"), QStringLiteral("inner:begin"),
        QStringLiteral("visit:Transform/x"), QStringLiteral("deleted"),
        QStringLiteral("visit:Transform/y"), QStringLiteral("visit:Transform/Pivot/z"),
        QStringLiteral("inner:end"), QStringLiteral("visit:visible"),
        QStringLiteral("visit:Transform/x"), QStringLiteral("visit:Transform/y"),
        QStringLiteral("visit:Transform/Pivot/z"), QStringLiteral("visit:fov"),
        QStringLiteral("destroyed")};
    QCOMPARE(log.events, expected);
}

namespace {

enum class DataAction {
    SetValue, // PropertyModel::setValue()
    ResetAll,
    ResetGroup,
    DependentLeaf, // a write that changes the enabled state of a leaf
    DependentGroup, // a write that changes the enabled state of a group
};

} // namespace

void tst_PropertyModel::modelDeletedByADataChangedHandler_data()
{
    QTest::addColumn<int>("action");
    QTest::addColumn<int>("deleteAt"); // number of the dataChanged() that deletes the model
    QTest::newRow("setValue") << int(DataAction::SetValue) << 1;
    QTest::newRow("resetAll") << int(DataAction::ResetAll) << 1;
    QTest::newRow("resetAll/second") << int(DataAction::ResetAll) << 2;
    QTest::newRow("resetGroup") << int(DataAction::ResetGroup) << 1;
    QTest::newRow("dependentLeaf") << int(DataAction::DependentLeaf) << 2;
    QTest::newRow("dependentGroup") << int(DataAction::DependentGroup) << 2;
    QTest::newRow("dependentGroup/subtree") << int(DataAction::DependentGroup) << 3;
}

// A handler of dataChanged() deletes the model, also the one announced for a
// dependent property or for the rows below a group: the write or the walk goes
// on without announcements, and the tree dies once, after the outermost write.
void tst_PropertyModel::modelDeletedByADataChangedHandler()
{
    QFETCH(int, action);
    QFETCH(int, deleteAt);

    WalkLog log;
    auto* model = new PropertyModel(createTree());
    const QPointer<PropertyModel> guard(model);
    const auto cleanup = qScopeGuard([&guard] { delete guard.data(); });
    prepareValues(*model);
    auto marker = std::make_shared<DestroyedMarker>(&log);
    const std::weak_ptr<DestroyedMarker> alive = marker;
    for (const QString& path : AllLeaves)
        watch(*model->find(path), log, false, path == QStringLiteral("fov") ? marker : nullptr);
    marker.reset();
    if (DataAction(action) == DataAction::DependentLeaf)
        model->find(QStringLiteral("Transform/y"))->setEnabledWhen(QStringLiteral("visible"));
    else if (DataAction(action) == DataAction::DependentGroup)
        model->find(QStringLiteral("Transform"))->setEnabledWhen(QStringLiteral("visible"));

    int dataCalls = 0;
    QObject::connect(model, &QAbstractItemModel::dataChanged, [&] {
        if (++dataCalls != deleteAt)
            return;
        log.events << QStringLiteral("deleted");
        delete model;
    });
    int valueCalls = 0;
    QObject::connect(model, &PropertyModel::valueChanged, [&valueCalls] { ++valueCalls; });

    QStringList expectedVisits;
    bool result = false;
    switch (DataAction(action)) {
    case DataAction::SetValue:
        result = model->setValue(QStringLiteral("Transform/y"), 7.0);
        expectedVisits = visitsOf({QStringLiteral("Transform/y")});
        break;
    case DataAction::ResetAll:
        result = model->resetAllToDefault();
        expectedVisits = visitsOf(AllLeaves);
        break;
    case DataAction::ResetGroup:
        result = model->resetToDefault(
            model->indexOf(model->find(QStringLiteral("Transform"))));
        expectedVisits = visitsOf({XPath, QStringLiteral("Transform/y"),
            QStringLiteral("Transform/Pivot/z")});
        break;
    case DataAction::DependentLeaf:
    case DataAction::DependentGroup:
        result = model->setValue(QStringLiteral("visible"), true);
        expectedVisits = visitsOf({QStringLiteral("visible")});
        break;
    }

    QVERIFY(result);
    QCOMPARE(dataCalls, deleteAt); // nothing is announced after the deletion
    // Only the writes that were announced before the deletion reached valueChanged().
    QCOMPARE(valueCalls, DataAction(action) == DataAction::ResetAll && deleteAt == 2 ? 1 : 0);
    QVERIFY(guard.isNull());
    QVERIFY(alive.expired());
    QCOMPARE(visitsIn(log.events), expectedVisits);
    QCOMPARE(log.events.count(QStringLiteral("deleted")), 1);
    QCOMPARE(log.events.count(QStringLiteral("destroyed")), 1);
    QCOMPARE(log.events.last(), QStringLiteral("destroyed"));
}

QTEST_APPLESS_MAIN(tst_PropertyModel)
#include "tst_propertymodel.moc"
