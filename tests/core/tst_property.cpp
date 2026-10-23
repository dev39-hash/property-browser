// Property, PropertyGroup and builders without a model.

#include <qpb/qpbcore.h>

#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTest>

#include <memory>

using namespace qpb;

namespace {

enum class GroupWalk {
    Reset,
    Normalize,
    MakeDefault,
};

enum class Notification {
    Value, // valueChanged()
    Batch, // batchValueChanged()
    Failure, // validationFailed()
    Data, // dataChanged()
};

// What happened, in order: "visit:<path>" when a validator is asked about a
// value, "deleted" when a handler deletes the model, "destroyed" when the
// property that owns the marker is destroyed.
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

// root
// |-- name
// |-- g
// |   |-- a
// |   |-- b
// |   `-- sub
// |       `-- c
// `-- tail
std::unique_ptr<PropertyGroup> createWalkTree()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->addString(QStringLiteral("name"), QStringLiteral("Main"));
    PropertyGroup& g = root->addGroup(QStringLiteral("g"));
    g.addInt(QStringLiteral("a"), 0).range(0, 100);
    g.addInt(QStringLiteral("b"), 0).range(0, 100);
    g.addGroup(QStringLiteral("sub")).addInt(QStringLiteral("c"), 0).range(0, 100);
    root->addInt(QStringLiteral("tail"), 0);
    return root;
}

} // namespace

class tst_Property : public QObject
{
    Q_OBJECT

private slots:
    void typedAddsStoreValueAndDefault();
    void groupsAndIdentity();
    void pathsAndLookup();
    void duplicateIdReturnsExistingChild();
    void addGroupOverValueUsesFreeId();
    void invalidIdsAreSanitized();
    void removeChild();
    void createAndAttach();
    void initialValueIsConverted();
    void effectiveStateIsInherited();
    void flagsAreIndependentPerProperty();
    void buildersSetAttributes();
    void enumFromLabels();
    void attributesApi();
    void setValueWithoutModel();
    void setValueAllowedWhenReadOnlyOrDisabled();
    void validatorRejectsValue();
    void modifiedAndReset();
    void groupResetResetsDescendants();
    void liveProperties();

    void normalizeValueWithoutModel();
    void normalizeValueOnGroups();
    void makeValueDefaultWithoutModel();
    void attributeChangesLeaveValuesAlone();

    // A handler deletes the model while a group is being walked.
    void modelDeletedDuringAGroupWalk_data();
    void modelDeletedDuringAGroupWalk();
};

void tst_Property::typedAddsStoreValueAndDefault()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& b = root->addBool(QStringLiteral("b"), true);
    Property& i = root->addInt(QStringLiteral("i"), 7);
    Property& d = root->addDouble(QStringLiteral("d"), 1.5);
    Property& s = root->addString(QStringLiteral("s"), QStringLiteral("text"));
    Property& f = root->addFilePath(QStringLiteral("f"), QStringLiteral("/tmp/a.txt"));
    Property& p = root->addDirPath(QStringLiteral("p"), QStringLiteral("/tmp"));

    QCOMPARE(b.typeId(), TypeId(Types::Bool));
    QCOMPARE(b.value(), QVariant(true));
    QCOMPARE(i.typeId(), TypeId(Types::Int));
    QCOMPARE(i.value(), QVariant(7));
    QCOMPARE(d.typeId(), TypeId(Types::Double));
    QCOMPARE(d.value(), QVariant(1.5));
    QCOMPARE(s.typeId(), TypeId(Types::String));
    QCOMPARE(s.value(), QVariant(QStringLiteral("text")));
    QCOMPARE(f.typeId(), TypeId(Types::FilePath));
    QCOMPARE(p.typeId(), TypeId(Types::DirPath));

    for (const Property* property : root->children()) {
        QCOMPARE(property->defaultValue(), property->value());
        QVERIFY(!property->isModified());
        QVERIFY(!property->isGroup());
        QCOMPARE(property->toGroup(), nullptr);
    }
}

