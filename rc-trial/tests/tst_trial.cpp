// Drives the RC trial application through its UI (docs/PLAN.md RC.1).

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "DevicesPage.h"
#include "InspectorPage.h"
#include "MainWindow.h"
#include "PluginsPage.h"
#include "SettingsPage.h"

namespace {

template <class Editor>
Editor* openEditor(qpb::PropertyTreeView* view, const QModelIndex& valueIndex)
{
    view->setFocus();
    if (view->currentIndex() == valueIndex)
        view->edit(valueIndex); // already current: open it again, as F2 does
    else
        view->setCurrentIndex(valueIndex);
    QWidget* editor = view->indexWidget(valueIndex);
    if (!editor)
        return nullptr;
    if (auto* e = qobject_cast<Editor*>(editor))
        return e;
    return editor->findChild<Editor*>(); // part of a composite editor
}

QModelIndex valueIndex(qpb::PropertyModel& model, const QString& path)
{
    return model.indexOf(model.find(path), qpb::PropertyModel::ValueColumn);
}

QString currentPath(qpb::PropertyTreeView* view)
{
    return view->currentIndex().data(qpb::PropertyModel::PathRole).toString();
}

// Presses Tab in whatever has the focus (an editor or one of its children).
void pressTab()
{
    QWidget* focus = QApplication::focusWidget();
    QVERIFY(focus);
    QTest::keyClick(focus, Qt::Key_Tab);
}

// Opens the view's context menu on index and triggers its first action if
// it is enabled. Returns whether it was enabled.
bool triggerContextAction(qpb::PropertyTreeView* view, const QModelIndex& index)
{
    bool enabled = false;
    QTimer::singleShot(0, view, [&enabled] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu)
            return;
        QAction* action = menu->actions().value(0);
        enabled = action && action->isEnabled();
        if (enabled)
            action->trigger();
        menu->close();
    });
    const QPoint pos = view->visualRect(index).center();
    QContextMenuEvent event(QContextMenuEvent::Mouse, pos, view->viewport()->mapToGlobal(pos));
    QApplication::sendEvent(view->viewport(), &event);
    return enabled;
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
    void inspectorRejectsDuplicateName();
    void inspectorHidesCameraForLights();
    void settingsLoadStoredValues();
    void settingsDependencyAndSave();
    void settingsSaveUpdatesReadOnlyStatus();
    void settingsResetAll();
    void settingsKeyboardNavigation();
    void settingsRejectsMissingDirectory();
    void settingsContextMenuResetsGroup();
    void pluginsRuntimeChangesAndSearch();
    void pluginsEnableAllIsOneBatch();
    // Round 4 (1.2.0).
    void settingsStoredWithSerialization();
    void settingsFormLayout();
    void settingsJsonExportImport();
    void pluginsSearchByGroupName();
    void devicesFollowObjects();
    void devicesObjectRefusesValue();
    void devicesRemovedObject();
    // Round 5 (1.3.0).
    void devicesTitlesAndLiveValues();
    // Round 6 (1.4.0).
    void settingsDependencyIsDeclared();
    void inspectorCallbacksFollowSelection();
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
    page->objectList()->setCurrentRow(0);

    // Field of view uses the application's slider editor.
    auto* fov = openEditor<QSlider>(page->view(), valueIndex(page->model(), "Camera/fov"));
    QVERIFY(fov);
    QCOMPARE(fov->maximum(), 170);
    fov->setValue(75);
    QTest::keyClick(fov, Qt::Key_Return);
    QTRY_COMPARE(page->scene()[0].fov, 75);

    page->objectList()->setCurrentRow(1);
    auto* name = openEditor<QLineEdit>(page->view(), valueIndex(page->model(), "name"));
    QVERIFY(name);
    name->setText("Rim light");
    QTest::keyClick(name, Qt::Key_Tab); // commits and moves on
    QTRY_COMPARE(page->scene()[1].name, QString("Rim light"));
    QCOMPARE(page->objectList()->item(1)->text(), QString("Rim light"));

    QVERIFY(page->model().setValue("Render/tint", QColor(Qt::red)));
    QCOMPARE(page->scene()[1].tint, QColor(Qt::red));
    QCOMPARE(page->scene()[0].fov, 75); // other objects untouched
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

