// QObjectPropertySource (see qpb/doc/guide/qobject-source.md).

#include <qpb/qpbcore.h>

#include <QAbstractItemModelTester>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>

#include <functional>

using namespace qpb;

namespace {

class Camera : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("qpb:fov", "min=10;max=170;suffix= deg;displayName=Field of view")
    Q_CLASSINFO("qpb:lut", "type=filepath;filter=LUT (*.cube)")
    Q_CLASSINFO("qpb:secret", "exclude")
    Q_CLASSINFO("qpb:exposure", "hidden;toolTip=Exposure value")
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(int fov READ fov WRITE setFov NOTIFY fovChanged)
    Q_PROPERTY(double exposure READ exposure WRITE setExposure NOTIFY exposureChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(qint64 frames READ frames WRITE setFrames)
    Q_PROPERTY(Projection projection READ projection WRITE setProjection NOTIFY projectionChanged)
    Q_PROPERTY(QString lut MEMBER m_lut NOTIFY lutChanged)
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString secret MEMBER m_secret)
    Q_PROPERTY(QVariantList unsupported MEMBER m_unsupported)

public:
    enum Projection { Perspective, Orthographic };
    Q_ENUM(Projection)

    QString name() const
    {
        return m_name;
    }
    void setName(const QString& name)
    {
        if (m_name != name) {
            m_name = name;
            emit nameChanged();
        }
    }
    int fov() const
    {
        return m_fov;
    }
    // Refuses odd values: the model must show the object's value afterwards.
    void setFov(int fov)
    {
        fov -= fov % 2;
        if (m_fov != fov) {
            m_fov = fov;
            emit fovChanged();
        }
    }
    double exposure() const
    {
        return m_exposure;
    }
    void setExposure(double exposure)
    {
        m_exposure = exposure;
        emit exposureChanged();
    }
    bool active() const
    {
        return m_active;
    }
    void setActive(bool active)
    {
        m_active = active;
        emit activeChanged();
    }
    qint64 frames() const
    {
        return m_frames;
    }
    void setFrames(qint64 frames)
    {
        m_frames = frames;
    }
    Projection projection() const
    {
        return m_projection;
    }
    void setProjection(Projection projection)
    {
        m_projection = projection;
        emit projectionChanged();
    }
    QString id() const
    {
        return QStringLiteral("cam-1");
    }

signals:
    void nameChanged();
    void fovChanged();
    void exposureChanged();
    void activeChanged();
    void projectionChanged();
    void lutChanged();

private:
    QString m_name = QStringLiteral("Main");
    int m_fov = 60;
    double m_exposure = 1.5;
    bool m_active = true;
    qint64 m_frames = 5000000000;
    Projection m_projection = Perspective;
    QString m_lut;
    QString m_secret;
    QVariantList m_unsupported;
};

// Chooses and orders its properties.
class Light : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("qpb:properties", "intensity, name")
    Q_PROPERTY(QString name MEMBER m_name)
    Q_PROPERTY(double intensity MEMBER m_intensity)
    Q_PROPERTY(bool ignored MEMBER m_ignored)

public:
    QString m_name = QStringLiteral("Key");
    double m_intensity = 2.0;
    bool m_ignored = false;
};

// Since 1.3: a title property and live values.
class Sensor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("qpb:title", "label")
    Q_CLASSINFO("qpb:offset", "live")
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    Q_PROPERTY(double offset MEMBER m_offset NOTIFY offsetChanged)
    Q_PROPERTY(int reading READ reading NOTIFY readingChanged)
    Q_PROPERTY(QString serial READ serial CONSTANT)

public:
    QString label() const
    {
        return m_label;
    }
    void setLabel(const QString& label)
    {
        m_label = label;
        emit labelChanged();
    }
    int reading() const
    {
        return m_reading;
    }
    void measure(int value)
    {
        m_reading = value;
        emit readingChanged();
    }
    QString serial() const
    {
        return QStringLiteral("S-1");
    }

    QString m_label = QStringLiteral("Outdoor");
    double m_offset = 0.0;
    int m_reading = 0;

signals:
    void labelChanged();
    void offsetChanged();
    void readingChanged();
};

// No class info: titled through setTitleProperty().
class Plain : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString caption MEMBER m_caption NOTIFY captionChanged)

public:
    QString m_caption = QStringLiteral("First");

signals:
    void captionChanged();
};

// Since 1.4: conditions between Q_PROPERTYs of the same object.
class Recorder : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("qpb:minutes", "enabledWhen=autostop")
    Q_CLASSINFO("qpb:folder", "visibleWhen=saveToDisk")
    Q_PROPERTY(bool autostop MEMBER m_autostop NOTIFY changed)
    Q_PROPERTY(int minutes MEMBER m_minutes NOTIFY changed)
    Q_PROPERTY(bool saveToDisk MEMBER m_saveToDisk NOTIFY changed)
    Q_PROPERTY(QString folder MEMBER m_folder NOTIFY changed)

public:
    bool m_autostop = false;
    int m_minutes = 10;
    bool m_saveToDisk = true;
    QString m_folder;

signals:
    void changed();
};

// Application code reacting inside the setter: the hook runs instead of the
// notification and may remove or delete the object, so nothing of the object
// is used after it.
class Reactive : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int level READ level WRITE setLevel NOTIFY levelChanged)

public:
    int level() const
    {
        return m_level;
    }
    void setLevel(int level)
    {
        m_level = level;
        if (inSetter) {
            const std::function<void()> hook = inSetter;
            hook();
            return;
        }
        emit levelChanged();
    }

    std::function<void()> inSetter;

signals:
    void levelChanged();

private:
    int m_level = 1;
};

QStringList childIds(const PropertyGroup* group)
{
    QStringList ids;
    for (const Property* child : group->children())
        ids << child->id();
    return ids;
}

} // namespace