void tst_Property::groupsAndIdentity()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& group = root->addGroup(QStringLiteral("g"));
    Property& leaf = group.addInt(QStringLiteral("x"), 1);

    QVERIFY(root->isGroup());
    QCOMPARE(root->typeId(), TypeId(Types::Group));
    QCOMPARE(root->toGroup(), root.get());
    QVERIFY(!root->value().isValid());
    QVERIFY(!root->isModified());
    QCOMPARE(root->parent(), nullptr);
    QCOMPARE(group.parent(), root.get());
    QCOMPARE(leaf.parent(), &group);
    QCOMPARE(leaf.id(), QStringLiteral("x"));
    QCOMPARE(leaf.displayName(), QStringLiteral("x"));
    leaf.setDisplayName(QStringLiteral("X axis"));
    QCOMPARE(leaf.displayName(), QStringLiteral("X axis"));
    leaf.setToolTip(QStringLiteral("tip"));
    QCOMPARE(leaf.toolTip(), QStringLiteral("tip"));
}

void tst_Property::pathsAndLookup()
{
    auto root = PropertyGroup::create(QStringLiteral("Camera"));
    PropertyGroup& transform = root->addGroup(QStringLiteral("Transform"));
    Property& x = transform.addDouble(QStringLiteral("x"), 0.0);
    Property& y = transform.addDouble(QStringLiteral("y"), 0.0);
    PropertyGroup& nested = transform.addGroup(QStringLiteral("Pivot"));
    Property& px = nested.addDouble(QStringLiteral("x"), 0.0);
    Property& name = root->addString(QStringLiteral("name"), QString());

    QCOMPARE(root->path(), QString());
    QCOMPARE(transform.path(), QStringLiteral("Transform"));
    QCOMPARE(x.path(), QStringLiteral("Transform/x"));
    QCOMPARE(px.path(), QStringLiteral("Transform/Pivot/x"));
    QCOMPARE(name.path(), QStringLiteral("name"));

    QCOMPARE(root->find(QStringLiteral("Transform/x")), &x);
    QCOMPARE(root->find(QStringLiteral("Transform/Pivot/x")), &px);
    QCOMPARE(transform.find(QStringLiteral("Pivot/x")), &px);
    QCOMPARE(root->find(QStringLiteral("Transform")), &transform);
    QCOMPARE(root->find(QStringLiteral("Transform/z")), nullptr);
    QCOMPARE(root->find(QStringLiteral("name/x")), nullptr);
    QCOMPARE(root->find(QString()), nullptr);

    QCOMPARE(transform.childCount(), 3);
    QCOMPARE(transform.child(0), &x);
    QCOMPARE(transform.child(1), &y);
    QCOMPARE(transform.child(3), nullptr);
    QCOMPARE(transform.child(-1), nullptr);
    QCOMPARE(transform.child(QStringLiteral("y")), &y);
    QCOMPARE(transform.child(QStringLiteral("nope")), nullptr);
    QCOMPARE(transform.indexOf(&y), 1);
    QCOMPARE(transform.indexOf(&name), -1);
    QCOMPARE(transform.indexOf(nullptr), -1);
    QCOMPARE(transform.children(), (QList<Property*> {&x, &y, &nested}));
}

void tst_Property::duplicateIdReturnsExistingChild()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& first = root->addInt(QStringLiteral("a"), 1);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("already has a child")));
    Property& second = root->addInt(QStringLiteral("a"), 2);
    QCOMPARE(&second, &first);
    QCOMPARE(root->childCount(), 1);
    QCOMPARE(first.value(), QVariant(1));
}

void tst_Property::addGroupOverValueUsesFreeId()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("a"), 1);
    root->addInt(QStringLiteral("a_2"), 2);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("is not a group")));
    PropertyGroup& group = root->addGroup(QStringLiteral("a"));
    QCOMPARE(group.id(), QStringLiteral("a_3"));
    QCOMPARE(root->childCount(), 3);

    // An existing group with the same id is returned as is.
    PropertyGroup& g = root->addGroup(QStringLiteral("g"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("already has a child")));
    QCOMPARE(&root->addGroup(QStringLiteral("g")), &g);
}

void tst_Property::invalidIdsAreSanitized()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("empty property id")));
    QCOMPARE(root->addInt(QString(), 1).property().id(), QStringLiteral("property"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("contains '/'")));
    QCOMPARE(root->addInt(QStringLiteral("a/b"), 1).property().id(), QStringLiteral("a_b"));
}

