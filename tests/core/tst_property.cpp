// Property, PropertyGroup and builders without a model (docs/PLAN.md M2.1-M2.3).

#include <qpb/qpbcore.h>

#include <QRegularExpression>
#include <QTest>

using namespace qpb;

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
    QCOMPARE(n.value().metaType(), QMetaType::fromType<int>());
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
// set values (found in the RC trial, SPEC D34). Validation still applies.
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

QTEST_APPLESS_MAIN(tst_Property)
#include "tst_property.moc"
