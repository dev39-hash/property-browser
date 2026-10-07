// API compatibility test for the Qt 5 configuration of qpb 1.7 (docs/SPEC.md
// §9.5, PLAN M11, D53).
//
// Qt 5.15 is supported since 1.7. Most of the API is the same with Qt 5 and
// Qt 6, and the files of earlier releases (v1_1 ... v1_6) build with both.
// This file covers what differs with Qt 5: the Types / Attr constants are
// QLatin1String and TypeHandler::storageType is a type id. Once 1.7.0 is
// released this file is FROZEN: never edit it.

#include <qpb/qpb.h>

#include <QApplication>
#include <QColor>
#include <QTest>

#include <type_traits>

class tst_ApiCompat_Qt5_1_7 : public QObject
{
    Q_OBJECT

private slots:
    void constants();
    void storageType();
};

void tst_ApiCompat_Qt5_1_7::constants()
{
    static_assert(std::is_same_v<decltype(qpb::Types::Int), const QLatin1String>);
    static_assert(std::is_same_v<decltype(qpb::Attr::Minimum), const QLatin1String>);
    constexpr QLatin1String id = qpb::Types::Bool;
    static_assert(id.size() == 4);
    const qpb::TypeId type = qpb::Types::Int; // converts to QString
    QCOMPARE(type, QStringLiteral("int"));
    QCOMPARE(QString(qpb::Attr::MaxLength), QStringLiteral("maxLength"));

    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    qpb::Property& n = root->add(qpb::Types::Int, QStringLiteral("n"), 1);
    n.setAttribute(qpb::Attr::Maximum, 10);
    QVERIFY(n.setValue(20));
    QCOMPARE(n.value(), QVariant(10));
    QCOMPARE(n.attribute(qpb::Attr::Maximum), QVariant(10));
}

void tst_ApiCompat_Qt5_1_7::storageType()
{
    static_assert(std::is_same_v<decltype(qpb::TypeHandler::storageType), int>);
    qpb::TypeHandler handler;
    QCOMPARE(handler.storageType, int(QMetaType::UnknownType));
    handler.storageType = qMetaTypeId<QColor>();
    handler.displayText = [](const QVariant& value, const qpb::Property&) {
        return value.value<QColor>().name();
    };
    const qpb::TypeId id = QStringLiteral("compat.qt5.color");
    QVERIFY(qpb::TypeRegistry::global().registerType(id, handler));
    QCOMPARE(qpb::TypeRegistry::global().handler(id)->storageType, qMetaTypeId<QColor>());

    const qpb::TypeId typed = QStringLiteral("compat.qt5.point");
    QVERIFY(qpb::TypeRegistry::global().registerType<QPoint>(typed, qpb::TypeHandler()));
    QCOMPARE(qpb::TypeRegistry::global().handler(typed)->storageType, qMetaTypeId<QPoint>());

    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    qpb::Property& tint = root->add(id, QStringLiteral("tint"), QColor(Qt::red));
    QVERIFY(tint.setValue(QStringLiteral("#00ff00"))); // converted to QColor
    QCOMPARE(tint.value().userType(), qMetaTypeId<QColor>());
    QCOMPARE(tint.value().value<QColor>(), QColor(Qt::green));
}

QTEST_MAIN(tst_ApiCompat_Qt5_1_7)
#include "qt5_v1_7.moc"