void tst_Property::removeChild()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("a"), 1);
    PropertyGroup& g = root->addGroup(QStringLiteral("g"));
    g.addInt(QStringLiteral("x"), 1);

    QVERIFY(root->remove(QStringLiteral("g")));
    QCOMPARE(root->childCount(), 1);
    QCOMPARE(root->find(QStringLiteral("g/x")), nullptr);
    QVERIFY(!root->remove(QStringLiteral("g")));
    QVERIFY(root->remove(QStringLiteral("a")));
    QCOMPARE(root->childCount(), 0);
}

void tst_Property::createAndAttach()
{
    std::unique_ptr<Property> custom
        = Property::create(QStringLiteral("test.custom"), QStringLiteral("c"), 3);
    QCOMPARE(custom->parent(), nullptr);
    QCOMPARE(custom->path(), QString());

    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property* raw = custom.get();
    QCOMPARE(&root->add(std::move(custom)), raw);
    QCOMPARE(raw->parent(), root.get());
    QCOMPARE(raw->path(), QStringLiteral("c"));
    QCOMPARE(raw->typeId(), QStringLiteral("test.custom"));

    auto sub = PropertyGroup::create(QStringLiteral("sub"));
    sub->addBool(QStringLiteral("flag"), false);
    root->add(std::move(sub));
    QVERIFY(root->find(QStringLiteral("sub/flag")));
}

void tst_Property::initialValueIsConverted()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& n = root->add(Types::Int, QStringLiteral("n"), QStringLiteral("5"));
    QCOMPARE(n.value().userType(), qMetaTypeId<int>());
    QCOMPARE(n.value(), QVariant(5));

    // Unconvertible or unregistered: stored as given.
    Property& u
        = root->add(QStringLiteral("test.unknown"), QStringLiteral("u"), QStringLiteral("v"));
    QCOMPARE(u.value(), QVariant(QStringLiteral("v")));
}

void tst_Property::effectiveStateIsInherited()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& a = root->addGroup(QStringLiteral("a"));
    PropertyGroup& b = a.addGroup(QStringLiteral("b"));
    Property& leaf = b.addInt(QStringLiteral("leaf"), 0);

    QVERIFY(!leaf.isReadOnly());
    QVERIFY(leaf.isEnabled());
    QVERIFY(leaf.isVisible());

    a.setReadOnly(true);
    QVERIFY(leaf.isReadOnly());
    QVERIFY(b.isReadOnly());
    QVERIFY(!root->isReadOnly());
    QCOMPARE(leaf.flags(), Property::Flags());

    root->setEnabled(false);
    QVERIFY(!leaf.isEnabled());
    root->setEnabled(true);
    QVERIFY(leaf.isEnabled());

    b.setVisible(false);
    QVERIFY(!leaf.isVisible());
    QVERIFY(a.isVisible());

    a.setReadOnly(false);
    QVERIFY(!leaf.isReadOnly());
}

void tst_Property::flagsAreIndependentPerProperty()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& p = root->addInt(QStringLiteral("p"), 0);
    p.setFlags(Property::Flag::ReadOnly | Property::Flag::Hidden);
    QCOMPARE(p.flags(), Property::Flag::ReadOnly | Property::Flag::Hidden);
    p.setFlag(Property::Flag::Hidden, false);
    QCOMPARE(p.flags(), Property::Flags(Property::Flag::ReadOnly));
    p.setEnabled(false);
    QVERIFY(p.flags().testFlag(Property::Flag::Disabled));
    QVERIFY(!p.isEnabled());
}

