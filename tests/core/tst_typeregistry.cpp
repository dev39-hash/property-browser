// TypeRegistry and the built-in types (see qpb/doc/guide/custom-types-and-editors.md).

#include <qpb/qpbcore.h>

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QPoint>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <limits>

using namespace qpb;

namespace {

// TypeHandler::storageType: a QMetaType with Qt 6, a type id with Qt 5.
template <class T> auto storageTypeFor()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QMetaType::fromType<T>();
#else
    return qMetaTypeId<T>();
#endif
}

} // namespace

namespace {

QString display(const Property& property)
{
    const TypeHandler* handler = TypeRegistry::global().handler(property.typeId());
    if (handler && handler->displayText)
        return handler->displayText(property.value(), property);
    return property.value().toString();
}

} // namespace

class tst_TypeRegistry : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void builtinTypesAreRegistered();
    void boolType();
    void intClampsAndDisplays();
    void intConversion();
    void int64ClampsDisplaysAndConverts();
    void doubleRoundsClampsAndDisplays();
    void doubleRejectsNaN();
    void stringMaxLengthAndPattern();
    void stringInvalidPatternIsIgnored();
    void stringMultiline();
    void enumByIndex();
    void enumByStringValue();
    void filePathMustExist();
    void dirPathMustExist();
    void pathsDisplayNative();

    void registerCustomType();
    void reservedIdsAreRejected();
    void replaceType();

    // Doubles stored without rounding.
    void roundToDecimalsKeyAndSpelling();
    void doubleWithoutRoundingKeepsEveryDigit();
    void doubleWithoutRoundingStillClamps();
    void doubleWithoutRoundingDisplaysItsDecimals();
    void doubleWithoutRoundingConvertsOtherTypes();
    void doubleRoundsByDefaultAndWhenAskedTo();
    void roundingAttributeChangesNeverMoveTheValue();
};

void tst_TypeRegistry::initTestCase()
{
    QLocale::setDefault(QLocale::c());
}

void tst_TypeRegistry::builtinTypesAreRegistered()
{
    const QList<TypeId> expected {Types::Bool, Types::Int, Types::Double, Types::String,
        Types::Enum, Types::FilePath, Types::DirPath};
    const QList<TypeId> types = TypeRegistry::global().types();
    QVERIFY(types.size() >= expected.size());
    QCOMPARE(types.mid(0, expected.size()), expected);
    for (const TypeId& id : expected) {
        QVERIFY(TypeRegistry::global().contains(id));
        QVERIFY(TypeRegistry::global().handler(id));
    }
    QVERIFY(!TypeRegistry::global().contains(Types::Group));
    QCOMPARE(TypeRegistry::global().handler(QStringLiteral("test.none")), nullptr);
}

void tst_TypeRegistry::boolType()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& b = root->addBool(QStringLiteral("b"), false);
    QVERIFY(b.setValue(1));
    QCOMPARE(b.value(), QVariant(true));
    QCOMPARE(b.value().userType(), qMetaTypeId<bool>());
}

void tst_TypeRegistry::intClampsAndDisplays()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& i = root->addInt(QStringLiteral("i"), 5)
                      .range(0, 10)
                      .prefix(QStringLiteral("#"))
                      .suffix(QStringLiteral(" px"));
    QVERIFY(i.setValue(42));
    QCOMPARE(i.value(), QVariant(10));
    QVERIFY(i.setValue(-3));
    QCOMPARE(i.value(), QVariant(0));
    QVERIFY(i.setValue(1234));
    QCOMPARE(i.value(), QVariant(10));
    QCOMPARE(display(i), QStringLiteral("#10 px"));

    Property& big = root->addInt(QStringLiteral("big"), 1234567);
    QCOMPARE(display(big), QLocale().toString(1234567));
}

