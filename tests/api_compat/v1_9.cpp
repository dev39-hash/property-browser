// API compatibility test for qpb 1.9: client code written against 1.9 must keep
// building and behaving the same in every later 1.x; see
// docs/design/public-headers.md.
//
// Exercises every public API added in 1.9 the way client code would. Builds
// with Qt 6 and Qt 5.15. Once 1.9.0 is released this file is FROZEN: never
// edit it. Every later 1.x release must compile it (deprecation warnings
// allowed) and pass it unchanged.

#include <qpb/qpb.h>

#include <QApplication>
#include <QIdentityProxyModel>
#include <QLabel>
#include <QTest>

#include <type_traits>

namespace {

// An application's layer over a PropertyModel that colours the name cell of
// one property.
class ColouringProxy : public QIdentityProxyModel
{
public:
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (role == Qt::ForegroundRole && index.column() == 0
            && QIdentityProxyModel::data(index, qpb::PropertyModel::PathRole).toString()
                == QLatin1String("n"))
            return QVariant::fromValue(QBrush(QColor(Qt::red)));
        return QIdentityProxyModel::data(index, role);
    }
};

QLabel* labelFor(qpb::PropertyFormView& view, const QString& path)
{
    for (QLabel* candidate : view.findChildren<QLabel*>()) {
        if (candidate->buddy() == view.editor(path))
            return candidate;
    }
    return nullptr;
}

} // namespace

class tst_ApiCompat_1_9 : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void showModelForeground();
    void showModelForegroundFromStyleSheet();
    void userValueChanged();
    void onUserValueChanged();
    void normalizeValues();
    void makeValuesDefault();
};

void tst_ApiCompat_1_9::cleanup()
{
    qApp->setStyleSheet(QString());
}

void tst_ApiCompat_1_9::showModelForeground()
{
    static_assert(std::is_same_v<decltype(&qpb::PropertyFormView::showModelForeground),
        bool (qpb::PropertyFormView::*)() const>);
    static_assert(std::is_same_v<decltype(&qpb::PropertyFormView::setShowModelForeground),
        void (qpb::PropertyFormView::*)(bool)>);

    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("n"), 1);
    root->addInt(QStringLiteral("m"), 2);
    qpb::PropertyModel model(std::move(root));
    ColouringProxy proxy;
    proxy.setSourceModel(&model);

    qpb::PropertyFormView view;
    view.setModel(&proxy);
    const qpb::PropertyFormView& constView = view;
    QVERIFY(!constView.showModelForeground());
    QCOMPARE(view.property("showModelForeground"), QVariant(false));

    QLabel* coloured = labelFor(view, QStringLiteral("n"));
    QLabel* plain = labelFor(view, QStringLiteral("m"));
    QVERIFY(coloured && plain);
    const QColor normal = plain->palette().color(QPalette::WindowText);
    QVERIFY(normal != QColor(Qt::red));
    QCOMPARE(coloured->palette().color(QPalette::WindowText), normal);

    view.setShowModelForeground(true);
    QVERIFY(constView.showModelForeground());
    QCOMPARE(view.property("showModelForeground"), QVariant(true));
    QCOMPARE(coloured->palette().color(QPalette::WindowText), QColor(Qt::red));
    QCOMPARE(plain->palette().color(QPalette::WindowText), normal);

    QVERIFY(view.setProperty("showModelForeground", false));
    QVERIFY(!view.showModelForeground());
    QCOMPARE(coloured->palette().color(QPalette::WindowText), normal);
}

void tst_ApiCompat_1_9::showModelForegroundFromStyleSheet()
{
    qApp->setStyleSheet(
        QStringLiteral("qpb--PropertyFormView { qproperty-showModelForeground: true; }"));
    qpb::PropertyFormView view;
    view.ensurePolished();
    QVERIFY(view.showModelForeground());
}

void tst_ApiCompat_1_9::userValueChanged()
{
    static_assert(std::is_same_v<decltype(&qpb::PropertyModel::userValueChanged),
        void (qpb::PropertyModel::*)(const QString&, const QVariant&, const QVariant&)>);

    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("n"), 1);
    qpb::PropertyModel model(std::move(root));

    QStringList events;
    QObject::connect(&model, &qpb::PropertyModel::valueChanged, &model,
        [&events](const QString& path, const QVariant& value, const QVariant& old) {
            events << QStringLiteral("value:%1:%2:%3")
                          .arg(path)
                          .arg(value.toInt())
                          .arg(old.toInt());
        });
    QObject::connect(&model, &qpb::PropertyModel::userValueChanged, &model,
        [&events](const QString& path, const QVariant& value, const QVariant& old) {
            events << QStringLiteral("user:%1:%2:%3")
                          .arg(path)
                          .arg(value.toInt())
                          .arg(old.toInt());
        });

    // The user (setData) is reported after the general signal; application
    // writes are not reported as user edits.
    QVERIFY(model.setData(model.indexOf(model.find(QStringLiteral("n")), 1), 5));
    QCOMPARE(events,
        (QStringList {QStringLiteral("value:n:5:1"), QStringLiteral("user:n:5:1")}));
    events.clear();
    QVERIFY(model.setValue(QStringLiteral("n"), 6));
    QCOMPARE(events, (QStringList {QStringLiteral("value:n:6:5")}));
}

