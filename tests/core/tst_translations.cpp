// qpb's translations (see qpb/doc/guide/translations.md).

#include <qpb/qpbcore.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPoint>
#include <QSignalSpy>
#include <QTest>
#include <QTranslator>
#include <QXmlStreamReader>

#include <cmath>
#include <limits>

using namespace qpb;

namespace {

// One property per translated message of qpb::core.
std::unique_ptr<PropertyGroup> createTree()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    root->addString(QStringLiteral("name"), QString()).maxLength(3);
    root->addString(QStringLiteral("initial"), QString()).maxLength(1);
    root->addString(QStringLiteral("code"), QString()).regularExpression(QStringLiteral("[a-z]+"));
    root->addDouble(QStringLiteral("ratio"), 1.0);
    root->addEnum(QStringLiteral("mode"), {QStringLiteral("A"), QStringLiteral("B")}, 0);
    root->addFilePath(QStringLiteral("file"), QString()).mustExist();
    root->addDirPath(QStringLiteral("dir"), QString()).mustExist();
    root->addInt(QStringLiteral("count"), 0);
    return root;
}

QString missingPath()
{
    return QDir::temp().filePath(QStringLiteral("qpb-no-such-entry"));
}

// The message of validationFailed when model rejects value for path.
QString rejection(PropertyModel& model, const QString& path, const QVariant& value)
{
    QSignalSpy failed(&model, &PropertyModel::validationFailed);
    if (model.setValue(path, value) || failed.count() != 1)
        return QStringLiteral("<accepted>");
    return failed.first().at(2).toString();
}

// The messages of every case, in order.
QStringList messages(PropertyModel& model)
{
    return {
        rejection(model, QStringLiteral("name"), QStringLiteral("abcd")),
        rejection(model, QStringLiteral("initial"), QStringLiteral("ab")),
        rejection(model, QStringLiteral("code"), QStringLiteral("A1")),
        rejection(model, QStringLiteral("ratio"), std::numeric_limits<double>::quiet_NaN()),
        rejection(model, QStringLiteral("mode"), 7),
        rejection(model, QStringLiteral("file"), missingPath()),
        rejection(model, QStringLiteral("dir"), missingPath()),
        rejection(model, QStringLiteral("count"), QPoint(1, 2)),
    };
}

QStringList headers(const PropertyModel& model)
{
    return {model.headerData(0, Qt::Horizontal).toString(),
        model.headerData(1, Qt::Horizontal).toString()};
}

// A message of a .ts file: its translation forms (one, or one per plural form).
struct TsMessage
{
    QString context;
    QString source;
    bool numerus = false;
    bool unfinished = false;
    QStringList forms;
};

QList<TsMessage> readTs(const QString& fileName)
{
    QList<TsMessage> result;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return result;
    QXmlStreamReader xml(&file);
    QString context;
    TsMessage message;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement() && xml.name() == QLatin1String("message")) {
            result.append(message);
            continue;
        }
        if (!xml.isStartElement())
            continue;
        const auto name = xml.name();
        if (name == QLatin1String("name")) {
            context = xml.readElementText();
        } else if (name == QLatin1String("message")) {
            message = TsMessage();
            message.context = context;
            message.numerus = xml.attributes().value(QLatin1String("numerus")) == QLatin1String("yes");
        } else if (name == QLatin1String("source")) {
            message.source = xml.readElementText();
        } else if (name == QLatin1String("translation")) {
            message.unfinished = !xml.attributes().value(QLatin1String("type")).isEmpty();
            if (!message.numerus)
                message.forms.append(xml.readElementText());
        } else if (name == QLatin1String("numerusform")) {
            message.forms.append(xml.readElementText());
        }
    }
    return result;
}

} // namespace

class tst_Translations : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void englishWithoutTranslation();
    void loadTranslation_data();
    void loadTranslation();
    void translatedTexts_data();
    void translatedTexts();
    void englishPluralForms();
    void columnTitlesFollowLanguageChanges();
    void qmMatchesTs_data();
    void qmMatchesTs();

private:
    QTranslator m_translator;
};