class tst_QObjectPropertySource : public QObject
{
    Q_OBJECT

private slots:
    void propertiesAndTypes();
    void metadata();
    void propertyListAndOrder();
    void groupIds();
    void modelToObject();
    void objectToModel();
    void objectDestroyed();
    void removeObjectAndSourceLifetime();
    void titleFromClassInfo();
    void titleFromSetting();
    void liveProperties();
    void conditionsFromMetadata();

    void treeReplacedByAnEarlierHandlerStillReachesTheObject();
    void treeReplacedWhileTheObjectAdjustsTheValue();

    void objectChangesAreApplicationWrites();
    void userEditsOfSourcedPropertiesEmitUserValueChanged();
    void treeReplacedByAUserValueHandlerStillReachesTheObject();
    void treeReplacedByAUserValueHandlerWhileTheObjectAdjustsTheValue();

    // Removing or deleting objects while the source is syncing them.
    void modelWriteWithAReadBackSlotRemovingTheObject_data();
    void modelWriteWithAReadBackSlotRemovingTheObject();
    void objectReactingToAModelWriteByLeaving_data();
    void objectReactingToAModelWriteByLeaving();
    void objectChangeWithSeveralPropertiesLeavingInTheFirstSlot_data();
    void objectChangeWithSeveralPropertiesLeavingInTheFirstSlot();
    void titleNotificationAfterTheObjectLeft();
    void refreshWithAnObjectLeavingInASlot_data();
    void refreshWithAnObjectLeavingInASlot();
    void otherObjectRemovedInsideAModelWrite();
    void otherObjectRemovedInsideAnObjectChange();
    void objectRemovedByAnEarlierSlotLeavesNoRecord();
    void objectRemovedAndAddedAgainInsideASync_data();
    void objectRemovedAndAddedAgainInsideASync();
};

void tst_QObjectPropertySource::propertiesAndTypes()
{
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    QCOMPARE(source.model(), &model);
    Camera camera;
    camera.setObjectName(QStringLiteral("camera"));

    PropertyGroup* group = source.addObject(&camera);
    QVERIFY(group);
    QCOMPARE(group->path(), QStringLiteral("camera"));
    QCOMPARE(source.groupOf(&camera), group);
    QCOMPARE(source.objects(), QList<QObject*>({&camera}));
    // objectName (QObject), the excluded and the unsupported ones are skipped.
    QCOMPARE(childIds(group),
        QStringList({"name", "fov", "exposure", "active", "frames", "projection", "lut", "id"}));

    QCOMPARE(group->child(QStringLiteral("name"))->typeId(), TypeId(Types::String));
    QCOMPARE(group->child(QStringLiteral("fov"))->typeId(), TypeId(Types::Int));
    QCOMPARE(group->child(QStringLiteral("exposure"))->typeId(), TypeId(Types::Double));
    QCOMPARE(group->child(QStringLiteral("active"))->typeId(), TypeId(Types::Bool));
    QCOMPARE(group->child(QStringLiteral("frames"))->typeId(), TypeId(Types::Int64));
    QCOMPARE(group->child(QStringLiteral("frames"))->value(), QVariant(qint64(5000000000)));
    Property* projection = group->child(QStringLiteral("projection"));
    QCOMPARE(projection->typeId(), TypeId(Types::Enum));
    QCOMPARE(projection->value(), QVariant(0));
    const auto options = projection->attribute(Attr::Options).value<QList<EnumOption>>();
    QCOMPARE(options.size(), 2);
    QCOMPARE(options[1].label, QStringLiteral("Orthographic"));
    QCOMPARE(options[1].value, QVariant(1));

    // CONSTANT without WRITE: read-only. Values are the defaults.
    QVERIFY(group->child(QStringLiteral("id"))->isReadOnly());
    QVERIFY(!group->child(QStringLiteral("name"))->isReadOnly());
    QVERIFY(!group->child(QStringLiteral("name"))->isModified());
}

void tst_QObjectPropertySource::metadata()
{
    PropertyModel model;
    QObjectPropertySource source(&model);
    Camera camera;
    PropertyGroup* group = source.addObject(&camera);

    Property* fov = group->child(QStringLiteral("fov"));
    QCOMPARE(fov->displayName(), QStringLiteral("Field of view"));
    QCOMPARE(fov->attribute(Attr::Maximum).toInt(), 170);
    QCOMPARE(fov->attribute(Attr::Suffix).toString(), QStringLiteral(" deg"));
    QVERIFY(fov->setValue(500));
    QCOMPARE(fov->value(), QVariant(170)); // clamped by the attribute
    QCOMPARE(camera.fov(), 170);

    Property* lut = group->child(QStringLiteral("lut"));
    QCOMPARE(lut->typeId(), TypeId(Types::FilePath));
    QCOMPARE(lut->attribute(Attr::Filter).toString(), QStringLiteral("LUT (*.cube)"));

    Property* exposure = group->child(QStringLiteral("exposure"));
    QVERIFY(!exposure->isVisible());
    QCOMPARE(exposure->toolTip(), QStringLiteral("Exposure value"));
}

void tst_QObjectPropertySource::propertyListAndOrder()
{
    PropertyModel model;
    QObjectPropertySource source(&model);
    Light light;
    PropertyGroup* group = source.addObject(&light);
    QCOMPARE(childIds(group), QStringList({"intensity", "name"}));
    QCOMPARE(group->id(), QStringLiteral("Light")); // no object name: the class name
}

