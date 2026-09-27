// QObjectPropertySource (docs/PLAN.md M6.1, SPEC 4.9).

#include <qpb/qpbcore.h>

#include <QAbstractItemModelTester>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>

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

QTEST_GUILESS_MAIN(tst_QObjectPropertySource)
#include "tst_qobjectpropertysource.moc"