void tst_Translations::initTestCase()
{
    qRegisterMetaType<Qt::Orientation>(); // QSignalSpy on headerDataChanged with Qt 5
}

void tst_Translations::cleanup()
{
    QCoreApplication::removeTranslator(&m_translator);
}

// Without qpb's translation the texts are those of 1.7, word for word.
void tst_Translations::englishWithoutTranslation()
{
    PropertyModel model(createTree());
    const QStringList expected = {
        QStringLiteral("At most 3 characters are allowed"),
        QStringLiteral("At most 1 characters are allowed"), // as in 1.7: no plural forms
        QStringLiteral("The value does not match the pattern [a-z]+"),
        QStringLiteral("The value is not a number"),
        QStringLiteral("\"7\" is not one of the options"),
        QStringLiteral("The file %1 does not exist").arg(missingPath()),
        QStringLiteral("The directory %1 does not exist").arg(missingPath()),
        QStringLiteral("Cannot convert the value to int"),
    };
    QCOMPARE(messages(model), expected);
    QCOMPARE(headers(model), QStringList({QStringLiteral("Property"), QStringLiteral("Value")}));
}

void tst_Translations::loadTranslation_data()
{
    QTest::addColumn<QLocale>("locale");
    QTest::addColumn<bool>("found");
    QTest::newRow("en_US") << QLocale(QLocale::English, QLocale::UnitedStates) << true;
    QTest::newRow("ja") << QLocale(QLocale::Japanese) << true;
    QTest::newRow("ja_JP") << QLocale(QLocale::Japanese, QLocale::Japan) << true;
    QTest::newRow("vi_VN") << QLocale(QLocale::Vietnamese, QLocale::Vietnam) << true;
    QTest::newRow("de_DE") << QLocale(QLocale::German, QLocale::Germany) << false;
    QTest::newRow("C") << QLocale::c() << false;
}

void tst_Translations::loadTranslation()
{
    QFETCH(QLocale, locale);
    QFETCH(bool, found);
    QTranslator translator;
    QCOMPARE(qpb::loadTranslation(translator, locale), found);
    QCOMPARE(translator.isEmpty(), !found);
}

void tst_Translations::translatedTexts_data()
{
    QTest::addColumn<QLocale>("locale");
    QTest::addColumn<QStringList>("expected");
    QTest::addColumn<QStringList>("titles");
    const QString missing = missingPath();
    QTest::newRow("ja") << QLocale(QLocale::Japanese, QLocale::Japan)
                        << QStringList({
                               QStringLiteral(u"3 文字まで入力できます"),
                               QStringLiteral(u"1 文字まで入力できます"),
                               QStringLiteral(u"値がパターン [a-z]+ に一致しません"),
                               QStringLiteral(u"値が数値ではありません"),
                               QStringLiteral(u"「7」は選択肢にありません"),
                               QStringLiteral(u"ファイル %1 は存在しません").arg(missing),
                               QStringLiteral(u"ディレクトリ %1 は存在しません").arg(missing),
                               QStringLiteral(u"値を int に変換できません"),
                           })
                        << QStringList({QStringLiteral(u"プロパティ"), QStringLiteral(u"値")});
    QTest::newRow("vi") << QLocale(QLocale::Vietnamese, QLocale::Vietnam)
                        << QStringList({
                               QStringLiteral(u"Chỉ cho phép tối đa 3 ký tự"),
                               QStringLiteral(u"Chỉ cho phép tối đa 1 ký tự"),
                               QStringLiteral(u"Giá trị không khớp với mẫu [a-z]+"),
                               QStringLiteral(u"Giá trị không phải là số"),
                               QStringLiteral(u"“7” không nằm trong các lựa chọn"),
                               QStringLiteral(u"Tệp %1 không tồn tại").arg(missing),
                               QStringLiteral(u"Thư mục %1 không tồn tại").arg(missing),
                               QStringLiteral(u"Không thể chuyển giá trị sang kiểu int"),
                           })
                        << QStringList({QStringLiteral(u"Thuộc tính"), QStringLiteral(u"Giá trị")});
}