// Since 1.2.
void tst_TypeRegistry::int64ClampsDisplaysAndConverts()
{
    QVERIFY(TypeRegistry::global().contains(Types::Int64));
    QCOMPARE(TypeRegistry::global().types().indexOf(TypeId(Types::Int64)), 7); // after 1.0's
    QCOMPARE(
        TypeRegistry::global().handler(Types::Int64)->storageType, storageTypeFor<qint64>());

    auto root = PropertyGroup::create(QStringLiteral("r"));
    const qint64 big = qint64(1) << 40;
    Property& n = root->addInt64(QStringLiteral("n"), 5)
                      .range(-big, big)
                      .step(1000)
                      .prefix(QStringLiteral("#"))
                      .suffix(QStringLiteral(" B"));
    QCOMPARE(n.typeId(), TypeId(Types::Int64));
    QCOMPARE(n.value(), QVariant(qint64(5)));
    QCOMPARE(n.attribute(Attr::Step), QVariant(qint64(1000)));
    QVERIFY(n.setValue(big * 4));
    QCOMPARE(n.value(), QVariant(big));
    QVERIFY(n.setValue(-big - 1));
    QCOMPARE(n.value(), QVariant(-big));
    QCOMPARE(display(n), QStringLiteral("#") + QLocale().toString(-big) + QStringLiteral(" B"));

    // Conversion from other types, e.g. text read from an INI file.
    QVERIFY(n.setValue(QStringLiteral("123456789012")));
    QCOMPARE(n.value(), QVariant(qint64(123456789012)));
    QVERIFY(n.setValue(7)); // int
    QCOMPARE(n.value(), QVariant(qint64(7)));

    // Without attributes: the whole range.
    Property& full = root->addInt64(QStringLiteral("full"), std::numeric_limits<qint64>::max());
    QCOMPARE(full.value(), QVariant(std::numeric_limits<qint64>::max()));
}

void tst_TypeRegistry::intConversion()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& i = root->addInt(QStringLiteral("i"), 0);
    QVERIFY(i.setValue(QStringLiteral("17")));
    QCOMPARE(i.value(), QVariant(17));
    QVERIFY(!i.setValue(QStringLiteral("abc")));
    QCOMPARE(i.value(), QVariant(17));
}

void tst_TypeRegistry::doubleRoundsClampsAndDisplays()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& d = root->addDouble(QStringLiteral("d"), 0.0).range(-1.0, 1.0);
    QVERIFY(d.setValue(0.123456));
    QCOMPARE(d.value(), QVariant(0.12)); // default: 2 decimals
    QVERIFY(d.setValue(5.0));
    QCOMPARE(d.value(), QVariant(1.0));
    QCOMPARE(display(d), QStringLiteral("1"));
    QVERIFY(d.setValue(0.5));
    QCOMPARE(display(d), QStringLiteral("0.5"));

    Property& p
        = root->addDouble(QStringLiteral("p"), 0.0).decimals(3).suffix(QStringLiteral(" m"));
    QVERIFY(p.setValue(2.34567));
    QCOMPARE(p.value(), QVariant(2.346));
    QCOMPARE(display(p), QStringLiteral("2.346 m"));

    Property& unbounded = root->addDouble(QStringLiteral("u"), 0.0);
    QVERIFY(unbounded.setValue(std::numeric_limits<double>::infinity()));
    QVERIFY(qIsInf(unbounded.value().toDouble()));
}

void tst_TypeRegistry::doubleRejectsNaN()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& d = root->addDouble(QStringLiteral("d"), 1.0);
    QVERIFY(!d.setValue(std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(d.value(), QVariant(1.0));
}

void tst_TypeRegistry::stringMaxLengthAndPattern()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& s = root->addString(QStringLiteral("s"), QStringLiteral("abc"))
                      .maxLength(4)
                      .regularExpression(QStringLiteral("[a-c]+"));
    QVERIFY(s.setValue(QStringLiteral("cab")));
    QVERIFY(!s.setValue(QStringLiteral("abcab"))); // too long
    QVERIFY(!s.setValue(QStringLiteral("abd"))); // pattern
    QVERIFY(!s.setValue(QStringLiteral("xabc"))); // the whole value must match
    QCOMPARE(s.value(), QVariant(QStringLiteral("cab")));

    Property& n = root->addString(QStringLiteral("n"), QString());
    QVERIFY(n.setValue(42));
    QCOMPARE(n.value(), QVariant(QStringLiteral("42")));
}

// Since 1.1: multi-line strings keep their line breaks and display on one line.
void tst_TypeRegistry::stringMultiline()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    Property& plain = root->addString(QStringLiteral("plain"), QStringLiteral("a\nb"));
    Property& notes
        = root->addString(QStringLiteral("notes"), QStringLiteral("one\r\ntwo\nthree")).multiline();
    QCOMPARE(notes.attribute(Attr::Multiline), QVariant(true));
    QCOMPARE(notes.value().toString(), QStringLiteral("one\r\ntwo\nthree"));
    QCOMPARE(display(notes), QStringLiteral("one \u00B6 two \u00B6 three"));
    QCOMPARE(display(plain), QStringLiteral("a\nb")); // unchanged without the attribute

    root->addString(QStringLiteral("off"), QString()).multiline(false);
    QCOMPARE(root->child(QStringLiteral("off"))->attribute(Attr::Multiline), QVariant(false));
}

