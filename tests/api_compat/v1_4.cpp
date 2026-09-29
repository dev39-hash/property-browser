// API compatibility test for qpb 1.4 (docs/SPEC.md §9.5, PLAN M8.5).
//
// Exercises every public API added in 1.4 the way client code would. Once
// 1.4.0 is released this file is FROZEN: never edit it. Every later 1.x
// release must compile it (deprecation warnings allowed) and pass it unchanged.

#include <qpb/qpb.h>

#include <QTest>

#include <type_traits>

namespace {

class Pump : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("qpb:rate", "enabledWhen=running")
    Q_CLASSINFO("qpb:note", "visibleWhen=running")
    Q_PROPERTY(bool running MEMBER m_running NOTIFY changed)
    Q_PROPERTY(int rate MEMBER m_rate NOTIFY changed)
    Q_PROPERTY(QString note MEMBER m_note NOTIFY changed)

public:
    bool m_running = false;
    int m_rate = 1;
    QString m_note;

signals:
    void changed();
};

} // namespace

class tst_ApiCompat_1_4 : public QObject
{
    Q_OBJECT

private slots:
    void conditions();
    void builders();
    void onValueChanged();
    void sourceMetadata();
};

void tst_ApiCompat_1_4::conditions()
{
    static_assert(std::is_same_v<qpb::Property::Condition, std::function<bool(const QVariant&)>>);
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addBool(QStringLiteral("on"), false);
    root->addInt(QStringLiteral("mode"), 0);
    qpb::Property& a = root->addInt(QStringLiteral("a"), 0);
    qpb::Property& b = root->addInt(QStringLiteral("b"), 0);
    qpb::Property& c = root->addInt(QStringLiteral("c"), 0);
    a.setEnabledWhen(QStringLiteral("on"));
    b.setEnabledWhen(QStringLiteral("mode"), 0);
    b.setEnabledWhen(QStringLiteral("mode"), QVariant(0));
    c.setEnabledWhen(QStringLiteral("mode"), [](const QVariant& v) { return v.toInt() > 1; });
    a.setVisibleWhen(QStringLiteral("on"));
    b.setVisibleWhen(QStringLiteral("mode"), 0);
    b.setVisibleWhen(QStringLiteral("mode"), QVariant(0));
    c.setVisibleWhen(QStringLiteral("mode"), [](const QVariant& v) { return v.toInt() >= 0; });

    qpb::PropertyModel model(std::move(root));
    QVERIFY(!model.find(QStringLiteral("a"))->isEnabled());
    QVERIFY(model.find(QStringLiteral("b"))->isEnabled());
    QVERIFY(!model.find(QStringLiteral("c"))->isEnabled());
    QVERIFY(model.setValue(QStringLiteral("on"), true));
    QVERIFY(model.find(QStringLiteral("a"))->isVisible());
    model.find(QStringLiteral("a"))->clearEnabledWhen();
    model.find(QStringLiteral("a"))->clearVisibleWhen();
}

void tst_ApiCompat_1_4::builders()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("mode"), 0);
    qpb::IntBuilder builder = root->addInt(QStringLiteral("n"), 1);
    qpb::IntBuilder& chained
        = builder.enabledWhen(QStringLiteral("mode"))
              .enabledWhen(QStringLiteral("mode"), 1)
              .enabledWhen(QStringLiteral("mode"), QVariant(1))
              .enabledWhen(QStringLiteral("mode"), [](const QVariant& v) { return v.toInt() == 1; })
              .visibleWhen(QStringLiteral("mode"))
              .visibleWhen(QStringLiteral("mode"), 0)
              .visibleWhen(QStringLiteral("mode"), QVariant(0))
              .visibleWhen(
                  QStringLiteral("mode"), [](const QVariant& v) { return v.toInt() == 0; });
    QCOMPARE(&chained, &builder);
}

void tst_ApiCompat_1_4::onValueChanged()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addGroup(QStringLiteral("g")).addInt(QStringLiteral("x"), 0);
    qpb::PropertyModel model(std::move(root));
    int value = 0;
    QStringList paths;
    const QMetaObject::Connection single = model.onValueChanged(
        QStringLiteral("g/x"), this, [&value](const QVariant& v) { value = v.toInt(); });
    const QMetaObject::Connection group = model.onValueChanged(QStringLiteral("g"), nullptr,
        [&paths](const QString& path, const QVariant&) { paths << path; });
    QVERIFY(single);
    QVERIFY(group);
    QVERIFY(model.setValue(QStringLiteral("g/x"), 4));
    QCOMPARE(value, 4);
    QCOMPARE(paths, QStringList({"g/x"}));
    QVERIFY(QObject::disconnect(single));
    QVERIFY(QObject::disconnect(group));
}

void tst_ApiCompat_1_4::sourceMetadata()
{
    qpb::PropertyModel model;
    qpb::QObjectPropertySource source(&model);
    Pump pump;
    pump.setObjectName(QStringLiteral("pump"));
    source.addObject(&pump);
    QVERIFY(!model.find(QStringLiteral("pump/rate"))->isEnabled());
    QVERIFY(!model.find(QStringLiteral("pump/note"))->isVisible());
    QVERIFY(model.setValue(QStringLiteral("pump/running"), true));
    QVERIFY(model.find(QStringLiteral("pump/rate"))->isEnabled());
    QVERIFY(model.find(QStringLiteral("pump/note"))->isVisible());
}

QTEST_GUILESS_MAIN(tst_ApiCompat_1_4)
#include "v1_4.moc"
