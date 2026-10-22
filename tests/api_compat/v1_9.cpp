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

QTEST_MAIN(tst_ApiCompat_1_9)
#include "v1_9.moc"