void tst_TypeRegistry::stringInvalidPatternIsIgnored()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& s = root->addString(QStringLiteral("s"), QString())
                      .regularExpression(QStringLiteral("(unclosed"));
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("invalid regular expression")));
    QVERIFY(s.setValue(QStringLiteral("anything")));
}

void tst_TypeRegistry::enumByIndex()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& e = root->addEnum(
        QStringLiteral("e"), {QStringLiteral("Perspective"), QStringLiteral("Orthographic")}, 0);
    QCOMPARE(display(e), QStringLiteral("Perspective"));
    QVERIFY(e.setValue(1));
    QCOMPARE(display(e), QStringLiteral("Orthographic"));
    QVERIFY(e.setValue(QStringLiteral("0"))); // normalized onto the int option
    QCOMPARE(e.value(), QVariant(0));
    QCOMPARE(e.value().userType(), qMetaTypeId<int>());
    QVERIFY(!e.setValue(2));
    QCOMPARE(e.value(), QVariant(0));
}

void tst_TypeRegistry::enumByStringValue()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& e = root->addEnum(QStringLiteral("lang"),
        {EnumOption {QStringLiteral("English"), QStringLiteral("en")},
            EnumOption {QStringLiteral("Vietnamese"), QStringLiteral("vi")}},
        QStringLiteral("en"));
    QCOMPARE(display(e), QStringLiteral("English"));
    QVERIFY(e.setValue(QStringLiteral("vi")));
    QCOMPARE(display(e), QStringLiteral("Vietnamese"));
    QVERIFY(!e.setValue(QStringLiteral("fr")));
}

void tst_TypeRegistry::filePathMustExist()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString existing = dir.filePath(QStringLiteral("a.txt"));
    QFile file(existing);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    const QString missing = dir.filePath(QStringLiteral("missing.txt"));

    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& open = root->addFilePath(QStringLiteral("open"), QString()).mustExist();
    QVERIFY(open.setValue(existing));
    QVERIFY(!open.setValue(missing));
    QVERIFY(!open.setValue(dir.path())); // a directory is not a file
    QVERIFY(open.setValue(QString())); // empty is always valid

    Property& save = root->addFilePath(QStringLiteral("save"), QString())
                         .dialogMode(FileMode::Save)
                         .mustExist();
    QVERIFY(save.setValue(missing));

    Property& free = root->addFilePath(QStringLiteral("free"), QString());
    QVERIFY(free.setValue(missing));
}

void tst_TypeRegistry::dirPathMustExist()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->addDirPath(QStringLiteral("p"), QString()).mustExist();
    QVERIFY(p.setValue(dir.path()));
    QVERIFY(!p.setValue(dir.filePath(QStringLiteral("nope"))));
    QVERIFY(p.setValue(QString()));
}

void tst_TypeRegistry::pathsDisplayNative()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& f = root->addFilePath(QStringLiteral("f"), QStringLiteral("/a/b.txt"));
    QCOMPARE(display(f), QDir::toNativeSeparators(QStringLiteral("/a/b.txt")));
}

void tst_TypeRegistry::registerCustomType()
{
    const TypeId id = QStringLiteral("test.point");
    TypeHandler handler;
    handler.displayText = [](const QVariant& value, const Property&) {
        const QPoint point = value.toPoint();
        return QStringLiteral("(%1, %2)").arg(point.x()).arg(point.y());
    };
    handler.validate = [](const QVariant& value, const Property&) {
        return value.toPoint().x() >= 0 ? ValidationResult::valid()
                                        : ValidationResult::error(QStringLiteral("negative"));
    };
    QVERIFY(TypeRegistry::global().registerType<QPoint>(id, handler));
    QVERIFY(TypeRegistry::global().contains(id));
    QCOMPARE(TypeRegistry::global().handler(id)->storageType, storageTypeFor<QPoint>());
    QCOMPARE(TypeRegistry::global().types().last(), id);
    QVERIFY(!TypeRegistry::global().registerType(id, handler)); // already registered

    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& p = root->add(id, QStringLiteral("p"), QPoint(1, 2));
    QCOMPARE(display(p), QStringLiteral("(1, 2)"));
    QVERIFY(p.setValue(QPoint(3, 4)));
    QVERIFY(!p.setValue(QPoint(-1, 0)));
    QCOMPARE(p.value(), QVariant(QPoint(3, 4)));
}

