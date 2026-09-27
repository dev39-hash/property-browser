// API compatibility test for qpb 1.2 (docs/SPEC.md §9.5, PLAN M6.5).
//
// Exercises every public API added in 1.2 the way client code would. Once
// 1.2.0 is released this file is FROZEN: never edit it. Every later 1.x
// release must compile it (deprecation warnings allowed) and pass it unchanged.

#include <qpb/qpb.h>

#include <QApplication>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <limits>
#include <type_traits>

namespace {

class Device : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("qpb:properties", "label,capacity")
    Q_CLASSINFO("qpb:capacity", "min=0;suffix= B")
    Q_PROPERTY(QString label MEMBER m_label NOTIFY labelChanged)
    Q_PROPERTY(qint64 capacity MEMBER m_capacity NOTIFY capacityChanged)

public:
    QString m_label = QStringLiteral("disk");
    qint64 m_capacity = qint64(1) << 42;

signals:
    void labelChanged();
    void capacityChanged();
};

// An application's own function with the same name and argument types as a
// qpb one: unqualified calls must stay unambiguous.
int save(const qpb::PropertyGroup&, QSettings&)
{
    return 1;
}

} // namespace

class tst_ApiCompat_1_2 : public QObject
{
    Q_OBJECT

private slots:
    void int64Type();
    void serialization();
    void typeHandlerJsonFields();
    void qobjectPropertySource();
};

void tst_ApiCompat_1_2::int64Type()
{
    QCOMPARE(QString(qpb::Types::Int64), QStringLiteral("int64"));
    QVERIFY(qpb::TypeRegistry::global().contains(qpb::Types::Int64));
    QVERIFY(qpb::EditorFactory::global().contains(qpb::Types::Int64));

    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    qpb::Int64Builder builder = root->addInt64(QStringLiteral("n"), 1);
    qpb::Int64Builder& chained = builder.range(0, qint64(1) << 50)
                                     .minimum(0)
                                     .maximum(qint64(1) << 50)
                                     .step(1024)
                                     .prefix(QStringLiteral("~"))
                                     .suffix(QStringLiteral(" B"));
    QCOMPARE(&chained, &builder);
    qpb::Property& n = builder;
    QVERIFY(n.setValue(std::numeric_limits<qint64>::max()));
    QCOMPARE(n.value(), QVariant(qint64(1) << 50));
}

void tst_ApiCompat_1_2::serialization()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addGroup(QStringLiteral("g")).addInt(QStringLiteral("i"), 1);
    root->addString(QStringLiteral("fixed"), QStringLiteral("x")).readOnly();
    QVERIFY(root->find(QStringLiteral("g/i"))->setValue(5));

    const QJsonObject json = qpb::serialization::toJson(*root);
    QCOMPARE(json.value(QStringLiteral("g")).toObject().value(QStringLiteral("i")).toInt(), 5);
    QVERIFY(!json.contains(QStringLiteral("fixed")));
    auto copy = qpb::PropertyGroup::create(QStringLiteral("root"));
    copy->addGroup(QStringLiteral("g")).addInt(QStringLiteral("i"), 1);
    const bool loaded = qpb::serialization::fromJson(*copy, json);
    QVERIFY(loaded);
    QCOMPARE(copy->find(QStringLiteral("g/i"))->value(), QVariant(5));

    QTemporaryDir dir;
    QSettings settings(dir.filePath(QStringLiteral("s.ini")), QSettings::IniFormat);
    qpb::serialization::save(*root, settings);
    QCOMPARE(save(*root, settings), 1); // the application's own function
    QVERIFY(qpb::serialization::load(*copy, settings));
    QCOMPARE(settings.value(QStringLiteral("g/i")).toInt(), 5);
}

void tst_ApiCompat_1_2::typeHandlerJsonFields()
{
    qpb::TypeHandler handler;
    handler.toJson = [](const QVariant& value, const qpb::Property&) {
        return QJsonValue(value.toString().toUpper());
    };
    handler.fromJson = [](const QJsonValue& json, const qpb::Property&) {
        return QVariant(json.toString().toLower());
    };
    const qpb::TypeId id = QStringLiteral("compat.upper");
    QVERIFY(qpb::TypeRegistry::global().registerType<QString>(id, handler));
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->add(id, QStringLiteral("s"), QStringLiteral("abc"));
    QCOMPARE(qpb::serialization::toJson(*root).value(QStringLiteral("s")).toString(),
        QStringLiteral("ABC"));
}

void tst_ApiCompat_1_2::qobjectPropertySource()
{
    static_assert(std::is_base_of_v<QObject, qpb::QObjectPropertySource>);
    qpb::PropertyModel model(qpb::PropertyGroup::create(QStringLiteral("root")));
    qpb::QObjectPropertySource source(&model);
    QCOMPARE(source.model(), &model);
    Device device;
    qpb::PropertyGroup* group = source.addObject(&device);
    QVERIFY(group);
    QCOMPARE(source.addObject(&device, model.root(), QStringLiteral("again")), group);
    QCOMPARE(source.groupOf(&device), group);
    QCOMPARE(source.objects(), QList<QObject*>({&device}));
    QCOMPARE(group->childCount(), 2);
    QCOMPARE(group->child(QStringLiteral("capacity"))->typeId(), qpb::TypeId(qpb::Types::Int64));

    QVERIFY(model.setValue(group->path() + QStringLiteral("/label"), QStringLiteral("ssd")));
    QCOMPARE(device.m_label, QStringLiteral("ssd"));
    device.m_capacity = 7;
    source.refresh();
    QCOMPARE(group->child(QStringLiteral("capacity"))->value(), QVariant(qint64(7)));

    QVERIFY(source.removeObject(&device));
    QCOMPARE(source.groupOf(&device), nullptr);
}

QTEST_MAIN(tst_ApiCompat_1_2)
#include "v1_2.moc"
