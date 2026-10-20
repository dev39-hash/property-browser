// Translated texts of qpb::widgets and language changes at run time
// (docs/PLAN.md M12.4, M12.5, SPEC 4.10, D57).

#include <qpb/qpb.h>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QTest>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>
#include <QVBoxLayout>

#include "widgets/PathEdit_p.h"

using namespace qpb;

namespace {

// General
// |-- count   int
// |-- file    file path
// `-- dir     dir path
std::unique_ptr<PropertyGroup> createTree()
{
    auto root = PropertyGroup::create(QStringLiteral("root"));
    PropertyGroup& general = root->addGroup(QStringLiteral("General"));
    general.addInt(QStringLiteral("count"), 1);
    general.addFilePath(QStringLiteral("file"), QString());
    general.addDirPath(QStringLiteral("dir"), QString());
    return root;
}

// The texts of the context menu that widget opens at position.
QStringList menuTexts(QWidget* widget, const QPoint& position)
{
    QStringList texts;
    QTimer::singleShot(0, widget, [&texts] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu)
            return;
        for (const QAction* action : menu->actions())
            texts.append(action->text());
        menu->close();
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, position, widget->mapToGlobal(position));
    QApplication::sendEvent(widget, &event);
    return texts;
}

QString browseToolTip(QWidget* editor)
{
    auto* path = qobject_cast<detail::PathEdit*>(editor);
    return path ? path->browseButton()->toolTip() : QStringLiteral("<no path editor>");
}

} // namespace

class tst_WidgetTranslations : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void treeViewTexts();
    void formViewTexts();
    void formViewFollowsLanguageChanges();

private:
    void install(QLocale::Language language)
    {
        QCoreApplication::removeTranslator(&m_translator);
        QVERIFY(qpb::loadTranslation(m_translator, QLocale(language)));
        QVERIFY(QCoreApplication::installTranslator(&m_translator));
    }

    QTranslator m_translator;
};

void tst_WidgetTranslations::cleanup()
{
    QCoreApplication::removeTranslator(&m_translator);
}

void tst_WidgetTranslations::treeViewTexts()
{
    install(QLocale::Japanese);
    PropertyModel model(createTree());
    PropertyTreeView view;
    view.setModel(&model);
    view.resize(400, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QCOMPARE(view.header()->model()->headerData(0, Qt::Horizontal).toString(),
        QStringLiteral(u"プロパティ"));
    const QModelIndex group = model.indexOf(model.find(QStringLiteral("General")));
    const QModelIndex count = model.indexOf(model.find(QStringLiteral("General/count")));
    QCOMPARE(menuTexts(view.viewport(), view.visualRect(group).center()),
        QStringList(QStringLiteral(u"グループをリセット")));
    QCOMPARE(menuTexts(view.viewport(), view.visualRect(count).center()),
        QStringList(QStringLiteral(u"既定値にリセット")));

    // Editors are created when editing starts, in the current language.
    const QModelIndex dir
        = model.indexOf(model.find(QStringLiteral("General/dir"))).siblingAtColumn(1);
    view.setCurrentIndex(dir);
    view.edit(dir);
    QWidget* editor = view.indexWidget(dir);
    if (!editor)
        editor = view.viewport()->findChild<detail::PathEdit*>();
    QCOMPARE(browseToolTip(editor), QStringLiteral(u"ディレクトリを選択"));
}

void tst_WidgetTranslations::formViewTexts()
{
    install(QLocale::Vietnamese);
    PropertyModel model(createTree());
    PropertyFormView view;
    view.setModel(&model);
    view.resize(400, 300);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QLabel* label = nullptr;
    for (QLabel* candidate : view.findChildren<QLabel*>()) {
        if (candidate->buddy() == view.editor(QStringLiteral("General/count")))
            label = candidate;
    }
    QVERIFY(label);
    QCOMPARE(menuTexts(label, label->rect().center()),
        QStringList(QStringLiteral(u"Đặt lại về mặc định")));
    QToolButton* title = nullptr;
    for (QToolButton* candidate : view.findChildren<QToolButton*>()) {
        if (candidate->isCheckable() && candidate->text() == QStringLiteral("General"))
            title = candidate;
    }
    QVERIFY(title);
    QCOMPARE(menuTexts(title, title->rect().center()), QStringList(QStringLiteral(u"Đặt lại nhóm")));
    QCOMPARE(browseToolTip(view.editor(QStringLiteral("General/file"))), QStringLiteral(u"Chọn tệp"));
}

// The form view keeps its editors: their tool tips follow the language (D57).
void tst_WidgetTranslations::formViewFollowsLanguageChanges()
{
    PropertyModel model(createTree());
    PropertyFormView view;
    view.setModel(&model);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QWidget* file = view.editor(QStringLiteral("General/file"));
    QWidget* dir = view.editor(QStringLiteral("General/dir"));
    QCOMPARE(browseToolTip(file), QStringLiteral("Choose a file"));

    install(QLocale::Japanese); // widgets get LanguageChange as a posted event
    QTRY_COMPARE(browseToolTip(file), QStringLiteral(u"ファイルを選択"));
    QTRY_COMPARE(browseToolTip(dir), QStringLiteral(u"ディレクトリを選択"));

    install(QLocale::Vietnamese);
    QTRY_COMPARE(browseToolTip(file), QStringLiteral(u"Chọn tệp"));
    QTRY_COMPARE(browseToolTip(dir), QStringLiteral(u"Chọn thư mục"));

    QVERIFY(QCoreApplication::removeTranslator(&m_translator));
    QTRY_COMPARE(browseToolTip(file), QStringLiteral("Choose a file"));
    QCOMPARE(view.editor(QStringLiteral("General/file")), file); // the same editor
}

QTEST_MAIN(tst_WidgetTranslations)
#include "tst_widgettranslations.moc"