void tst_QObjectPropertySource::groupIds()
{
    PropertyModel model(PropertyGroup::create(QStringLiteral("scene")));
    QObjectPropertySource source(&model);
    Light a;
    Light b;
    Light c;
    PropertyGroup& lights = model.root()->addGroup(QStringLiteral("lights"));
    QCOMPARE(source.addObject(&a, &lights)->path(), QStringLiteral("lights/Light"));
    QCOMPARE(source.addObject(&b, &lights)->path(), QStringLiteral("lights/Light_2"));
    QCOMPARE(source.addObject(&c, &lights, QStringLiteral("fill"))->path(),
        QStringLiteral("lights/fill"));
    QCOMPARE(source.addObject(&a), lights.child(QStringLiteral("Light"))); // already added
    QCOMPARE(source.addObject(nullptr), nullptr);
}

void tst_QObjectPropertySource::modelToObject()
{
    PropertyModel model;
    QObjectPropertySource source(&model);
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    source.addObject(&camera);

    QVERIFY(model.setValue(QStringLiteral("cam/name"), QStringLiteral("Rim")));
    QCOMPARE(camera.name(), QStringLiteral("Rim"));
    QVERIFY(model.setValue(QStringLiteral("cam/projection"), 1));
    QCOMPARE(camera.projection(), Camera::Orthographic);
    QVERIFY(model.setValue(QStringLiteral("cam/frames"), qint64(7000000000)));
    QCOMPARE(camera.frames(), qint64(7000000000));

    // A user edit through the model (setData) reaches the object too.
    const QModelIndex active
        = model.indexOf(model.find(QStringLiteral("cam/active")), PropertyModel::ValueColumn);
    QVERIFY(model.setData(active, Qt::Unchecked, Qt::CheckStateRole));
    QVERIFY(!camera.active());

    // The object adjusts the value: the model shows the object's value.
    QVERIFY(model.setValue(QStringLiteral("cam/fov"), 75));
    QCOMPARE(camera.fov(), 74);
    QCOMPARE(model.find(QStringLiteral("cam/fov"))->value(), QVariant(74));
}

void tst_QObjectPropertySource::objectToModel()
{
    PropertyModel model;
    QObjectPropertySource source(&model);
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    source.addObject(&camera);
    QSignalSpy changed(&model, &PropertyModel::valueChanged);

    camera.setName(QStringLiteral("Top"));
    QCOMPARE(model.find(QStringLiteral("cam/name"))->value(), QVariant(QStringLiteral("Top")));
    QCOMPARE(changed.count(), 1);
    camera.setProperty("lut", QStringLiteral("/tmp/a.cube")); // MEMBER with NOTIFY
    QCOMPARE(
        model.find(QStringLiteral("cam/lut"))->value(), QVariant(QStringLiteral("/tmp/a.cube")));

    // No NOTIFY signal: refresh() reads it.
    camera.setFrames(42);
    QCOMPARE(model.find(QStringLiteral("cam/frames"))->value(), QVariant(qint64(5000000000)));
    source.refresh();
    QCOMPARE(model.find(QStringLiteral("cam/frames"))->value(), QVariant(qint64(42)));
}

void tst_QObjectPropertySource::objectDestroyed()
{
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    auto camera = std::make_unique<Camera>();
    camera->setObjectName(QStringLiteral("cam"));
    source.addObject(camera.get());
    QVERIFY(model.find(QStringLiteral("cam")));
    camera.reset();
    QVERIFY(!model.find(QStringLiteral("cam")));
    QVERIFY(source.objects().isEmpty());
}

void tst_QObjectPropertySource::removeObjectAndSourceLifetime()
{
    PropertyModel model;
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    {
        QObjectPropertySource source(&model);
        source.addObject(&camera);
        QVERIFY(source.removeObject(&camera));
        QVERIFY(!source.removeObject(&camera));
        QVERIFY(!model.find(QStringLiteral("cam")));

        source.addObject(&camera);
    }
    // The source is gone: the group stays, nothing is synchronized any more.
    QVERIFY(model.find(QStringLiteral("cam")));
    QVERIFY(model.setValue(QStringLiteral("cam/name"), QStringLiteral("Other")));
    QCOMPARE(camera.name(), QStringLiteral("Main"));
    camera.setName(QStringLiteral("Changed"));
    QCOMPARE(model.find(QStringLiteral("cam/name"))->value(), QVariant(QStringLiteral("Other")));
}

void tst_QObjectPropertySource::titleFromClassInfo()
{
    PropertyModel model;
    QObjectPropertySource source(&model);
    Sensor sensor;
    sensor.setObjectName(QStringLiteral("sensor1"));
    PropertyGroup* group = source.addObject(&sensor);
    QCOMPARE(group->id(), QStringLiteral("sensor1")); // paths stay stable
    QCOMPARE(group->displayName(), QStringLiteral("Outdoor"));

    sensor.setLabel(QStringLiteral("Roof"));
    QCOMPARE(group->displayName(), QStringLiteral("Roof"));
    // Edited through the model: the object's NOTIFY updates the title too.
    QVERIFY(model.setValue(QStringLiteral("sensor1/label"), QStringLiteral("Garden")));
    QCOMPARE(group->displayName(), QStringLiteral("Garden"));
    QCOMPARE(model.indexOf(group).data(Qt::DisplayRole).toString(), QStringLiteral("Garden"));

    // The class info wins over setTitleProperty().
    source.setTitleProperty(QStringLiteral("serial"));
    Sensor other;
    QCOMPARE(source.addObject(&other)->displayName(), QStringLiteral("Outdoor"));
}