void tst_Property::buildersSetAttributes()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& i = root->addInt(QStringLiteral("i"), 5)
                      .range(0, 10)
                      .step(2)
                      .prefix(QStringLiteral("#"))
                      .suffix(QStringLiteral(" px"))
                      .displayName(QStringLiteral("Integer"))
                      .toolTip(QStringLiteral("tip"))
                      .readOnly();
    QCOMPARE(i.attribute(Attr::Minimum), QVariant(0));
    QCOMPARE(i.attribute(Attr::Maximum), QVariant(10));
    QCOMPARE(i.attribute(Attr::Step), QVariant(2));
    QCOMPARE(i.attribute(Attr::Prefix), QVariant(QStringLiteral("#")));
    QCOMPARE(i.attribute(Attr::Suffix), QVariant(QStringLiteral(" px")));
    QCOMPARE(i.displayName(), QStringLiteral("Integer"));
    QCOMPARE(i.toolTip(), QStringLiteral("tip"));
    QVERIFY(i.isReadOnly());

    Property& d = root->addDouble(QStringLiteral("d"), 0.0).minimum(-1.0).maximum(1.0).decimals(3);
    QCOMPARE(d.attribute(Attr::Minimum), QVariant(-1.0));
    QCOMPARE(d.attribute(Attr::Decimals), QVariant(3));

    Property& s = root->addString(QStringLiteral("s"), QString())
                      .maxLength(8)
                      .placeholder(QStringLiteral("name"))
                      .regularExpression(QStringLiteral("[a-z]*"));
    QCOMPARE(s.attribute(Attr::MaxLength), QVariant(8));
    QCOMPARE(s.attribute(Attr::RegularExpression), QVariant(QStringLiteral("[a-z]*")));

    Property& f = root->addFilePath(QStringLiteral("f"), QString())
                      .filter(QStringLiteral("*.txt"))
                      .dialogMode(FileMode::Save)
                      .defaultDir(QStringLiteral("/tmp"))
                      .mustExist();
    QCOMPARE(f.attribute(Attr::DialogMode), QVariant(int(FileMode::Save)));
    QCOMPARE(f.attribute(Attr::MustExist), QVariant(true));

    Property& dir = root->addDirPath(QStringLiteral("dir"), QString()).mustExist(false);
    QCOMPARE(dir.attribute(Attr::MustExist), QVariant(false));

    Property& e = root->addBool(QStringLiteral("e"), false)
                      .editor(QStringLiteral("test.switch"))
                      .attribute(QStringLiteral("test.key"), 42)
                      .visible(false)
                      .enabled(false);
    QCOMPARE(e.attribute(Attr::EditorId), QVariant(QStringLiteral("test.switch")));
    QCOMPARE(e.attribute(QStringLiteral("test.key")), QVariant(42));
    QVERIFY(!e.isVisible());
    QVERIFY(!e.isEnabled());
}

void tst_Property::enumFromLabels()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& e = root->addEnum(QStringLiteral("e"), {QStringLiteral("A"), QStringLiteral("B")}, 1);
    QCOMPARE(e.typeId(), TypeId(Types::Enum));
    QCOMPARE(e.value(), QVariant(1));
    const auto options = e.attribute(Attr::Options).value<QList<EnumOption>>();
    QCOMPARE(options.size(), 2);
    QCOMPARE(options.at(0).label, QStringLiteral("A"));
    QCOMPARE(options.at(1).value, QVariant(1));

    Property& s = root->addEnum(QStringLiteral("s"),
        {EnumOption {QStringLiteral("English"), QStringLiteral("en")},
            EnumOption {QStringLiteral("Tiếng Việt"), QStringLiteral("vi")}},
        QStringLiteral("vi"));
    QCOMPARE(s.value(), QVariant(QStringLiteral("vi")));
}

void tst_Property::attributesApi()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& p = root->addInt(QStringLiteral("p"), 0);
    QVERIFY(!p.hasAttribute(QStringLiteral("k")));
    QCOMPARE(p.attribute(QStringLiteral("k"), 5), QVariant(5));
    p.setAttribute(QStringLiteral("k"), 1);
    QVERIFY(p.hasAttribute(QStringLiteral("k")));
    QCOMPARE(p.attributes().value(QStringLiteral("k")), QVariant(1));
    p.removeAttribute(QStringLiteral("k"));
    QVERIFY(!p.hasAttribute(QStringLiteral("k")));
}

void tst_Property::setValueWithoutModel()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& p = root->addInt(QStringLiteral("p"), 1);
    QVERIFY(p.setValue(2));
    QCOMPARE(p.value(), QVariant(2));
    QVERIFY(p.setValue(QStringLiteral("3")));
    QCOMPARE(p.value(), QVariant(3));
    QVERIFY(!root->setValue(1)); // groups have no value
}

