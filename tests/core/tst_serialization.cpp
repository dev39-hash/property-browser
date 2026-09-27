// Serialization of property values (docs/PLAN.md M6.2, SPEC 4.8).

#include <qpb/qpbcore.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace qpb;

namespace {

std::unique_ptr<PropertyGroup> createSettings()
{
    auto root = PropertyGroup::create(QStringLiteral("Settings"));
    PropertyGroup& general = root->addGroup(QStringLiteral("General"));
    general.addEnum(QStringLiteral("language"),
        QList<EnumOption>({{QStringLiteral("English"), QStringLiteral("en")},
            {QStringLiteral("Vietnamese"), QStringLiteral("vi")}}),
        QStringLiteral("en"));
    general.addBool(QStringLiteral("autosave"), true);
    general.addInt(QStringLiteral("minutes"), 5).range(1, 60);
    general.addEnum(QStringLiteral("theme"), {QStringLiteral("Light"), QStringLiteral("Dark")}, 0);
    PropertyGroup& limits = root->addGroup(QStringLiteral("Limits"));
    limits.addDouble(QStringLiteral("ratio"), 0.5);
    limits.addInt64(QStringLiteral("big"), 1);
    limits.addString(QStringLiteral("notes"), QString()).multiline();
    root->addString(QStringLiteral("version"), QStringLiteral("1.2")).readOnly();
    return root;
}

void modify(PropertyGroup& root)
{
    QVERIFY(root.find(QStringLiteral("General/language"))->setValue(QStringLiteral("vi")));
    QVERIFY(root.find(QStringLiteral("General/autosave"))->setValue(false));
    QVERIFY(root.find(QStringLiteral("General/minutes"))->setValue(30));
    QVERIFY(root.find(QStringLiteral("General/theme"))->setValue(1));
    QVERIFY(root.find(QStringLiteral("Limits/ratio"))->setValue(0.25));
    QVERIFY(root.find(QStringLiteral("Limits/big"))->setValue(qint64(1) << 60));
    QVERIFY(root.find(QStringLiteral("Limits/notes"))->setValue(QStringLiteral("a\nb")));
}

void compareValues(const PropertyGroup& a, const PropertyGroup& b)
{
    const QStringList paths {"General/language", "General/autosave", "General/minutes",
        "General/theme", "Limits/ratio", "Limits/big", "Limits/notes", "version"};
    for (const QString& path : paths)
        QCOMPARE(a.find(path)->value(), b.find(path)->value());
}

} // namespace

class tst_Serialization : public QObject
{
    Q_OBJECT

private slots:
    void jsonLayout();
    void jsonRoundTrip();
    void fromJsonIgnoresUnknownAndReadOnly();
    void fromJsonReportsRejectedValues();
    void customTypeJson();
    void settingsRoundTrip_data();
    void settingsRoundTrip();
    void settingsUnderGroupAndMissingKeys();
    void batchOnModel();
};

void tst_Serialization::jsonLayout()
{
    auto root = createSettings();
    modify(*root);
    const QJsonObject json = serialization::toJson(*root);
    QCOMPARE(json.keys(), QStringList({"General", "Limits"})); // no read-only "version"
    const QJsonObject general = json.value(QStringLiteral("General")).toObject();
    QCOMPARE(general.value(QStringLiteral("language")).toString(), QStringLiteral("vi"));
    QCOMPARE(general.value(QStringLiteral("autosave")).toBool(), false);
    QCOMPARE(general.value(QStringLiteral("minutes")).toInt(), 30);
    QCOMPARE(general.value(QStringLiteral("theme")).toInt(), 1);
    const QJsonObject limits = json.value(QStringLiteral("Limits")).toObject();
    QCOMPARE(limits.value(QStringLiteral("ratio")).toDouble(), 0.25);
    // Beyond 2^53: a string, so no precision is lost.
    QCOMPARE(limits.value(QStringLiteral("big")).toString(), QString::number(qint64(1) << 60));
    QCOMPARE(limits.value(QStringLiteral("notes")).toString(), QStringLiteral("a\nb"));
}

void tst_Serialization::jsonRoundTrip()
{
    auto saved = createSettings();
    modify(*saved);
    const QByteArray text = QJsonDocument(serialization::toJson(*saved)).toJson();

    auto restored = createSettings();
    QVERIFY(serialization::fromJson(*restored, QJsonDocument::fromJson(text).object()));
    compareValues(*saved, *restored);

    // Small 64-bit values are plain numbers and read back as such.
    QVERIFY(saved->find(QStringLiteral("Limits/big"))->setValue(qint64(-12)));
    const QJsonObject json = serialization::toJson(*saved);
    QVERIFY(
        json.value(QStringLiteral("Limits")).toObject().value(QStringLiteral("big")).isDouble());
    QVERIFY(serialization::fromJson(*restored, json));
    QCOMPARE(restored->find(QStringLiteral("Limits/big"))->value(), QVariant(qint64(-12)));
}