void tst_QObjectPropertySource::titleFromSetting()
{
    PropertyModel model;
    QObjectPropertySource source(&model);
    QCOMPARE(source.titleProperty(), QString());
    Plain before;
    before.setObjectName(QStringLiteral("before"));
    QCOMPARE(source.addObject(&before)->displayName(), QStringLiteral("before")); // 1.2 behaviour

    source.setTitleProperty(QStringLiteral("caption"));
    QCOMPARE(source.titleProperty(), QStringLiteral("caption"));
    Plain after;
    after.setObjectName(QStringLiteral("after"));
    PropertyGroup* group = source.addObject(&after);
    QCOMPARE(group->displayName(), QStringLiteral("First"));
    after.setProperty("caption", QStringLiteral("Second"));
    QCOMPARE(group->displayName(), QStringLiteral("Second"));
    QCOMPARE(source.groupOf(&before)->displayName(), QStringLiteral("before")); // unchanged

    // An empty value keeps the previous title; a missing property is ignored.
    after.setProperty("caption", QString());
    QCOMPARE(group->displayName(), QStringLiteral("Second"));
    source.setTitleProperty(QStringLiteral("nothing"));
    Plain third;
    third.setObjectName(QStringLiteral("third"));
    QCOMPARE(source.addObject(&third)->displayName(), QStringLiteral("third"));
}

void tst_QObjectPropertySource::liveProperties()
{
    PropertyModel model;
    QObjectPropertySource source(&model);
    QVERIFY(!source.liveReadOnlyProperties());
    Sensor sensor;
    sensor.setObjectName(QStringLiteral("s"));
    source.addObject(&sensor);

    // "live" metadata: a writable live value.
    Property* offset = model.find(QStringLiteral("s/offset"));
    QVERIFY(offset->isLive());
    QVERIFY(!offset->isReadOnly());
    // Read-only with NOTIFY: not live by default (1.2 behaviour).
    Property* reading = model.find(QStringLiteral("s/reading"));
    QVERIFY(!reading->isLive());
    sensor.measure(21);
    QVERIFY(reading->isModified());

    source.setLiveReadOnlyProperties(true);
    QVERIFY(source.liveReadOnlyProperties());
    Sensor second;
    second.setObjectName(QStringLiteral("t"));
    source.addObject(&second);
    Property* liveReading = model.find(QStringLiteral("t/reading"));
    QVERIFY(liveReading->isLive());
    second.measure(30);
    QCOMPARE(liveReading->value(), QVariant(30));
    QVERIFY(!liveReading->isModified());
    // CONSTANT (no NOTIFY): read-only, not live.
    QVERIFY(!model.find(QStringLiteral("t/serial"))->isLive());
}

void tst_QObjectPropertySource::conditionsFromMetadata()
{
    PropertyModel model;
    QObjectPropertySource source(&model);
    Recorder recorder;
    recorder.setObjectName(QStringLiteral("rec"));
    source.addObject(&recorder);
    Property* minutes = model.find(QStringLiteral("rec/minutes"));
    Property* folder = model.find(QStringLiteral("rec/folder"));
    QVERIFY(!minutes->isEnabled());
    QVERIFY(folder->isVisible());

    QVERIFY(model.setValue(QStringLiteral("rec/autostop"), true));
    QVERIFY(minutes->isEnabled());
    // The object changes by itself: NOTIFY updates the source, then the condition.
    recorder.m_saveToDisk = false;
    emit recorder.changed();
    QVERIFY(!folder->isVisible());
}

// A handler connected before the source replaces the tree: the source, which
// runs after it, still writes the value that was assigned to the object, and
// the object keeps working with the source afterwards.
void tst_QObjectPropertySource::treeReplacedByAnEarlierHandlerStillReachesTheObject()
{
    PropertyModel model(PropertyGroup::create(QStringLiteral("root")));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    int replaced = 0;
    QObject::connect(&model, &PropertyModel::valueChanged, &model,
        [&model, &replaced](const QString& path) {
            if (path == QStringLiteral("cam/name") && replaced++ == 0)
                model.setRoot(PropertyGroup::create(QStringLiteral("root")));
        });
    QObjectPropertySource source(&model);
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    QVERIFY(source.addObject(&camera));

    QVERIFY(model.setValue(QStringLiteral("cam/name"), QStringLiteral("Rim")));

    QCOMPARE(replaced, 1);
    QCOMPARE(camera.name(), QStringLiteral("Rim"));
    QVERIFY(!model.find(QStringLiteral("cam")));
    QVERIFY(!source.groupOf(&camera));

    // The object is still bound but has no properties in the new tree.
    camera.setName(QStringLiteral("Later"));
    QCOMPARE(camera.name(), QStringLiteral("Later"));
    QCOMPARE(source.objects(), QList<QObject*>({&camera}));
    QVERIFY(source.removeObject(&camera));
}

// The object refuses part of the value, so the source reads it back inside
// the original write; a handler connected after the source replaces the tree
// while that nested write is running. The object's notification sets the
// property again, so the signal is emitted twice for the path; the handler
// replaces the tree on the first emission only.
void tst_QObjectPropertySource::treeReplacedWhileTheObjectAdjustsTheValue()
{
    PropertyModel model(PropertyGroup::create(QStringLiteral("root")));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    QVERIFY(source.addObject(&camera));
    int replaced = 0;
    bool done = false;
    QObject::connect(&model, &PropertyModel::valueChanged, &model,
        [&model, &replaced, &done](const QString& path) {
            if (path == QStringLiteral("cam/fov") && !done) {
                done = true;
                ++replaced;
                model.setRoot(PropertyGroup::create(QStringLiteral("root")));
            }
        });

    QVERIFY(model.setValue(QStringLiteral("cam/fov"), 75));

    QCOMPARE(replaced, 1);
    QCOMPARE(camera.fov(), 74); // odd values are refused by the object
    QVERIFY(!model.find(QStringLiteral("cam")));
    camera.setFov(90); // no property to follow it any more
    QCOMPARE(camera.fov(), 90);
}

