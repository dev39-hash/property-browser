// Drives the RC trial application through its UI (docs/PLAN.md RC.1).

#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

#include "InspectorPage.h"
#include "MainWindow.h"
#include "PluginsPage.h"
#include "SettingsPage.h"

namespace {

template <class Editor>
Editor* openEditor(qpb::PropertyTreeView* view, const QModelIndex& valueIndex)
{
    view->setFocus();
    view->setCurrentIndex(valueIndex);
    for (Editor* e : view->findChildren<Editor*>()) {
        if (e->isVisible())
            return e;
    }
    return nullptr;
}

QModelIndex valueIndex(qpb::PropertyModel& model, const QString& path)
{
    return model.indexOf(model.find(path), qpb::PropertyModel::ValueColumn);
}

} // namespace

class tst_Trial : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void inspectorEditsWriteBackToScene();
    void inspectorSwitchesObjects();
    void settingsLoadStoredValues();
    void settingsDependencyAndSave();
    void settingsSaveUpdatesReadOnlyStatus();
    void settingsResetAll();
    void pluginsRuntimeChangesAndSearch();
    void screenshots();

private:
    QTemporaryDir m_dir;
    std::unique_ptr<MainWindow> m_window;
    QString settingsFile() const
    {
        return m_dir.filePath("settings.ini");
    }
    void createWindow()
    {
        m_window = std::make_unique<MainWindow>(settingsFile());
        m_window->show();
        QVERIFY(QTest::qWaitForWindowActive(m_window.get()));
    }
};

void tst_Trial::init()
{
    QVERIFY(m_dir.isValid());
    QFile::remove(settingsFile());
    createWindow();
}

void tst_Trial::cleanup()
{
    m_window.reset();
}

void tst_Trial::inspectorEditsWriteBackToScene()
{
    InspectorPage* page = m_window->inspector();
    page->objectList()->setCurrentRow(1);

    auto* fov = openEditor<QSpinBox>(page->view(), valueIndex(page->model(), "Camera/fov"));
    QVERIFY(fov);
    fov->setValue(75);
    QTest::keyClick(fov, Qt::Key_Return);
    QTRY_COMPARE(page->scene()[1].fov, 75);

    auto* name = openEditor<QLineEdit>(page->view(), valueIndex(page->model(), "name"));
    QVERIFY(name);
    name->setText("Rim light");
    QTest::keyClick(name, Qt::Key_Tab); // commits and moves on
    QTRY_COMPARE(page->scene()[1].name, QString("Rim light"));
    QCOMPARE(page->objectList()->item(1)->text(), QString("Rim light"));

    QVERIFY(page->model().setValue("Render/tint", QColor(Qt::red)));
    QCOMPARE(page->scene()[1].tint, QColor(Qt::red));
    QCOMPARE(page->scene()[0].fov, 60); // other objects untouched
}

void tst_Trial::inspectorSwitchesObjects()
{
    InspectorPage* page = m_window->inspector();
    page->objectList()->setCurrentRow(0);
    QVERIFY(page->model().setValue("Transform/x", 12.5));
    page->objectList()->setCurrentRow(2);
    QCOMPARE(page->model().find("name")->value().toString(), QString("Fill light"));
    QCOMPARE(page->model().find("Transform/x")->value().toDouble(), 0.0);
    page->objectList()->setCurrentRow(0);
    QCOMPARE(page->model().find("Transform/x")->value().toDouble(), 12.5);
    QVERIFY(page->view()->isExpanded(page->model().indexOf(page->model().find("Transform"))));
}