void tst_TypeRegistry::reservedIdsAreRejected()
{
    TypeRegistry& registry = TypeRegistry::global();
    const TypeHandler handler;
    QVERIFY(!registry.registerType(QString(), handler));
    QVERIFY(!registry.registerType(QStringLiteral("qpb.anything"), handler));
    QVERIFY(!registry.registerType(Types::Group, handler));
    QVERIFY(!registry.registerType(Types::Int, handler));
}

void tst_TypeRegistry::replaceType()
{
    TypeRegistry& registry = TypeRegistry::global();
    const TypeId id = QStringLiteral("test.replaceable");
    TypeHandler first;
    first.displayText = [](const QVariant&, const Property&) { return QStringLiteral("first"); };
    QVERIFY(registry.registerType<int>(id, first));
    const TypeHandler* pointer = registry.handler(id);

    TypeHandler second = first;
    second.displayText = [](const QVariant&, const Property&) { return QStringLiteral("second"); };
    QVERIFY(registry.replaceType(id, second));
    QCOMPARE(registry.handler(id), pointer); // same entry, new content

    auto root = PropertyGroup::create(QStringLiteral("r"));
    QCOMPARE(display(root->add(id, QStringLiteral("p"), 1)), QStringLiteral("second"));
    QVERIFY(!registry.replaceType(QStringLiteral("test.missing"), second));
}

// The attribute key is the documented spelling; a map written with that spelling
// switches rounding off.
void tst_TypeRegistry::roundToDecimalsKeyAndSpelling()
{
    QCOMPARE(QString(Attr::RoundToDecimals), QStringLiteral("roundToDecimals"));

    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& d = root->addDouble(QStringLiteral("d"), 0.0);
    d.setAttribute(QStringLiteral("roundToDecimals"), false);
    QVERIFY(d.setValue(0.0125));
    QCOMPARE(d.value(), QVariant(0.0125));
}

// With roundToDecimals false every digit of an assigned double is stored,
// whatever "decimals" says (the default of 2 included).
void tst_TypeRegistry::doubleWithoutRoundingKeepsEveryDigit()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& d = root->addDouble(QStringLiteral("d"), 0.0).attribute(Attr::RoundToDecimals, false);

    QVERIFY(d.setValue(0.0125));
    QCOMPARE(d.value(), QVariant(0.0125));
    QVERIFY(d.setValue(1234.5678));
    QCOMPARE(d.value(), QVariant(1234.5678));
    QVERIFY(d.setValue(-0.000123456));
    QCOMPARE(d.value(), QVariant(-0.000123456));
    QVERIFY(d.setValue(0.3333333333333333));
    QCOMPARE(d.value(), QVariant(0.3333333333333333));
    QCOMPARE(d.value().userType(), qMetaTypeId<double>());

    d.setAttribute(Attr::Decimals, 0);
    QVERIFY(d.setValue(2.71828));
    QCOMPARE(d.value(), QVariant(2.71828));
    d.setAttribute(Attr::Decimals, 6);
    QVERIFY(d.setValue(2.718281828));
    QCOMPARE(d.value(), QVariant(2.718281828));

    // Values beyond the range of exact decimals, and the infinities, are kept too.
    QVERIFY(d.setValue(123456789012.345678));
    QCOMPARE(d.value(), QVariant(123456789012.345678));
    QVERIFY(d.setValue(std::numeric_limits<double>::infinity()));
    QVERIFY(qIsInf(d.value().toDouble()));
}