// Guard: what the source copies from the object, by notification or by
// refresh(), and what an application writes to the model is not a user edit.
void tst_QObjectPropertySource::objectChangesAreApplicationWrites()
{
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    source.addObject(&camera);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);

    camera.setName(QStringLiteral("Top")); // notification
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(0).toString(), QStringLiteral("cam/name"));
    camera.setFrames(42); // no notification: refresh() reads it
    source.refresh();
    QCOMPARE(model.find(QStringLiteral("cam/frames"))->value(), QVariant(qint64(42)));
    QCOMPARE(values.count(), 2);

    QVERIFY(model.setValue(QStringLiteral("cam/name"), QStringLiteral("Rim")));
    QCOMPARE(camera.name(), QStringLiteral("Rim"));
    QCOMPARE(values.count(), 3);
    QCOMPARE(user.count(), 0);
}

// A user edit of a sourced property reaches the object and announces both
// signals; the value the source reads back from the object is a separate,
// application, write.
void tst_QObjectPropertySource::userEditsOfSourcedPropertiesEmitUserValueChanged()
{
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    source.addObject(&camera);
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    QSignalSpy user(&model, &PropertyModel::userValueChanged);

    const QModelIndex name
        = model.indexOf(model.find(QStringLiteral("cam/name")), PropertyModel::ValueColumn);
    QVERIFY(model.setData(name, QStringLiteral("Rim")));
    QCOMPARE(camera.name(), QStringLiteral("Rim"));
    QCOMPARE(values.count(), 1);
    QCOMPARE(user.count(), 1);
    QCOMPARE(user.at(0).at(0).toString(), QStringLiteral("cam/name"));
    QCOMPARE(user.at(0).at(1), QVariant(QStringLiteral("Rim")));
    QCOMPARE(user.at(0).at(2), QVariant(QStringLiteral("Main")));

    const QModelIndex active
        = model.indexOf(model.find(QStringLiteral("cam/active")), PropertyModel::ValueColumn);
    QVERIFY(model.setData(active, Qt::Unchecked, Qt::CheckStateRole));
    QVERIFY(!camera.active());
    QCOMPARE(values.count(), 2);
    QCOMPARE(user.count(), 2);
    QCOMPARE(user.at(1).at(0).toString(), QStringLiteral("cam/active"));
    QCOMPARE(user.at(1).at(1), QVariant(false));

    // The object adjusts the value: the model shows the object's value (a
    // second, application, valueChanged) and the edit is announced once.
    const QModelIndex fov
        = model.indexOf(model.find(QStringLiteral("cam/fov")), PropertyModel::ValueColumn);
    QVERIFY(model.setData(fov, 75));
    QCOMPARE(camera.fov(), 74);
    QCOMPARE(model.find(QStringLiteral("cam/fov"))->value(), QVariant(74));
    QCOMPARE(values.count(), 4);
    QCOMPARE(user.count(), 3);
    QCOMPARE(user.at(2).at(0).toString(), QStringLiteral("cam/fov"));
    QCOMPARE(user.at(2).at(1), QVariant(75));
    QCOMPARE(user.at(2).at(2), QVariant(60));

    // What the object does on its own afterwards is not an edit.
    camera.setName(QStringLiteral("Top"));
    QCOMPARE(values.count(), 5);
    QCOMPARE(user.count(), 3);
}

// A slot of userValueChanged() replaces the tree: the object already holds the
// edited value and keeps working with the source.
void tst_QObjectPropertySource::treeReplacedByAUserValueHandlerStillReachesTheObject()
{
    PropertyModel model(PropertyGroup::create(QStringLiteral("root")));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    int replaced = 0;
    QObject::connect(&model, &PropertyModel::userValueChanged, &model,
        [&model, &replaced](const QString& path) {
            if (path == QStringLiteral("cam/name") && replaced++ == 0)
                model.setRoot(PropertyGroup::create(QStringLiteral("root")));
        });
    QObjectPropertySource source(&model);
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    QVERIFY(source.addObject(&camera));

    QVERIFY(model.setData(
        model.indexOf(model.find(QStringLiteral("cam/name")), PropertyModel::ValueColumn),
        QStringLiteral("Rim")));

    QCOMPARE(replaced, 1);
    QCOMPARE(camera.name(), QStringLiteral("Rim"));
    QVERIFY(!model.find(QStringLiteral("cam")));
    QVERIFY(!source.groupOf(&camera));

    // The object is still bound but has no properties in the new tree.
    camera.setName(QStringLiteral("Later"));
    QCOMPARE(camera.name(), QStringLiteral("Later"));
    QCOMPARE(source.objects(), QList<QObject*>({&camera}));
    QVERIFY(source.removeObject(&camera));
}

// The object refuses part of the value, so the source reads it back inside
// the original write; a slot of userValueChanged() replaces the tree after
// that nested write.
void tst_QObjectPropertySource::treeReplacedByAUserValueHandlerWhileTheObjectAdjustsTheValue()
{
    PropertyModel model(PropertyGroup::create(QStringLiteral("root")));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    QVERIFY(source.addObject(&camera));
    int replaced = 0;
    QObject::connect(&model, &PropertyModel::userValueChanged, &model,
        [&model, &replaced](const QString& path) {
            if (path == QStringLiteral("cam/fov") && replaced++ == 0)
                model.setRoot(PropertyGroup::create(QStringLiteral("root")));
        });
    QSignalSpy user(&model, &PropertyModel::userValueChanged);

    QVERIFY(model.setData(
        model.indexOf(model.find(QStringLiteral("cam/fov")), PropertyModel::ValueColumn), 75));

    QCOMPARE(replaced, 1);
    QCOMPARE(user.count(), 1);
    QCOMPARE(camera.fov(), 74); // odd values are refused by the object
    QVERIFY(!model.find(QStringLiteral("cam")));
    camera.setFov(90); // no property to follow it any more
    QCOMPARE(camera.fov(), 90);
}

void tst_QObjectPropertySource::modelWriteWithAReadBackSlotRemovingTheObject_data()
{
    QTest::addColumn<bool>("throughSetData");
    QTest::newRow("setValue") << false;
    QTest::newRow("setData") << true;
}

