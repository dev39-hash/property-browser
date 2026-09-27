// API compatibility test for qpb 1.3 (docs/SPEC.md §9.5, PLAN M7.5).
//
// Exercises every public API added in 1.3 the way client code would. Once
// 1.3.0 is released this file is FROZEN: never edit it. Every later 1.x
// release must compile it (deprecation warnings allowed) and pass it unchanged.

#include <qpb/qpb.h>

#include <QTest>

namespace {

class Meter : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("qpb:title", "label")
    Q_CLASSINFO("qpb:target", "live")
    Q_PROPERTY(QString label MEMBER m_label NOTIFY labelChanged)
    Q_PROPERTY(int target MEMBER m_target NOTIFY targetChanged)
    Q_PROPERTY(int level READ level NOTIFY levelChanged)

public:
    int level() const
    {
        return m_level;
    }
    void setLevel(int level)
    {
        m_level = level;
        emit levelChanged();
    }

    QString m_label = QStringLiteral("Main");
    int m_target = 0;
    int m_level = 0;

signals:
    void labelChanged();
    void targetChanged();
    void levelChanged();
};

class Untitled : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString caption MEMBER m_caption)

public:
    QString m_caption = QStringLiteral("Caption");
};

} // namespace

class tst_ApiCompat_1_3 : public QObject
{
    Q_OBJECT

private slots:
    void liveFlag();
    void sourceTitlesAndLiveValues();
};

void tst_ApiCompat_1_3::liveFlag()
{
    QCOMPARE(int(qpb::Property::Flag::Live), 0x8);
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    qpb::IntBuilder builder = root->addInt(QStringLiteral("n"), 1);
    qpb::IntBuilder& chained = builder.live();
    QCOMPARE(&chained, &builder);
    qpb::Property& n = builder;
    QVERIFY(n.isLive());
    n.setLive(false);
    QVERIFY(!n.isLive());
    n.setLive(true);
    QVERIFY(n.setValue(5));
    QVERIFY(!n.isModified());
    QVERIFY(root->resetToDefault());
    QCOMPARE(n.value(), QVariant(5));
    QVERIFY(qpb::serialization::toJson(*root).isEmpty());
}

void tst_ApiCompat_1_3::sourceTitlesAndLiveValues()
{
    qpb::PropertyModel model;
    qpb::QObjectPropertySource source(&model);
    source.setTitleProperty(QStringLiteral("caption"));
    QCOMPARE(source.titleProperty(), QStringLiteral("caption"));
    source.setLiveReadOnlyProperties(true);
    QVERIFY(source.liveReadOnlyProperties());

    Meter meter;
    meter.setObjectName(QStringLiteral("meter"));
    qpb::PropertyGroup* group = source.addObject(&meter);
    QCOMPARE(group->id(), QStringLiteral("meter"));
    QCOMPARE(group->displayName(), QStringLiteral("Main"));
    QVERIFY(group->child(QStringLiteral("target"))->isLive());
    QVERIFY(group->child(QStringLiteral("level"))->isLive());
    meter.setLevel(3);
    QCOMPARE(group->child(QStringLiteral("level"))->value(), QVariant(3));

    Untitled untitled;
    QCOMPARE(source.addObject(&untitled)->displayName(), QStringLiteral("Caption"));
}

QTEST_GUILESS_MAIN(tst_ApiCompat_1_3)
#include "v1_3.moc"