void tst_Trial::settingsLoadStoredValues()
{
    m_window.reset();
    {
        QSettings stored(settingsFile(), QSettings::IniFormat);
        stored.setValue("General/language", "vi");
        stored.setValue("General/autosave", false);
        stored.setValue("General/autosaveMinutes", 30);
    }
    createWindow();
    qpb::PropertyModel& model = m_window->settingsPage()->model();
    QCOMPARE(model.find("General/language")->value().toString(), QString("vi"));
    QCOMPARE(model.find("General/autosaveMinutes")->value().toInt(), 30);
    QVERIFY(!model.find("General/autosaveMinutes")->isEnabled());
    QVERIFY(model.find("General/language")->isModified());
}

void tst_Trial::settingsDependencyAndSave()
{
    SettingsPage* page = m_window->settingsPage();
    m_window->tabs()->setCurrentWidget(page);
    const QModelIndex autosave = valueIndex(page->model(), "General/autosave");
    QTest::mouseClick(
        page->view()->viewport(), Qt::LeftButton, {}, page->view()->visualRect(autosave).center());
    QCOMPARE(page->model().find("General/autosave")->value().toBool(), false);
    QVERIFY(!page->model().find("General/autosaveMinutes")->isEnabled());

    page->save();
    QSettings stored(settingsFile(), QSettings::IniFormat);
    QCOMPARE(stored.value("General/autosave").toBool(), false);
    QVERIFY(!stored.contains("About/version")); // read-only entries are not saved
}

// The application updates a read-only property (the user cannot edit it).
void tst_Trial::settingsSaveUpdatesReadOnlyStatus()
{
    SettingsPage* page = m_window->settingsPage();
    page->save();
    QVERIFY(page->model().find("About/lastSaved")->value().toString() != QString("never"));
}

void tst_Trial::settingsResetAll()
{
    qpb::PropertyModel& model = m_window->settingsPage()->model();
    QVERIFY(model.setValue("General/language", "vi"));
    QVERIFY(model.setValue("General/autosaveMinutes", 30));
    QVERIFY(model.setValue("General/autosave", false));
    m_window->settingsPage()->resetAll();
    QCOMPARE(model.find("General/language")->value().toString(), QString("en"));
    QCOMPARE(model.find("General/autosave")->value().toBool(), true);
    QCOMPARE(model.find("General/autosaveMinutes")->value().toInt(), 5);
}

void tst_Trial::pluginsRuntimeChangesAndSearch()
{
    PluginsPage* page = m_window->plugins();
    m_window->tabs()->setCurrentWidget(page);
    page->load({"export", {"PNG", "EXR"}});
    QCOMPARE(page->model().rowCount(), 3);
    page->unload("sharpen");
    QCOMPARE(page->model().rowCount(), 2);

    QTest::keyClicks(page->search(), "thresh");
    QCOMPARE(page->proxy().rowCount(), 2); // both groups, kept for their matching child
    const QModelIndex group = page->proxy().index(0, 0);
    QCOMPARE(page->proxy().rowCount(group), 1);

    // Edit through the filter proxy.
    auto* threshold = openEditor<QDoubleSpinBox>(page->view(), page->proxy().index(0, 1, group));
    QVERIFY(threshold);
    threshold->setValue(0.8);
    QTest::keyClick(threshold, Qt::Key_Return);
    QTRY_COMPARE(page->model().find("denoise/threshold")->value().toDouble(), 0.8);

    page->search()->clear();
    QCOMPARE(page->proxy().rowCount(page->proxy().index(0, 0)), 4);
}

void tst_Trial::screenshots()
{
    const QString directory = qEnvironmentVariable("QPB_TRIAL_SCREENSHOTS");
    if (directory.isEmpty())
        QSKIP("Set QPB_TRIAL_SCREENSHOTS to save screenshots");
    m_window->inspector()->model().setValue("Camera/fov", 90);
    const char* names[] = {"inspector.png", "settings.png", "plugins.png"};
    for (int i = 0; i < 3; ++i) {
        m_window->tabs()->setCurrentIndex(i);
        QVERIFY(m_window->grab().save(QDir(directory).filePath(names[i])));
    }
}

QTEST_MAIN(tst_Trial)
#include "tst_trial.moc"