// The object adjusts the written value, so the source reads it back inside the
// write; a slot connected after the source reacts to that read-back by removing
// the object while the write is still running.
void tst_QObjectPropertySource::modelWriteWithAReadBackSlotRemovingTheObject()
{
    QFETCH(bool, throughSetData);
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    QVERIFY(source.addObject(&camera));
    int removals = 0;
    QObject::connect(
        &model, &PropertyModel::valueChanged, &model, [&source, &camera, &removals](const QString& path) {
            if (path == QStringLiteral("cam/fov") && source.removeObject(&camera))
                ++removals;
        });

    bool ok = false;
    if (throughSetData) {
        ok = model.setData(
            model.indexOf(model.find(QStringLiteral("cam/fov")), PropertyModel::ValueColumn), 75);
    } else {
        ok = model.setValue(QStringLiteral("cam/fov"), 75);
    }

    QVERIFY(ok);
    QCOMPARE(removals, 1);
    QCOMPARE(camera.fov(), 74); // odd values are refused by the object
    QVERIFY(!model.find(QStringLiteral("cam")));
    QVERIFY(source.objects().isEmpty());
    QVERIFY(!source.groupOf(&camera));
    QVERIFY(!source.removeObject(&camera));

    // The object is not synchronized any more.
    camera.setFov(90);
    camera.setName(QStringLiteral("Later"));
    QVERIFY(!model.find(QStringLiteral("cam")));

    // It can be added again and shows its current values.
    PropertyGroup* again = source.addObject(&camera);
    QVERIFY(again);
    QCOMPARE(again->path(), QStringLiteral("cam"));
    QCOMPARE(model.find(QStringLiteral("cam/fov"))->value(), QVariant(90));
    QCOMPARE(model.find(QStringLiteral("cam/name"))->value(), QVariant(QStringLiteral("Later")));
}

namespace {

enum class Leaving {
    SetterRemoves,
    SetterDeletes,
    NotifySlotRemoves,
    NotifySlotDeletes,
};

} // namespace

void tst_QObjectPropertySource::objectReactingToAModelWriteByLeaving_data()
{
    QTest::addColumn<int>("leaving");
    QTest::addColumn<bool>("throughSetData");
    const struct
    {
        const char* name;
        Leaving leaving;
    } rows[] = {
        {"setterRemoves", Leaving::SetterRemoves},
        {"setterDeletes", Leaving::SetterDeletes},
        {"notifySlotRemoves", Leaving::NotifySlotRemoves},
        {"notifySlotDeletes", Leaving::NotifySlotDeletes},
    };
    for (const auto& row : rows) {
        QTest::newRow(QByteArray(row.name).append("/setValue").constData())
            << int(row.leaving) << false;
        QTest::newRow(QByteArray(row.name).append("/setData").constData())
            << int(row.leaving) << true;
    }
}

// A model-side write reaches the object's setter; the setter itself, or a slot
// on the object's NOTIFY signal, removes the object from the source or deletes
// it before the write has returned.
void tst_QObjectPropertySource::objectReactingToAModelWriteByLeaving()
{
    QFETCH(int, leaving);
    QFETCH(bool, throughSetData);
    const bool deletes = Leaving(leaving) == Leaving::SetterDeletes
        || Leaving(leaving) == Leaving::NotifySlotDeletes;
    const bool inSetter = Leaving(leaving) == Leaving::SetterRemoves
        || Leaving(leaving) == Leaving::SetterDeletes;

    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    auto* object = new Reactive;
    const QPointer<Reactive> guard(object);
    const auto cleanup = qScopeGuard([&guard] { delete guard.data(); });
    object->setObjectName(QStringLiteral("r"));
    QVERIFY(source.addObject(object));
    const auto leave = [&source, object, deletes] {
        if (deletes)
            delete object;
        else
            source.removeObject(object);
    };
    if (inSetter)
        object->inSetter = leave;
    else
        QObject::connect(object, &Reactive::levelChanged, leave);

    const QString level = QStringLiteral("r/level");
    bool ok = false;
    if (throughSetData)
        ok = model.setData(model.indexOf(model.find(level), PropertyModel::ValueColumn), 5);
    else
        ok = model.setValue(level, 5);

    QVERIFY(ok);
    QVERIFY(!model.find(QStringLiteral("r")));
    QVERIFY(source.objects().isEmpty());
    if (deletes) {
        QVERIFY(guard.isNull());
        return;
    }
    QVERIFY(!guard.isNull());
    QCOMPARE(object->level(), 5);
    QVERIFY(!source.groupOf(object));
    QVERIFY(!source.removeObject(object));

    // Not synchronized any more; added again it shows the current value.
    object->inSetter = nullptr;
    object->setLevel(8);
    QVERIFY(!model.find(QStringLiteral("r")));
    PropertyGroup* again = source.addObject(object);
    QVERIFY(again);
    QCOMPARE(again->path(), QStringLiteral("r"));
    QCOMPARE(model.find(level)->value(), QVariant(8));
}

void tst_QObjectPropertySource::objectChangeWithSeveralPropertiesLeavingInTheFirstSlot_data()
{
    QTest::addColumn<bool>("destroy");
    QTest::newRow("remove") << false;
    QTest::newRow("delete") << true;
}

