#include <qpb/qpbglobal.h>

#include <QFile>
#include <QTest>

class tst_Version : public QObject
{
    Q_OBJECT

private slots:
    void headerMatchesVersionFile();
    void componentsMatchString();
    void runtimeMatchesHeader();
    void versionCheckOrdering();
};

void tst_Version::headerMatchesVersionFile()
{
    QFile file(QStringLiteral(QPB_TEST_VERSION_FILE));
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.errorString()));
    const QString fromFile = QString::fromUtf8(file.readLine()).trimmed();
    QCOMPARE(QStringLiteral(QPB_VERSION_STR), fromFile);
}

void tst_Version::componentsMatchString()
{
    const QString fromComponents = QStringLiteral("%1.%2.%3")
                                       .arg(QPB_VERSION_MAJOR)
                                       .arg(QPB_VERSION_MINOR)
                                       .arg(QPB_VERSION_PATCH);
    // QPB_VERSION_STR may carry a pre-release suffix ("1.0.0-rc1").
    const QString full = QStringLiteral(QPB_VERSION_STR);
    QVERIFY2(full == fromComponents || full.startsWith(fromComponents + QLatin1Char('-')),
        qPrintable(full));
    QCOMPARE(
        QPB_VERSION, QPB_VERSION_CHECK(QPB_VERSION_MAJOR, QPB_VERSION_MINOR, QPB_VERSION_PATCH));
}

void tst_Version::runtimeMatchesHeader()
{
    QCOMPARE(QString::fromLatin1(qpb::version()), QStringLiteral(QPB_VERSION_STR));
}

void tst_Version::versionCheckOrdering()
{
    static_assert(QPB_VERSION_CHECK(1, 0, 0) > QPB_VERSION_CHECK(0, 255, 255));
    static_assert(QPB_VERSION_CHECK(1, 2, 0) > QPB_VERSION_CHECK(1, 1, 9));
    static_assert(QPB_VERSION_CHECK(1, 2, 3) == 0x010203);
    QVERIFY(QPB_VERSION >= QPB_VERSION_CHECK(0, 0, 1));
}

QTEST_APPLESS_MAIN(tst_Version)
#include "tst_version.moc"