void tst_Translations::translatedTexts()
{
    QFETCH(QLocale, locale);
    QFETCH(QStringList, expected);
    QFETCH(QStringList, titles);
    QVERIFY(qpb::loadTranslation(m_translator, locale));
    QVERIFY(QCoreApplication::installTranslator(&m_translator));
    PropertyModel model(createTree());
    QCOMPARE(messages(model), expected);
    QCOMPARE(headers(model), titles);
}

void tst_Translations::englishPluralForms()
{
    QVERIFY(qpb::loadTranslation(m_translator, QLocale(QLocale::English, QLocale::UnitedKingdom)));
    QVERIFY(QCoreApplication::installTranslator(&m_translator));
    PropertyModel model(createTree());
    QCOMPARE(rejection(model, QStringLiteral("initial"), QStringLiteral("ab")),
        QStringLiteral("At most 1 character is allowed"));
    QCOMPARE(rejection(model, QStringLiteral("name"), QStringLiteral("abcd")),
        QStringLiteral("At most 3 characters are allowed"));
    QCOMPARE(headers(model), QStringList({QStringLiteral("Property"), QStringLiteral("Value")}));
}

// Installing or removing a translator makes views read the titles again (qpb
// updates only what it shows by itself; texts given by the application are
// already translated).
void tst_Translations::columnTitlesFollowLanguageChanges()
{
    PropertyModel model(createTree());
    PropertyModel other; // every model, also one without a tree
    QSignalSpy changed(&model, &PropertyModel::headerDataChanged);
    QSignalSpy otherChanged(&other, &PropertyModel::headerDataChanged);

    QVERIFY(qpb::loadTranslation(m_translator, QLocale(QLocale::Japanese)));
    QVERIFY(QCoreApplication::installTranslator(&m_translator));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().at(0).value<Qt::Orientation>(), Qt::Horizontal);
    QCOMPARE(changed.first().at(1).toInt(), 0);
    QCOMPARE(changed.first().at(2).toInt(), 1);
    QCOMPARE(otherChanged.count(), 1);
    QCOMPARE(headers(model).first(), QStringLiteral(u"プロパティ"));

    QVERIFY(QCoreApplication::removeTranslator(&m_translator));
    QCOMPARE(changed.count(), 2);
    QCOMPARE(headers(model).first(), QStringLiteral("Property"));

    // Switching: the application loads another locale and installs it again.
    QVERIFY(qpb::loadTranslation(m_translator, QLocale(QLocale::Vietnamese)));
    QVERIFY(QCoreApplication::installTranslator(&m_translator));
    QCOMPARE(changed.count(), 3);
    QCOMPARE(headers(model).first(), QStringLiteral(u"Thuộc tính"));
}

void tst_Translations::qmMatchesTs_data()
{
    QTest::addColumn<QString>("language");
    QTest::newRow("en") << QStringLiteral("en");
    QTest::newRow("ja") << QStringLiteral("ja");
    QTest::newRow("vi") << QStringLiteral("vi");
}

// The embedded .qm files are generated from the committed .ts files (a consumer
// may not have LinguistTools, so nothing is built at configure time):
// every translation of the .ts file is in the .qm file, none is unfinished.
void tst_Translations::qmMatchesTs()
{
    QFETCH(QString, language);
    const QList<TsMessage> ts
        = readTs(QStringLiteral(QPB_TRANSLATIONS_DIR "/qpb_%1.ts").arg(language));
    QVERIFY(!ts.isEmpty());
    QTranslator translator;
    QVERIFY(qpb::loadTranslation(translator, QLocale(language)));
    for (const TsMessage& message : ts) {
        const QByteArray context = message.context.toUtf8();
        const QByteArray source = message.source.toUtf8();
        QVERIFY2(!message.unfinished, source.constData());
        if (!message.numerus) {
            QCOMPARE(translator.translate(context, source), message.forms.value(0));
            continue;
        }
        // English has two plural forms (1, other); Japanese and Vietnamese one.
        QCOMPARE(translator.translate(context, source, nullptr, 1), message.forms.value(0));
        if (message.forms.size() > 1)
            QCOMPARE(translator.translate(context, source, nullptr, 2), message.forms.value(1));
    }
}

QTEST_GUILESS_MAIN(tst_Translations)
#include "tst_translations.moc"