// One NOTIFY signal announces four properties; the slot of the first value
// change removes (or deletes) the object: the other three are not written to
// the removed group.
void tst_QObjectPropertySource::objectChangeWithSeveralPropertiesLeavingInTheFirstSlot()
{
    QFETCH(bool, destroy);
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    auto* recorder = new Recorder;
    const QPointer<Recorder> guard(recorder);
    const auto cleanup = qScopeGuard([&guard] { delete guard.data(); });
    recorder->setObjectName(QStringLiteral("rec"));
    QVERIFY(source.addObject(recorder));
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    bool left = false;
    QObject::connect(&model, &PropertyModel::valueChanged, &model,
        [&source, recorder, destroy, &left](const QString&) {
            if (left)
                return;
            left = true;
            if (destroy)
                delete recorder;
            else
                source.removeObject(recorder);
        });

    recorder->m_autostop = true;
    recorder->m_minutes = 20;
    recorder->m_saveToDisk = false;
    recorder->m_folder = QStringLiteral("x");
    emit recorder->changed();

    QVERIFY(left);
    QCOMPARE(values.count(), 1);
    QCOMPARE(values.at(0).at(0).toString(), QStringLiteral("rec/autostop"));
    QVERIFY(!model.find(QStringLiteral("rec")));
    QVERIFY(source.objects().isEmpty());
    QCOMPARE(guard.isNull(), destroy);
}

// The signal that notifies the title property also updates the group's title
// after the properties: a slot of the property's change removes the object.
void tst_QObjectPropertySource::titleNotificationAfterTheObjectLeft()
{
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    Sensor sensor;
    sensor.setObjectName(QStringLiteral("s"));
    QVERIFY(source.addObject(&sensor));
    int removals = 0;
    QObject::connect(
        &model, &PropertyModel::valueChanged, &model, [&source, &sensor, &removals](const QString& path) {
            if (path == QStringLiteral("s/label") && source.removeObject(&sensor))
                ++removals;
        });

    sensor.setLabel(QStringLiteral("Roof"));

    QCOMPARE(removals, 1);
    QVERIFY(!model.find(QStringLiteral("s")));
    QVERIFY(source.objects().isEmpty());
}

void tst_QObjectPropertySource::refreshWithAnObjectLeavingInASlot_data()
{
    QTest::addColumn<int>("trigger"); // the object whose refreshed value runs the slot
    QTest::addColumn<int>("victim"); // the object the slot removes or deletes
    QTest::addColumn<bool>("destroy");
    QTest::newRow("firstRemovesItself") << 0 << 0 << false;
    QTest::newRow("secondRemovesItself") << 1 << 1 << false;
    QTest::newRow("secondIsDeletedByItsOwnChange") << 1 << 1 << true;
    QTest::newRow("firstRemovesTheSecond") << 0 << 1 << false;
    QTest::newRow("firstRemovesTheThird") << 0 << 2 << false;
    QTest::newRow("firstDeletesTheThird") << 0 << 2 << true;
    QTest::newRow("thirdRemovesTheFirst") << 2 << 0 << false;
}