void tst_Serialization::fromJsonIgnoresUnknownAndReadOnly()
{
    auto root = createSettings();
    const QJsonObject json = QJsonDocument::fromJson(R"({
        "General": {"minutes": 12, "unknown": 1},
        "Limits": 5,
        "Other": {"x": 1},
        "version": "9.9"
    })")
                                 .object();
    QVERIFY(serialization::fromJson(*root, json));
    QCOMPARE(root->find(QStringLiteral("General/minutes"))->value(), QVariant(12));
    QCOMPARE(root->find(QStringLiteral("version"))->value(), QVariant(QStringLiteral("1.2")));
}

void tst_Serialization::fromJsonReportsRejectedValues()
{
    auto root = createSettings();
    const QJsonObject json = QJsonDocument::fromJson(R"({
        "General": {"language": "fr", "minutes": 20}
    })")
                                 .object();
    QVERIFY(!serialization::fromJson(*root, json));
    QCOMPARE(
        root->find(QStringLiteral("General/language"))->value(), QVariant(QStringLiteral("en")));
    QCOMPARE(root->find(QStringLiteral("General/minutes"))->value(), QVariant(20)); // still applied
}

void tst_Serialization::customTypeJson()
{
    // A type with its own JSON form: a pair of ints stored as QPoint-like "x,y" text.
    const TypeId pairType = QStringLiteral("test.pair");
    if (!TypeRegistry::global().contains(pairType)) {
        TypeHandler handler;
        handler.toJson = [](const QVariant& value, const Property&) {
            const QList<QVariant> pair = value.toList();
            return QJsonValue(QJsonArray({pair.value(0).toInt(), pair.value(1).toInt()}));
        };
        handler.fromJson = [](const QJsonValue& json, const Property&) {
            const QJsonArray array = json.toArray();
            return QVariant(QVariantList({array.at(0).toInt(), array.at(1).toInt()}));
        };
        QVERIFY(TypeRegistry::global().registerType<QVariantList>(pairType, handler));
    }
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->add(pairType, QStringLiteral("size"), QVariantList({3, 4}));
    const QJsonObject json = serialization::toJson(*root);
    QCOMPARE(json.value(QStringLiteral("size")).toArray(), QJsonArray({3, 4}));

    auto restored = PropertyGroup::create(QStringLiteral("root"));
    restored->add(pairType, QStringLiteral("size"), QVariantList({0, 0}));
    QVERIFY(serialization::fromJson(*restored, json));
    QCOMPARE(restored->child(QStringLiteral("size"))->value(), QVariant(QVariantList({3, 4})));
}

void tst_Serialization::settingsRoundTrip_data()
{
    QTest::addColumn<int>("format");
    QTest::newRow("ini") << int(QSettings::IniFormat); // every value becomes text
    QTest::newRow("native") << int(QSettings::NativeFormat);
}

void tst_Serialization::settingsRoundTrip()
{
    QFETCH(int, format);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(QStringLiteral("settings.conf"));
    auto saved = createSettings();
    modify(*saved);
    {
        QSettings settings(file, QSettings::Format(format));
        serialization::save(*saved, settings);
        QVERIFY(!settings.contains(QStringLiteral("version")));
        QCOMPARE(settings.value(QStringLiteral("General/minutes")).toInt(), 30);
    }
    QSettings settings(file, QSettings::Format(format));
    auto restored = createSettings();
    QVERIFY(serialization::load(*restored, settings));
    compareValues(*saved, *restored);
}

void tst_Serialization::settingsUnderGroupAndMissingKeys()
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath(QStringLiteral("s.ini")), QSettings::IniFormat);
    auto root = createSettings();
    modify(*root);
    settings.beginGroup(QStringLiteral("App"));
    serialization::save(*root->find(QStringLiteral("General"))->toGroup(), settings);
    settings.endGroup();
    QCOMPARE(settings.value(QStringLiteral("App/language")).toString(), QStringLiteral("vi"));

    auto restored = createSettings();
    settings.beginGroup(QStringLiteral("App"));
    QVERIFY(serialization::load(*restored->find(QStringLiteral("General"))->toGroup(), settings));
    settings.endGroup();
    QCOMPARE(restored->find(QStringLiteral("General/minutes"))->value(), QVariant(30));
    // Nothing stored for Limits: unchanged.
    QVERIFY(serialization::load(*restored, settings));
    QCOMPARE(restored->find(QStringLiteral("Limits/ratio"))->value(), QVariant(0.5));
}

void tst_Serialization::batchOnModel()
{
    PropertyModel model(createSettings());
    auto source = createSettings();
    modify(*source);
    QSignalSpy batch(&model, &PropertyModel::batchValueChanged);
    model.beginBatch();
    QVERIFY(serialization::fromJson(*model.root(), serialization::toJson(*source)));
    model.endBatch();
    QCOMPARE(batch.count(), 1);
    QCOMPARE(batch[0][0].toStringList().size(), 7);
}

QTEST_GUILESS_MAIN(tst_Serialization)
#include "tst_serialization.moc"