// Unrounded values are still limited to the range.
void tst_TypeRegistry::doubleWithoutRoundingStillClamps()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& d = root->addDouble(QStringLiteral("d"), 0.0)
                      .range(-1.0, 1.0)
                      .attribute(Attr::RoundToDecimals, false);
    QVERIFY(d.setValue(0.123456));
    QCOMPARE(d.value(), QVariant(0.123456));
    QVERIFY(d.setValue(5.0));
    QCOMPARE(d.value(), QVariant(1.0));
    QVERIFY(d.setValue(-3.14159));
    QCOMPARE(d.value(), QVariant(-1.0));

    // A limit with more digits than "decimals" is reached exactly.
    Property& narrow = root->addDouble(QStringLiteral("narrow"), 0.0)
                           .range(0.0, 0.0125)
                           .attribute(Attr::RoundToDecimals, false);
    QVERIFY(narrow.setValue(7.0));
    QCOMPARE(narrow.value(), QVariant(0.0125));
    QVERIFY(narrow.setValue(0.0124));
    QCOMPARE(narrow.value(), QVariant(0.0124));

    // The type's own checks are unchanged: not a number is refused.
    QVERIFY(!d.setValue(std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(d.value(), QVariant(-1.0));
}

// "decimals" then only decides the displayed digits (trailing zeros removed,
// prefix and suffix as before); the stored value does not change.
void tst_TypeRegistry::doubleWithoutRoundingDisplaysItsDecimals()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& d = root->addDouble(QStringLiteral("d"), 0.0)
                      .suffix(QStringLiteral(" mm"))
                      .attribute(Attr::RoundToDecimals, false);

    QVERIFY(d.setValue(0.0125));
    QCOMPARE(display(d), QStringLiteral("0.01 mm"));
    QVERIFY(d.setValue(1234.5678));
    QCOMPARE(display(d), QStringLiteral("1234.57 mm"));
    QCOMPARE(d.value(), QVariant(1234.5678));

    d.setAttribute(Attr::Decimals, 4);
    QCOMPARE(display(d), QStringLiteral("1234.5678 mm"));
    d.setAttribute(Attr::Decimals, 0);
    QCOMPARE(display(d), QStringLiteral("1235 mm"));
    d.setAttribute(Attr::Decimals, 1);
    QVERIFY(d.setValue(0.5));
    QCOMPARE(display(d), QStringLiteral("0.5 mm"));
    QVERIFY(d.setValue(2.0));
    QCOMPARE(display(d), QStringLiteral("2 mm"));
}

// Text read from an INI file and integers reach the property as doubles
// without rounding.
void tst_TypeRegistry::doubleWithoutRoundingConvertsOtherTypes()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& d = root->addDouble(QStringLiteral("d"), 0.0).attribute(Attr::RoundToDecimals, false);
    QVERIFY(d.setValue(QStringLiteral("1234.5678")));
    QCOMPARE(d.value(), QVariant(1234.5678));
    QVERIFY(d.setValue(QStringLiteral("0.0125")));
    QCOMPARE(d.value(), QVariant(0.0125));
    QVERIFY(d.setValue(3));
    QCOMPARE(d.value(), QVariant(3.0));
    QCOMPARE(d.value().userType(), qMetaTypeId<double>());
    QVERIFY(!d.setValue(QStringLiteral("abc")));
    QCOMPARE(d.value(), QVariant(3.0));
}

// Guard (passes without the new attribute): no attribute and "true" round to
// the decimals as before.
void tst_TypeRegistry::doubleRoundsByDefaultAndWhenAskedTo()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& plain = root->addDouble(QStringLiteral("plain"), 0.0);
    Property& asked = root->addDouble(QStringLiteral("asked"), 0.0)
                          .attribute(Attr::RoundToDecimals, true);
    Property& three = root->addDouble(QStringLiteral("three"), 0.0)
                          .decimals(3)
                          .attribute(Attr::RoundToDecimals, true);

    QVERIFY(plain.setValue(0.0125));
    QCOMPARE(plain.value(), QVariant(0.01));
    QVERIFY(plain.setValue(1234.5678));
    QCOMPARE(plain.value(), QVariant(1234.57));
    QVERIFY(asked.setValue(0.0125));
    QCOMPARE(asked.value(), QVariant(0.01));
    QVERIFY(asked.setValue(1234.5678));
    QCOMPARE(asked.value(), QVariant(1234.57));
    QVERIFY(three.setValue(2.34567));
    QCOMPARE(three.value(), QVariant(2.346));
    QCOMPARE(display(three), QStringLiteral("2.346"));
}

// Guard for the rule that attribute changes never move a stored value: turning
// rounding on again keeps the digits until the value is written or normalised.
void tst_TypeRegistry::roundingAttributeChangesNeverMoveTheValue()
{
    auto root = PropertyGroup::create(QStringLiteral("r"));
    Property& d = root->addDouble(QStringLiteral("d"), 0.0).attribute(Attr::RoundToDecimals, false);
    QVERIFY(d.setValue(0.0125));

    d.setAttribute(Attr::RoundToDecimals, true);
    QCOMPARE(d.value(), QVariant(0.0125));
    QVERIFY(d.normalizeValue());
    QCOMPARE(d.value(), QVariant(0.01));

    d.setAttribute(Attr::RoundToDecimals, false);
    QCOMPARE(d.value(), QVariant(0.01)); // nothing brings the digits back
    QVERIFY(d.setValue(0.0125));
    QCOMPARE(d.value(), QVariant(0.0125));
    QVERIFY(d.normalizeValue()); // keeps them
    QCOMPARE(d.value(), QVariant(0.0125));
}

QTEST_APPLESS_MAIN(tst_TypeRegistry)
#include "tst_typeregistry.moc"