// Read-only and disabled restrict the user only; application code can still
// set values, so an application can update its own read-only status field.
// Validation still applies.
void tst_Property::setValueAllowedWhenReadOnlyOrDisabled()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& g = root->addGroup(QStringLiteral("g"));
    Property& p = g.addInt(QStringLiteral("p"), 1).range(0, 10);

    g.setReadOnly(true);
    QVERIFY(p.setValue(2));
    QCOMPARE(p.value(), QVariant(2));
    QVERIFY(!p.setValue(QStringLiteral("x"))); // still converted and validated
    QVERIFY(p.setValue(50));
    QCOMPARE(p.value(), QVariant(10)); // still normalized
    g.setReadOnly(false);

    p.setEnabled(false);
    QVERIFY(p.setValue(3));
    QCOMPARE(p.value(), QVariant(3));

    Property& unknown = root->add(QStringLiteral("test.unregistered"), QStringLiteral("u"), 1);
    QVERIFY(!unknown.setValue(2));
}

void tst_Property::validatorRejectsValue()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& p = root->addInt(QStringLiteral("p"), 2)
                      .validator([](const QVariant& value, const Property& property) {
                          if (property.id() != QStringLiteral("p"))
                              return ValidationResult::error(QStringLiteral("wrong property"));
                          return value.toInt() % 2 == 0
                              ? ValidationResult::valid()
                              : ValidationResult::error(QStringLiteral("odd"));
                      });
    QVERIFY(p.validator());
    QVERIFY(p.setValue(4));
    QVERIFY(!p.setValue(5));
    QCOMPARE(p.value(), QVariant(4));
}

void tst_Property::modifiedAndReset()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& p = root->addInt(QStringLiteral("p"), 1);
    QVERIFY(p.setValue(2));
    QVERIFY(p.isModified());
    QVERIFY(p.resetToDefault());
    QCOMPARE(p.value(), QVariant(1));
    QVERIFY(!p.isModified());

    p.setDefaultValue(QStringLiteral("7"));
    QCOMPARE(p.defaultValue(), QVariant(7));
    QVERIFY(p.isModified());
}

void tst_Property::groupResetResetsDescendants()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& g = root->addGroup(QStringLiteral("g"));
    Property& a = g.addInt(QStringLiteral("a"), 1);
    Property& b
        = g.addGroup(QStringLiteral("h")).addString(QStringLiteral("b"), QStringLiteral("x"));
    QVERIFY(a.setValue(10));
    QVERIFY(b.setValue(QStringLiteral("y")));
    QVERIFY(root->resetToDefault());
    QCOMPARE(a.value(), QVariant(1));
    QCOMPARE(b.value(), QVariant(QStringLiteral("x")));

    // Application-side resets include read-only and disabled descendants.
    QVERIFY(a.setValue(10));
    a.setReadOnly(true);
    b.setEnabled(false);
    QVERIFY(b.setValue(QStringLiteral("z")));
    QVERIFY(g.resetToDefault());
    QCOMPARE(a.value(), QVariant(1));
    QCOMPARE(b.value(), QVariant(QStringLiteral("x")));
}

// Since 1.3: values maintained by the application.
void tst_Property::liveProperties()
{
    QCOMPARE(int(Property::Flag::Live), 0x8);
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& setting = root->addInt(QStringLiteral("setting"), 1);
    Property& used = root->addInt(QStringLiteral("used"), 0).live().readOnly();
    PropertyGroup& status = root->addGroup(QStringLiteral("status"));
    Property& counter = status.addInt(QStringLiteral("counter"), 0);
    status.setLive(true);

    QVERIFY(used.isLive());
    QVERIFY(used.flags().testFlag(Property::Flag::Live));
    QVERIFY(counter.isLive()); // inherited from the group
    QVERIFY(!counter.flags().testFlag(Property::Flag::Live));
    QVERIFY(!setting.isLive());

    // Never modified.
    QVERIFY(used.setValue(42));
    QVERIFY(counter.setValue(7));
    QVERIFY(setting.setValue(2));
    QVERIFY(!used.isModified());
    QVERIFY(!counter.isModified());
    QVERIFY(setting.isModified());

    // Group resets leave live values alone; resetting one directly still works.
    QVERIFY(root->resetToDefault());
    QCOMPARE(setting.value(), QVariant(1));
    QCOMPARE(used.value(), QVariant(42));
    QCOMPARE(counter.value(), QVariant(7));
    QVERIFY(used.resetToDefault());
    QCOMPARE(used.value(), QVariant(0));

    // Not live any more: modified again.
    QVERIFY(counter.setValue(9));
    status.setLive(false);
    QVERIFY(counter.isModified());
}