// refresh() copies the values of three objects; a slot of the value change of
// one of them removes (or deletes) one of the three. The others are refreshed.
void tst_QObjectPropertySource::refreshWithAnObjectLeavingInASlot()
{
    QFETCH(int, trigger);
    QFETCH(int, victim);
    QFETCH(bool, destroy);
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    const QStringList names {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    QPointer<Camera> cameras[3];
    const auto cleanup = qScopeGuard([&cameras] {
        for (const QPointer<Camera>& camera : cameras)
            delete camera.data();
    });
    for (int i = 0; i < 3; ++i) {
        cameras[i] = new Camera;
        cameras[i]->setObjectName(names.at(i));
        QVERIFY(source.addObject(cameras[i]));
    }
    // "frames" has no NOTIFY signal: only refresh() sees the new values.
    for (int i = 0; i < 3; ++i)
        cameras[i]->setFrames(11 * (i + 1));
    bool left = false;
    const QString triggerPath = names.at(trigger) + QStringLiteral("/frames");
    QObject::connect(&model, &PropertyModel::valueChanged, &model,
        [&](const QString& path) {
            if (left || path != triggerPath)
                return;
            left = true;
            if (destroy)
                delete cameras[victim].data();
            else
                source.removeObject(cameras[victim]);
        });

    source.refresh();

    QVERIFY(left);
    for (int i = 0; i < 3; ++i) {
        if (i == victim) {
            QVERIFY(!model.find(names.at(i)));
            continue;
        }
        QVERIFY(model.find(names.at(i)));
        QCOMPARE(model.find(names.at(i) + QStringLiteral("/frames"))->value(),
            QVariant(qint64(11 * (i + 1))));
    }
    QCOMPARE(source.objects().size(), 2);
}

// Removing an object other than the one being written leaves the write intact.
// The removed object was added first, so it sits before the written one.
void tst_QObjectPropertySource::otherObjectRemovedInsideAModelWrite()
{
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    Reactive other;
    other.setObjectName(QStringLiteral("b"));
    Reactive writer;
    writer.setObjectName(QStringLiteral("a"));
    QVERIFY(source.addObject(&other));
    QVERIFY(source.addObject(&writer));
    writer.inSetter = [&source, &other] { source.removeObject(&other); };

    QVERIFY(model.setValue(QStringLiteral("a/level"), 5));

    QCOMPARE(writer.level(), 5);
    QVERIFY(!model.find(QStringLiteral("b")));
    QVERIFY(model.find(QStringLiteral("a")));
    QCOMPARE(model.find(QStringLiteral("a/level"))->value(), QVariant(5));
    QCOMPARE(source.objects(), QList<QObject*>({&writer}));

    // The written object is still synchronized both ways; the removed one is not.
    writer.inSetter = nullptr;
    QVERIFY(model.setValue(QStringLiteral("a/level"), 7));
    QCOMPARE(writer.level(), 7);
    writer.setLevel(9);
    QCOMPARE(model.find(QStringLiteral("a/level"))->value(), QVariant(9));
    other.setLevel(3);
    QVERIFY(!model.find(QStringLiteral("b")));
}

// Guard: the same for a change that starts in the object and reaches several
// properties.
void tst_QObjectPropertySource::otherObjectRemovedInsideAnObjectChange()
{
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    Reactive other;
    other.setObjectName(QStringLiteral("other"));
    Recorder changing;
    changing.setObjectName(QStringLiteral("rec"));
    QVERIFY(source.addObject(&other));
    QVERIFY(source.addObject(&changing));
    QSignalSpy values(&model, &PropertyModel::valueChanged);
    bool removed = false;
    QObject::connect(&model, &PropertyModel::valueChanged, &model,
        [&source, &other, &removed](const QString& path) {
            if (!removed && path.startsWith(QStringLiteral("rec/"))) {
                removed = true;
                source.removeObject(&other);
            }
        });

    changing.m_autostop = true;
    changing.m_minutes = 20;
    changing.m_saveToDisk = false;
    changing.m_folder = QStringLiteral("x");
    emit changing.changed();

    QVERIFY(removed);
    QCOMPARE(values.count(), 4);
    QVERIFY(!model.find(QStringLiteral("other")));
    QCOMPARE(model.find(QStringLiteral("rec/minutes"))->value(), QVariant(20));
    QCOMPARE(model.find(QStringLiteral("rec/folder"))->value(), QVariant(QStringLiteral("x")));
    QCOMPARE(source.objects(), QList<QObject*>({&changing}));
}

// Guard: a slot connected before the source removes the object at the first
// change, again and again; nothing of the removed objects is left in the
// source, so adding the object once more gives it a fresh group every time.
void tst_QObjectPropertySource::objectRemovedByAnEarlierSlotLeavesNoRecord()
{
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource* current = nullptr;
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    int removals = 0;
    QObject::connect(&model, &PropertyModel::valueChanged, &model,
        [&current, &camera, &removals](const QString& path) {
            if (path == QStringLiteral("cam/name") && current && current->removeObject(&camera))
                ++removals;
        });
    QObjectPropertySource source(&model);
    current = &source;

    for (int round = 0; round < 20; ++round) {
        PropertyGroup* group = source.addObject(&camera);
        QVERIFY(group);
        QCOMPARE(group->path(), QStringLiteral("cam"));
        QCOMPARE(source.objects(), QList<QObject*>({&camera}));
        QVERIFY(model.setValue(
            QStringLiteral("cam/name"), QStringLiteral("Name %1").arg(round)));
        QCOMPARE(removals, round + 1);
        QVERIFY(!model.find(QStringLiteral("cam")));
        QVERIFY(source.objects().isEmpty());
        QVERIFY(!source.groupOf(&camera));
    }
}

namespace {

enum class SyncKind {
    ModelWrite,
    ObjectNotification,
    Refresh,
};

} // namespace

void tst_QObjectPropertySource::objectRemovedAndAddedAgainInsideASync_data()
{
    QTest::addColumn<int>("kind");
    QTest::newRow("modelWrite") << int(SyncKind::ModelWrite);
    QTest::newRow("objectNotification") << int(SyncKind::ObjectNotification);
    QTest::newRow("refresh") << int(SyncKind::Refresh);
}

// A slot removes the object and adds it again while the source is syncing it:
// afterwards there is one record, one group, synchronized both ways.
void tst_QObjectPropertySource::objectRemovedAndAddedAgainInsideASync()
{
    QFETCH(int, kind);
    PropertyModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QObjectPropertySource source(&model);
    Camera camera;
    camera.setObjectName(QStringLiteral("cam"));
    QVERIFY(source.addObject(&camera));
    // The path whose change runs the slot: for a model write the read-back of
    // the adjusted value, for the others the value copied from the object.
    const QString triggerPath = SyncKind(kind) == SyncKind::ModelWrite
        ? QStringLiteral("cam/fov")
        : SyncKind(kind) == SyncKind::ObjectNotification ? QStringLiteral("cam/name")
                                                          : QStringLiteral("cam/frames");
    int rounds = 0;
    QObject::connect(&model, &PropertyModel::valueChanged, &model,
        [&source, &camera, &rounds, triggerPath](const QString& path) {
            if (rounds > 0 || path != triggerPath)
                return;
            ++rounds;
            source.removeObject(&camera);
            source.addObject(&camera);
        });

    switch (SyncKind(kind)) {
    case SyncKind::ModelWrite:
        QVERIFY(model.setValue(QStringLiteral("cam/fov"), 75));
        break;
    case SyncKind::ObjectNotification:
        camera.setName(QStringLiteral("Renamed"));
        break;
    case SyncKind::Refresh:
        camera.setFrames(77);
        source.refresh();
        break;
    }

    QCOMPARE(rounds, 1);
    QCOMPARE(source.objects(), QList<QObject*>({&camera}));
    QVERIFY(source.groupOf(&camera));
    QCOMPARE(childIds(model.root()), QStringList({QStringLiteral("cam")}));
    QCOMPARE(model.find(QStringLiteral("cam/fov"))->value(), QVariant(camera.fov()));
    QCOMPARE(model.find(QStringLiteral("cam/name"))->value(), QVariant(camera.name()));
    QCOMPARE(model.find(QStringLiteral("cam/frames"))->value(), QVariant(camera.frames()));

    // Synchronized both ways.
    QVERIFY(model.setValue(QStringLiteral("cam/name"), QStringLiteral("ViaModel")));
    QCOMPARE(camera.name(), QStringLiteral("ViaModel"));
    camera.setName(QStringLiteral("ViaObject"));
    QCOMPARE(model.find(QStringLiteral("cam/name"))->value(), QVariant(QStringLiteral("ViaObject")));

    // One removal succeeds, the next finds nothing.
    QVERIFY(source.removeObject(&camera));
    QVERIFY(!source.removeObject(&camera));
    QVERIFY(source.objects().isEmpty());
    QVERIFY(!model.find(QStringLiteral("cam")));
}

QTEST_GUILESS_MAIN(tst_QObjectPropertySource)
#include "tst_qobjectpropertysource.moc"