void tst_Trial::inspectorRejectsDuplicateName()
{
    InspectorPage* page = m_window->inspector();
    page->objectList()->setCurrentRow(1);
    QSignalSpy failed(&page->model(), &qpb::PropertyModel::validationFailed);

    auto* name = openEditor<QLineEdit>(page->view(), valueIndex(page->model(), "name"));
    QVERIFY(name);
    name->setText("Main camera");
    QTest::keyClick(name, Qt::Key_Return);
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(failed[0][0].toString(), QString("name"));
    QVERIFY(failed[0][2].toString().contains("already named"));
    QCOMPARE(page->scene()[1].name, QString("Key light"));
    QCOMPARE(page->model().find("name")->value().toString(), QString("Key light"));
}

void tst_Trial::inspectorHidesCameraForLights()
{
    InspectorPage* page = m_window->inspector();
    qpb::PropertyTreeView* view = page->view();
    const auto cameraHidden = [&] {
        const QModelIndex camera = page->model().indexOf(page->model().find("Camera"));
        return view->isRowHidden(camera.row(), camera.parent());
    };
    page->objectList()->setCurrentRow(0);
    QVERIFY(!cameraHidden());
    page->objectList()->setCurrentRow(1);
    QVERIFY(cameraHidden());

    // Tab from the last transform field skips the hidden camera settings.
    QVERIFY(openEditor<QDoubleSpinBox>(view, valueIndex(page->model(), "Transform/z")));
    pressTab();
    QTRY_COMPARE(currentPath(view), QString("Render/tint"));
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

// Tab chains the editors: it never stops on the read-only About entries, on
// autosaveMinutes while autosave is off, or on check boxes (SPEC 5.5, finding
// F5). Check boxes are reached with the arrow keys and toggled with Space.
void tst_Trial::settingsKeyboardNavigation()
{
    SettingsPage* page = m_window->settingsPage();
    m_window->tabs()->setCurrentWidget(page);
    qpb::PropertyTreeView* view = page->view();
    qpb::PropertyModel& model = page->model();

    QVERIFY(openEditor<QWidget>(view, valueIndex(model, "General/language")));
    QStringList visited {currentPath(view)};
    for (int i = 0; i < 6; ++i) {
        pressTab();
        QCoreApplication::processEvents();
        if (currentPath(view) != visited.last())
            visited << currentPath(view);
    }
    QCOMPARE(visited,
        QStringList({"General/language", "General/autosaveMinutes", "General/signature",
            "Paths/projectDir", "Paths/cacheDir", "Limits/cacheBytes"}));

    // Down from language to the autosave check box, Space toggles it.
    auto* language = openEditor<QWidget>(view, valueIndex(model, "General/language"));
    QVERIFY(language);
    QTest::keyClick(language, Qt::Key_Escape);
    QTRY_VERIFY(view->hasFocus());
    QTest::keyClick(view, Qt::Key_Down);
    QCOMPARE(currentPath(view), QString("General/autosave"));
    QTest::keyClick(view, Qt::Key_Space);
    QCOMPARE(model.find("General/autosave")->value().toBool(), false);

    QVERIFY(openEditor<QWidget>(view, valueIndex(model, "General/language")));
    pressTab(); // autosave is off: skips autosaveMinutes
    QTRY_COMPARE(currentPath(view), QString("General/signature"));
}

void tst_Trial::settingsRejectsMissingDirectory()
{
    SettingsPage* page = m_window->settingsPage();
    m_window->tabs()->setCurrentWidget(page);
    QSignalSpy failed(&page->model(), &qpb::PropertyModel::validationFailed);
    const QString before = page->model().find("Paths/projectDir")->value().toString();

    auto* path = openEditor<QLineEdit>(page->view(), valueIndex(page->model(), "Paths/projectDir"));
    QVERIFY(path);
    path->setText(m_dir.filePath("does-not-exist"));
    QTest::keyClick(path, Qt::Key_Return);
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(page->model().find("Paths/projectDir")->value().toString(), before);

    // An existing directory is accepted.
    path = openEditor<QLineEdit>(page->view(), valueIndex(page->model(), "Paths/projectDir"));
    QVERIFY(path);
    path->setText(m_dir.path());
    QTest::keyClick(path, Qt::Key_Return);
    QTRY_COMPARE(page->model().find("Paths/projectDir")->value().toString(), m_dir.path());
}

// "Reset group" from the context menu restores user settings and leaves the
// read-only status maintained by the application alone.
void tst_Trial::settingsContextMenuResetsGroup()
{
    SettingsPage* page = m_window->settingsPage();
    m_window->tabs()->setCurrentWidget(page);
    qpb::PropertyModel& model = page->model();
    QVERIFY(model.setValue("General/language", "vi"));
    QVERIFY(model.setValue("General/autosaveMinutes", 30));
    page->save();
    const QString lastSaved = model.find("About/lastSaved")->value().toString();

    QVERIFY(!triggerContextAction(page->view(), model.indexOf(model.find("About"))));
    QVERIFY(triggerContextAction(page->view(), model.indexOf(model.find("General"))));
    QCOMPARE(model.find("General/language")->value().toString(), QString("en"));
    QCOMPARE(model.find("General/autosaveMinutes")->value().toInt(), 5);
    QCOMPARE(model.find("About/lastSaved")->value().toString(), lastSaved);
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

void tst_Trial::pluginsEnableAllIsOneBatch()
{
    PluginsPage* page = m_window->plugins();
    QSignalSpy changed(&page->model(), &qpb::PropertyModel::valueChanged);
    QSignalSpy batch(&page->model(), &qpb::PropertyModel::batchValueChanged);
    page->setAllEnabled(true);
    QCOMPARE(changed.count(), 2);
    QCOMPARE(batch.count(), 1);
    QCOMPARE(batch[0][0].toStringList(), QStringList({"denoise/enabled", "sharpen/enabled"}));
    const QModelIndex group = page->proxy().index(0, 0);
    QCOMPARE(page->proxy().index(0, 1, group).data(Qt::EditRole).toBool(), true);
}

// --- Round 4 (1.2.0) ------------------------------------------------------------

// qpb::serialization replaces the hand-written save/load loops: same keys,
// read-only entries left out, a 64-bit value survives the INI file.
void tst_Trial::settingsStoredWithSerialization()
{
    qpb::PropertyModel& model = m_window->settingsPage()->model();
    const qint64 big = qint64(3) << 40;
    QVERIFY(model.setValue("General/language", "vi"));
    QVERIFY(model.setValue("Limits/cacheBytes", big));
    QVERIFY(model.setValue("General/signature", "Best,\nThe team"));
    m_window->settingsPage()->save();
    {
        QSettings stored(settingsFile(), QSettings::IniFormat);
        QCOMPARE(stored.value("General/language").toString(), QString("vi"));
        QVERIFY(stored.contains("Limits/cacheBytes"));
        QVERIFY(!stored.contains("About/version"));
        QVERIFY(!stored.contains("About/lastSaved"));
    }
    m_window.reset();
    createWindow();
    qpb::PropertyModel& reloaded = m_window->settingsPage()->model();
    QCOMPARE(reloaded.find("General/language")->value().toString(), QString("vi"));
    QCOMPARE(reloaded.find("Limits/cacheBytes")->value(), QVariant::fromValue(big));
    QCOMPARE(reloaded.find("General/signature")->value().toString(), QString("Best,\nThe team"));
}

// The same model as a form: the dependency and the multi-line field work
// there too, and edits in the form are saved.
void tst_Trial::settingsFormLayout()
{
    SettingsPage* page = m_window->settingsPage();
    m_window->tabs()->setCurrentWidget(page);
    page->setFormLayout(true);
    QVERIFY(page->isFormLayout());
    qpb::PropertyFormView* form = page->form();
    QTRY_VERIFY(form->isVisible());

    auto* autosave = qobject_cast<QCheckBox*>(form->editor("General/autosave"));
    QVERIFY(autosave);
    QTest::mouseClick(autosave, Qt::LeftButton, {}, QPoint(6, autosave->height() / 2));
    QCOMPARE(page->model().find("General/autosave")->value().toBool(), false);
    QVERIFY(!form->editor("General/autosaveMinutes")->isEnabled());

    auto* signature = qobject_cast<QPlainTextEdit*>(form->editor("General/signature"));
    QVERIFY(signature);
    signature->setFocus();
    QTest::keyClicks(signature, "Hi");
    QTest::keyClick(signature, Qt::Key_Return);
    QTest::keyClicks(signature, "there");
    QTest::keyClick(signature, Qt::Key_Return, Qt::ControlModifier);
    QTRY_COMPARE(page->model().find("General/signature")->value().toString(), QString("Hi\nthere"));

    // The tree shows the same values when switching back.
    page->setFormLayout(false);
    const QModelIndex autosaveIndex = valueIndex(page->model(), "General/autosave");
    QCOMPARE(autosaveIndex.data(Qt::CheckStateRole).toInt(), int(Qt::Unchecked));
    page->save();
    QSettings stored(settingsFile(), QSettings::IniFormat);
    QCOMPARE(stored.value("General/signature").toString(), QString("Hi\nthere"));
}

void tst_Trial::settingsJsonExportImport()
{
    SettingsPage* page = m_window->settingsPage();
    QVERIFY(page->model().setValue("General/autosaveMinutes", 25));
    const QJsonObject exported = page->exportJson();
    QVERIFY(!exported.contains("About"));

    page->resetAll();
    QCOMPARE(page->model().find("General/autosaveMinutes")->value().toInt(), 5);
    QSignalSpy batch(&page->model(), &qpb::PropertyModel::batchValueChanged);
    QVERIFY(page->importJson(exported));
    QCOMPARE(batch.count(), 1);
    QCOMPARE(page->model().find("General/autosaveMinutes")->value().toInt(), 25);

    // A bad value is reported; the others still apply.
    QJsonObject general = exported.value("General").toObject();
    general.insert("language", "fr");
    general.insert("autosaveMinutes", 40);
    QJsonObject bad = exported;
    bad.insert("General", general);
    QVERIFY(!page->importJson(bad));
    QCOMPARE(page->model().find("General/language")->value().toString(), QString("en"));
    QCOMPARE(page->model().find("General/autosaveMinutes")->value().toInt(), 40);
}

// PropertyFilterProxyModel: a matching group shows all of its properties.
void tst_Trial::pluginsSearchByGroupName()
{
    PluginsPage* page = m_window->plugins();
    QTest::keyClicks(page->search(), "denoise");
    QCOMPARE(page->proxy().rowCount(), 1);
    QCOMPARE(page->proxy().rowCount(page->proxy().index(0, 0)), 4);
}

void tst_Trial::devicesFollowObjects()
{
    DevicesPage* page = m_window->devices();
    m_window->tabs()->setCurrentWidget(page);
    qpb::PropertyFormView* form = page->form();
    Device* mic = page->devices().value(0);
    QVERIFY(mic);
    QCOMPARE(page->source().groupOf(mic)->path(), QString("studio_mic"));

    // Types and metadata from the Q_PROPERTYs and Q_CLASSINFO.
    qpb::PropertyModel& model = page->model();
    QCOMPARE(model.find("studio_mic/gain")->displayName(), QString("Input gain"));
    QCOMPARE(model.find("studio_mic/recordDir")->typeId(), qpb::TypeId(qpb::Types::DirPath));
    QCOMPARE(model.find("studio_mic/capacity")->typeId(), qpb::TypeId(qpb::Types::Int64));
    QVERIFY(model.find("studio_mic/firmware")->isReadOnly());
    QVERIFY(!form->editor("studio_mic/used")->isEnabled()); // no WRITE

    // Form -> object.
    auto* name = qobject_cast<QLineEdit*>(form->editor("studio_mic/name"));
    QVERIFY(name);
    name->setFocus();
    name->setText("Vocal mic");
    QTest::keyClick(name, Qt::Key_Return);
    QTRY_COMPARE(mic->name(), QString("Vocal mic"));
    auto* mode = qobject_cast<QComboBox*>(form->editor("studio_mic/mode"));
    QVERIFY(mode);
    mode->setFocus();
    QTest::keyClick(mode, Qt::Key_Down);
    QCOMPARE(mic->mode(), Device::Recording);

    // Object -> form (NOTIFY signals), with values beyond the range of int.
    mic->record(qint64(5) << 30);
    QCOMPARE(model.find("studio_mic/used")->value(), QVariant::fromValue(qint64(5) << 30));
    mic->setGain(12.5);
    QCOMPARE(qobject_cast<QDoubleSpinBox*>(form->editor("studio_mic/gain"))->value(), 12.5);
}

// The object refuses a value: the form shows what the object kept.
void tst_Trial::devicesObjectRefusesValue()
{
    DevicesPage* page = m_window->devices();
    Device* recorder = page->devices().value(1);
    QVERIFY(page->model().setValue("field_recorder/enabled", false));
    QVERIFY(!recorder->isEnabled());
    QVERIFY(page->model().setValue("field_recorder/mode", int(Device::Streaming)));
    QCOMPARE(recorder->mode(), Device::Idle);
    QCOMPARE(page->model().find("field_recorder/mode")->value().toInt(), int(Device::Idle));
    QCOMPARE(qobject_cast<QComboBox*>(page->form()->editor("field_recorder/mode"))->currentText(),
        QString("Idle"));
}

void tst_Trial::devicesRemovedObject()
{
    DevicesPage* page = m_window->devices();
    delete page->devices().value(0);
    QVERIFY(!page->model().find("studio_mic"));
    QVERIFY(!page->form()->editor("studio_mic/name"));
    QVERIFY(page->form()->editor("field_recorder/name"));
    QCOMPARE(page->source().objects().size(), 1);
}

// --- Round 5 (1.3.0) ------------------------------------------------------------

// F8: sections are titled with the device name and follow renames; paths keep
// the object name. F9: the "used" counter is live: never bold, not reset.
void tst_Trial::devicesTitlesAndLiveValues()
{
    DevicesPage* page = m_window->devices();
    m_window->tabs()->setCurrentWidget(page);
    qpb::PropertyModel& model = page->model();
    Device* mic = page->devices().value(0);
    QCOMPARE(model.find("studio_mic")->displayName(), QString("Studio mic"));
    mic->setName("Vocal mic");
    QCOMPARE(model.find("studio_mic")->displayName(), QString("Vocal mic"));
    QVERIFY(model.find("studio_mic/name")); // same path

    qpb::Property* used = model.find("studio_mic/used");
    QVERIFY(used->isLive());
    mic->record(qint64(1) << 30);
    QCOMPARE(used->value(), QVariant::fromValue(qint64(1) << 30));
    QVERIFY(!used->isModified());
    QVERIFY(!model.indexOf(used).data(qpb::PropertyModel::IsModifiedRole).toBool());

    // "Reset group" restores the settings but not the live counter.
    QVERIFY(model.setValue("studio_mic/gain", 18.0));
    QVERIFY(model.find("studio_mic")->resetToDefault());
    QCOMPARE(mic->gain(), 6.0);
    QCOMPARE(used->value(), QVariant::fromValue(qint64(1) << 30));
    QCOMPARE(model.find("studio_mic")->displayName(), QString("Studio mic")); // name reset too
}

// --- Round 6 (1.4.0) ------------------------------------------------------------

// F10: the dependency is a condition; the application's own flag is untouched,
// and loading stored values evaluates it without any code.
void tst_Trial::settingsDependencyIsDeclared()
{
    qpb::PropertyModel& model = m_window->settingsPage()->model();
    qpb::Property* minutes = model.find("General/autosaveMinutes");
    QVERIFY(model.setValue("General/autosave", false));
    QVERIFY(!minutes->isEnabled());
    QVERIFY(!minutes->flags().testFlag(qpb::Property::Flag::Disabled));
    QVERIFY(model.setValue("General/autosave", true));
    QVERIFY(minutes->isEnabled());
}

// F3: per-path callbacks write edits back to the selected object, also after
// the tree was replaced for another selection.
void tst_Trial::inspectorCallbacksFollowSelection()
{
    InspectorPage* page = m_window->inspector();
    page->objectList()->setCurrentRow(2);
    QVERIFY(page->model().setValue("Transform/y", -3.5));
    QCOMPARE(page->scene()[2].y, -3.5);
    page->objectList()->setCurrentRow(0);
    QVERIFY(page->model().setValue("Transform/y", 7.0));
    QCOMPARE(page->scene()[0].y, 7.0);
    QCOMPARE(page->scene()[2].y, -3.5);
}

void tst_Trial::screenshots()
{
    const QString directory = qEnvironmentVariable("QPB_TRIAL_SCREENSHOTS");
    if (directory.isEmpty())
        QSKIP("Set QPB_TRIAL_SCREENSHOTS to save screenshots");
    m_window->inspector()->model().setValue("Camera/fov", 90);
    m_window->plugins()->setAllEnabled(true);
    m_window->devices()->devices().value(0)->record(qint64(5) << 30);
    const char* names[] = {"inspector.png", "settings.png", "plugins.png", "devices.png"};
    for (int i = 0; i < 4; ++i) {
        m_window->tabs()->setCurrentIndex(i);
        QVERIFY(m_window->grab().save(QDir(directory).filePath(names[i])));
    }
}

QTEST_MAIN(tst_Trial)
#include "tst_trial.moc"