// A stored value is run through the write pipeline again, for example after a
// range changed.
void tst_Property::normalizeValueWithoutModel()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& n = root->addInt(QStringLiteral("n"), 5).range(0, 1000);
    Property& d = root->addDouble(QStringLiteral("d"), 1.234).decimals(3);
    Property& big = root->addInt64(QStringLiteral("big"), 900).range(0, 1000);
    Property& s = root->addString(QStringLiteral("s"), QStringLiteral("abc"));

    QVERIFY(n.setValue(500));
    n.setAttribute(Attr::Maximum, 100);
    QCOMPARE(n.value(), QVariant(500));
    QVERIFY(n.normalizeValue());
    QCOMPARE(n.value(), QVariant(100));
    QVERIFY(n.isModified());
    QVERIFY(n.normalizeValue()); // already inside the range
    QCOMPARE(n.value(), QVariant(100));

    d.setAttribute(Attr::Decimals, 1);
    QCOMPARE(d.value().toDouble(), 1.234);
    QVERIFY(d.normalizeValue());
    QCOMPARE(d.value().toDouble(), 1.2);

    big.setAttribute(Attr::Maximum, qint64(500));
    QVERIFY(big.normalizeValue());
    QCOMPARE(big.value(), QVariant(qint64(500)));

    QVERIFY(s.normalizeValue());
    QCOMPARE(s.value(), QVariant(QStringLiteral("abc")));

    // A value the validator refuses now is kept.
    s.setValidator([](const QVariant&, const Property&) {
        return ValidationResult::error(QStringLiteral("refused"));
    });
    QVERIFY(!s.normalizeValue());
    QCOMPARE(s.value(), QVariant(QStringLiteral("abc")));

    // Read-only and disabled properties are normalised: this is application code.
    Property& locked = root->addInt(QStringLiteral("locked"), 50).range(0, 100).readOnly();
    locked.setEnabled(false);
    locked.setAttribute(Attr::Maximum, 10);
    QVERIFY(locked.normalizeValue());
    QCOMPARE(locked.value(), QVariant(10));

    // A type that is not registered cannot be written, normalised included.
    Property& unknown = root->add(QStringLiteral("test.unregistered"), QStringLiteral("u"), 1);
    QVERIFY(!unknown.normalizeValue());
}

void tst_Property::normalizeValueOnGroups()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& g = root->addGroup(QStringLiteral("g"));
    Property& a = g.addInt(QStringLiteral("a"), 50).range(0, 100);
    Property& b = g.addGroup(QStringLiteral("h")).addInt(QStringLiteral("b"), 60).range(0, 100);
    Property& live = g.addInt(QStringLiteral("live"), 90).range(0, 100).live();
    PropertyGroup& status = g.addGroup(QStringLiteral("status"));
    status.setLive(true);
    Property& counter = status.addInt(QStringLiteral("counter"), 80).range(0, 100);
    Property& other = root->addInt(QStringLiteral("other"), 95).range(0, 100);
    for (Property* property : {&a, &b, &live, &counter, &other})
        property->setAttribute(Attr::Maximum, 40);

    QVERIFY(g.normalizeValue());
    QCOMPARE(a.value(), QVariant(40));
    QCOMPARE(b.value(), QVariant(40));
    QCOMPARE(live.value(), QVariant(90)); // live, and below a live group: left alone
    QCOMPARE(counter.value(), QVariant(80));
    QCOMPARE(other.value(), QVariant(95)); // outside the group

    // The whole tree through the root.
    QVERIFY(a.setValue(30));
    a.setAttribute(Attr::Maximum, 20);
    Property& refused = g.addInt(QStringLiteral("refused"), 5);
    refused.setValidator([](const QVariant& value, const Property&) {
        return value.toInt() > 5 ? ValidationResult::error(QStringLiteral("refused"))
                                 : ValidationResult::valid();
    });
    refused.setAttribute(Attr::Maximum, 5);
    QVERIFY(root->normalizeValue());
    QCOMPARE(a.value(), QVariant(20));
    QCOMPARE(other.value(), QVariant(40));

    // A refusal makes the call false but does not stop the others.
    QVERIFY(a.setValue(10));
    a.setAttribute(Attr::Maximum, 5);
    refused.setValidator([](const QVariant&, const Property&) {
        return ValidationResult::error(QStringLiteral("refused"));
    });
    QVERIFY(!root->normalizeValue());
    QCOMPARE(a.value(), QVariant(5));

    PropertyGroup& empty = root->addGroup(QStringLiteral("empty"));
    QVERIFY(empty.normalizeValue());
}

