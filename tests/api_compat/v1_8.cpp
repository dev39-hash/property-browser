// API compatibility test for qpb 1.8: client code written against 1.8 must keep
// building and behaving the same in every later 1.x; see
// docs/design/public-headers.md.
//
// Exercises every public API added in 1.8 the way client code would, and the
// translatable texts that 1.8 makes stable (applications translate qpb or
// override a wording through their contexts and source texts, so changing one
// would silently drop their translation). Builds with Qt 6 and Qt 5.15.
// Once 1.8.0 is released this file is FROZEN: never edit it. Every later 1.x
// release must compile it (deprecation warnings allowed) and pass it unchanged.

#include <qpb/qpb.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>
#include <QTranslator>

#include <type_traits>

class tst_ApiCompat_1_8 : public QObject
{
    Q_OBJECT

private slots:
    void loadTranslation();
    void stableTexts();
    void installedTranslation();
};

void tst_ApiCompat_1_8::loadTranslation()
{
    static_assert(std::is_same_v<decltype(&qpb::loadTranslation),
        bool (*)(QTranslator&, const QLocale&)>);
    QTranslator translator;
    QVERIFY(qpb::loadTranslation(translator, QLocale(QLocale::English, QLocale::UnitedStates)));
    QVERIFY(qpb::loadTranslation(translator, QLocale(QLocale::Japanese, QLocale::Japan)));
    QVERIFY(qpb::loadTranslation(translator, QLocale(QLocale::Vietnamese, QLocale::Vietnam)));
    QVERIFY(!qpb::loadTranslation(translator, QLocale::c()));

    const QLocale saved;
    QLocale::setDefault(QLocale(QLocale::Japanese));
    QVERIFY(qpb::loadTranslation(translator)); // the default argument: QLocale()
    QLocale::setDefault(saved);
}

// Applications translate these keys themselves (further languages, other wording).
void tst_ApiCompat_1_8::stableTexts()
{
    struct Text
    {
        const char* context;
        const char* source;
        bool plural;
    };
    const Text texts[] = {
        {"qpb::PropertyModel", "Property", false},
        {"qpb::PropertyModel", "Value", false},
        {"qpb::PropertyTreeView", "Reset to default", false},
        {"qpb::PropertyTreeView", "Reset group", false},
        {"qpb::PropertyFormView", "Reset to default", false},
        {"qpb::PropertyFormView", "Reset group", false},
        {"qpb::detail::PathEdit", "Choose a file", false},
        {"qpb::detail::PathEdit", "Choose a directory", false},
        {"qpb::TypeRegistry", "The value is not a number", false},
        {"qpb::TypeRegistry", "At most %n characters are allowed", true},
        {"qpb::TypeRegistry", "The value does not match the pattern %1", false},
        {"qpb::TypeRegistry", "\"%1\" is not one of the options", false},
        {"qpb::TypeRegistry", "The file %1 does not exist", false},
        {"qpb::TypeRegistry", "The directory %1 does not exist", false},
        {"qpb::Property", "Cannot convert the value to %1", false},
    };
    for (const QLocale::Language language : {QLocale::Japanese, QLocale::Vietnamese}) {
        QTranslator translator;
        QVERIFY(qpb::loadTranslation(translator, QLocale(language)));
        for (const Text& text : texts) {
            QVERIFY2(!translator.translate(text.context, text.source, nullptr, text.plural ? 2 : -1)
                          .isEmpty(),
                text.source);
        }
    }
    QTranslator english;
    QVERIFY(qpb::loadTranslation(english, QLocale(QLocale::English)));
    QCOMPARE(english.translate("qpb::TypeRegistry", "At most %n characters are allowed", nullptr, 1),
        QStringLiteral("At most %n character is allowed"));
}

void tst_ApiCompat_1_8::installedTranslation()
{
    auto root = qpb::PropertyGroup::create(QStringLiteral("root"));
    root->addString(QStringLiteral("name"), QString()).maxLength(1);
    qpb::PropertyModel model(std::move(root));
    QSignalSpy titles(&model, &qpb::PropertyModel::headerDataChanged);
    QSignalSpy failed(&model, &qpb::PropertyModel::validationFailed);

    QTranslator translator;
    QVERIFY(qpb::loadTranslation(translator, QLocale(QLocale::English)));
    QVERIFY(QCoreApplication::installTranslator(&translator));
    QCOMPARE(titles.count(), 1);
    QVERIFY(!model.setValue(QStringLiteral("name"), QStringLiteral("ab")));
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(2).toString(), QStringLiteral("At most 1 character is allowed"));
    QVERIFY(QCoreApplication::removeTranslator(&translator));
    QCOMPARE(titles.count(), 2);
}

QTEST_GUILESS_MAIN(tst_ApiCompat_1_8)
#include "v1_8.moc"