void tst_ApiCompat_1_9::onUserValueChanged()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    qpb::PropertyGroup& group = root->addGroup(QStringLiteral("g"));
    group.addInt(QStringLiteral("a"), 1);
    group.addInt(QStringLiteral("b"), 2);
    qpb::PropertyModel model(std::move(root));

    QList<int> single;
    QStringList paths;
    QMetaObject::Connection one = model.onUserValueChanged(QStringLiteral("g/a"), nullptr,
        [&single](const QVariant& value) { single << value.toInt(); });
    QMetaObject::Connection all = model.onUserValueChanged(QStringLiteral("g"), nullptr,
        [&paths](const QString& path, const QVariant&) { paths << path; });
    QVERIFY(one);
    QVERIFY(all);

    QVERIFY(model.setValue(QStringLiteral("g/a"), 10));
    QVERIFY(single.isEmpty());
    QVERIFY(paths.isEmpty());

    QVERIFY(model.setData(model.indexOf(model.find(QStringLiteral("g/a")), 1), 11));
    QVERIFY(model.setData(model.indexOf(model.find(QStringLiteral("g/b")), 1), 12));
    QCOMPARE(single, (QList<int> {11}));
    QCOMPARE(paths, (QStringList {QStringLiteral("g/a"), QStringLiteral("g/b")}));

    QVERIFY(QObject::disconnect(one));
    QVERIFY(model.setData(model.indexOf(model.find(QStringLiteral("g/a")), 1), 13));
    QCOMPARE(single, (QList<int> {11}));
    QVERIFY(!model.onUserValueChanged(
        QStringLiteral("g"), nullptr, std::function<void(const QVariant&)>()));
}

void tst_ApiCompat_1_9::normalizeValues()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("n"), 150);
    qpb::PropertyGroup& group = root->addGroup(QStringLiteral("g"));
    group.addInt(QStringLiteral("m"), 70);
    qpb::PropertyModel model(std::move(root));

    // A range set afterwards does not move a value by itself.
    model.find(QStringLiteral("n"))->setAttribute(qpb::Attr::Maximum, 100);
    model.find(QStringLiteral("g/m"))->setAttribute(qpb::Attr::Maximum, 50);
    QCOMPARE(model.find(QStringLiteral("n"))->value().toInt(), 150);
    QCOMPARE(model.find(QStringLiteral("g/m"))->value().toInt(), 70);

    QVERIFY(model.find(QStringLiteral("n"))->normalizeValue());
    QCOMPARE(model.find(QStringLiteral("n"))->value().toInt(), 100);
    QVERIFY(model.find(QStringLiteral("g"))->normalizeValue());
    QCOMPARE(model.find(QStringLiteral("g/m"))->value().toInt(), 50);

    model.find(QStringLiteral("n"))->setAttribute(qpb::Attr::Maximum, 20);
    QVERIFY(model.normalizeAllValues());
    QCOMPARE(model.find(QStringLiteral("n"))->value().toInt(), 20);
}

void tst_ApiCompat_1_9::makeValuesDefault()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addInt(QStringLiteral("n"), 1);
    qpb::PropertyGroup& group = root->addGroup(QStringLiteral("g"));
    group.addInt(QStringLiteral("m"), 2);
    group.addInt(QStringLiteral("k"), 3);
    qpb::PropertyModel model(std::move(root));

    qpb::Property* n = model.find(QStringLiteral("n"));
    qpb::Property* m = model.find(QStringLiteral("g/m"));
    qpb::Property* k = model.find(QStringLiteral("g/k"));
    QVERIFY(n->setValue(10));
    QVERIFY(m->setValue(20));
    QVERIFY(k->setValue(30));
    QVERIFY(n->isModified() && m->isModified() && k->isModified());

    n->makeValueDefault();
    QVERIFY(!n->isModified());
    QCOMPARE(n->defaultValue().toInt(), 10);
    model.find(QStringLiteral("g"))->makeValueDefault();
    QVERIFY(!m->isModified());
    QVERIFY(!k->isModified());

    QVERIFY(n->setValue(11));
    QVERIFY(m->setValue(21));
    model.makeAllValuesDefault();
    QVERIFY(!n->isModified());
    QVERIFY(!m->isModified());
    QCOMPARE(m->defaultValue().toInt(), 21);
}

QTEST_MAIN(tst_ApiCompat_1_9)
#include "v1_9.moc"