// The current value becomes the default; groups cover their descendants,
// live ones excepted.
void tst_Property::makeValueDefaultWithoutModel()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& g = root->addGroup(QStringLiteral("g"));
    Property& p = g.addInt(QStringLiteral("p"), 1);
    Property& q = g.addInt(QStringLiteral("q"), 2);
    Property& r = g.addGroup(QStringLiteral("h")).addString(QStringLiteral("r"), QStringLiteral("x"));
    Property& ro = g.addInt(QStringLiteral("ro"), 4).readOnly();
    Property& lv = g.addInt(QStringLiteral("lv"), 5).live();
    Property& other = root->addInt(QStringLiteral("other"), 7);
    QVERIFY(p.setValue(10));
    QVERIFY(r.setValue(QStringLiteral("y")));
    QVERIFY(ro.setValue(40));
    QVERIFY(lv.setValue(50));
    QVERIFY(other.setValue(70));

    p.makeValueDefault();
    QCOMPARE(p.defaultValue(), QVariant(10));
    QCOMPARE(p.value(), QVariant(10));
    QVERIFY(!p.isModified());
    QVERIFY(p.resetToDefault());
    QCOMPARE(p.value(), QVariant(10));
    QVERIFY(p.setValue(11));
    QVERIFY(p.isModified());
    QVERIFY(p.resetToDefault());
    QCOMPARE(p.value(), QVariant(10));

    g.makeValueDefault();
    QCOMPARE(q.defaultValue(), QVariant(2));
    QCOMPARE(r.defaultValue(), QVariant(QStringLiteral("y")));
    QCOMPARE(ro.defaultValue(), QVariant(40)); // read-only: application code
    QCOMPARE(lv.defaultValue(), QVariant(5)); // live: left alone
    QCOMPARE(other.defaultValue(), QVariant(7)); // outside the group
    QVERIFY(!r.isModified());
    QVERIFY(!ro.isModified());
    QVERIFY(other.isModified());

    // On the live property itself it applies, as a reset does.
    lv.makeValueDefault();
    QCOMPARE(lv.defaultValue(), QVariant(50));

    root->makeValueDefault();
    QCOMPARE(other.defaultValue(), QVariant(70));
    QVERIFY(!other.isModified());
}

// Guard: an attribute change alone never changes a value or a default.
void tst_Property::attributeChangesLeaveValuesAlone()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& n = root->addInt(QStringLiteral("n"), 5).range(0, 1000);
    Property& d = root->addDouble(QStringLiteral("d"), 1.234).decimals(3);
    QVERIFY(n.setValue(500));

    n.setAttribute(Attr::Maximum, 100);
    n.setAttribute(Attr::Minimum, 50);
    d.setAttribute(Attr::Decimals, 0);
    d.setAttribute(Attr::Maximum, 1.0);

    QCOMPARE(n.value(), QVariant(500));
    QCOMPARE(n.defaultValue(), QVariant(5));
    QCOMPARE(d.value().toDouble(), 1.234);
    QCOMPARE(d.defaultValue().toDouble(), 1.234);
}

void tst_Property::modelDeletedDuringAGroupWalk_data()
{
    QTest::addColumn<int>("walk");
    QTest::addColumn<int>("notification");
    QTest::newRow("reset/value") << int(GroupWalk::Reset) << int(Notification::Value);
    QTest::newRow("reset/batch") << int(GroupWalk::Reset) << int(Notification::Batch);
    QTest::newRow("reset/failure") << int(GroupWalk::Reset) << int(Notification::Failure);
    QTest::newRow("normalize/value") << int(GroupWalk::Normalize) << int(Notification::Value);
    QTest::newRow("normalize/batch") << int(GroupWalk::Normalize) << int(Notification::Batch);
    QTest::newRow("normalize/failure") << int(GroupWalk::Normalize) << int(Notification::Failure);
    QTest::newRow("makeDefault/data") << int(GroupWalk::MakeDefault) << int(Notification::Data);
}

// A handler deletes the model at the first notification of a walk over a
// group (called on the group itself, not through the model). The walk goes on
// over the group's descendants, nothing is announced any more, and the tree
// dies once, when the walk has returned.
void tst_Property::modelDeletedDuringAGroupWalk()
{
    QFETCH(int, walk);
    QFETCH(int, notification);
    const bool refuses = Notification(notification) == Notification::Failure;

    WalkLog log;
    auto* model = new PropertyModel(createWalkTree());
    const QPointer<PropertyModel> guard(model);
    const auto cleanup = qScopeGuard([&guard] { delete guard.data(); });
    for (const QString& path : {QStringLiteral("g/a"), QStringLiteral("g/b"),
             QStringLiteral("g/sub/c")}) {
        QVERIFY(model->setValue(path, 10));
        model->find(path)->setAttribute(Attr::Maximum, 5);
    }
    auto marker = std::make_shared<DestroyedMarker>(&log);
    const std::weak_ptr<DestroyedMarker> alive = marker;
    watch(*model->find(QStringLiteral("g/a")), log, refuses, nullptr);
    watch(*model->find(QStringLiteral("g/b")), log, false, nullptr);
    watch(*model->find(QStringLiteral("g/sub/c")), log, false, nullptr);
    watch(*model->find(QStringLiteral("tail")), log, false, marker);
    marker.reset();

    const auto remove = [model, &log] {
        if (log.deletions++ > 0)
            return;
        log.events << QStringLiteral("deleted");
        delete model;
    };
    switch (Notification(notification)) {
    case Notification::Value:
        QObject::connect(model, &PropertyModel::valueChanged, remove);
        break;
    case Notification::Batch:
        QObject::connect(model, &PropertyModel::batchValueChanged, remove);
        break;
    case Notification::Failure:
        QObject::connect(model, &PropertyModel::validationFailed, remove);
        break;
    case Notification::Data:
        QObject::connect(model, &QAbstractItemModel::dataChanged, remove);
        break;
    }

    PropertyGroup* group = model->find(QStringLiteral("g"))->toGroup();
    bool result = true;
    switch (GroupWalk(walk)) {
    case GroupWalk::Reset:
        result = group->resetToDefault();
        break;
    case GroupWalk::Normalize:
        result = group->normalizeValue();
        break;
    case GroupWalk::MakeDefault:
        group->makeValueDefault();
        break;
    }

    // false only when a value was rejected
    QCOMPARE(result, !refuses);
    QCOMPARE(log.deletions, 1);
    QVERIFY(guard.isNull());
    QVERIFY(alive.expired()); // nothing leaks
    QStringList visits;
    for (const QString& event : std::as_const(log.events)) {
        if (event.startsWith(QLatin1String("visit:")))
            visits << event;
    }
    if (GroupWalk(walk) == GroupWalk::MakeDefault) {
        QVERIFY(visits.isEmpty()); // defaults are not written through the pipeline
        QCOMPARE(log.events,
            (QStringList {QStringLiteral("deleted"), QStringLiteral("destroyed")}));
        return;
    }
    // Every descendant of the group is visited once, the rest of the tree not.
    QCOMPARE(visits,
        (QStringList {QStringLiteral("visit:g/a"), QStringLiteral("visit:g/b"),
            QStringLiteral("visit:g/sub/c")}));
    // The tree is destroyed after everything the walk did.
    QCOMPARE(log.events.last(), QStringLiteral("destroyed"));
    QCOMPARE(log.events.count(QStringLiteral("deleted")), 1);
    QCOMPARE(log.events.count(QStringLiteral("destroyed")), 1);
    const int deleted = log.events.indexOf(QStringLiteral("deleted"));
    if (Notification(notification) == Notification::Batch)
        QCOMPARE(deleted, log.events.size() - 2); // announced after the last write
    else
        QCOMPARE(deleted, 1); // after the first visit, before the second
}

QTEST_APPLESS_MAIN(tst_Property)
#include "tst_property.moc"
