#pragma once

#include <qpb/qpb.h>

#include <QList>
#include <QPointer>
#include <QWidget>

// A recording device of the application, a plain QObject with Q_PROPERTYs
// (round 4, 1.2: shown through qpb::QObjectPropertySource).
class Device : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("qpb:properties", "name,enabled,mode,gain,recordDir,capacity,used,firmware")
    Q_CLASSINFO("qpb:title", "name") // round 5 (1.3): F8
    Q_CLASSINFO("qpb:gain", "min=0;max=24;step=0.5;suffix= dB;displayName=Input gain")
    Q_CLASSINFO("qpb:recordDir", "type=dirpath;displayName=Recording folder")
    Q_CLASSINFO("qpb:capacity", "suffix= B")
    Q_CLASSINFO("qpb:used", "suffix= B;toolTip=Updated while recording")
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY changed)
    Q_PROPERTY(double gain READ gain WRITE setGain NOTIFY changed)
    Q_PROPERTY(QString recordDir READ recordDir WRITE setRecordDir NOTIFY changed)
    Q_PROPERTY(qint64 capacity READ capacity CONSTANT)
    Q_PROPERTY(qint64 used READ used NOTIFY usedChanged)
    Q_PROPERTY(QString firmware READ firmware CONSTANT)

public:
    enum Mode { Idle, Recording, Streaming };
    Q_ENUM(Mode)

    Device(const QString& name, qint64 capacity, QObject* parent = nullptr);

    QString name() const
    {
        return m_name;
    }
    void setName(const QString& name);
    bool isEnabled() const
    {
        return m_enabled;
    }
    void setEnabled(bool enabled);
    Mode mode() const
    {
        return m_mode;
    }
    // A disabled device cannot record: the mode stays Idle.
    void setMode(Mode mode);
    double gain() const
    {
        return m_gain;
    }
    void setGain(double gain);
    QString recordDir() const
    {
        return m_recordDir;
    }
    void setRecordDir(const QString& directory);
    qint64 capacity() const
    {
        return m_capacity;
    }
    qint64 used() const
    {
        return m_used;
    }
    QString firmware() const
    {
        return QStringLiteral("2.4.1");
    }

    // Simulates recorded data arriving.
    void record(qint64 bytes);

signals:
    void changed();
    void usedChanged();

private:
    QString m_name;
    bool m_enabled = true;
    Mode m_mode = Idle;
    double m_gain = 6.0;
    QString m_recordDir;
    qint64 m_capacity = 0;
    qint64 m_used = 0;
};

// Reference scenario 4 (round 4): application objects edited in a form.
class DevicesPage : public QWidget
{
    Q_OBJECT
public:
    explicit DevicesPage(QWidget* parent = nullptr);
    ~DevicesPage() override;

    Device* addDevice(const QString& name, qint64 capacity);
    QList<Device*> devices() const;
    qpb::PropertyModel& model()
    {
        return m_model;
    }
    qpb::QObjectPropertySource& source()
    {
        return m_source;
    }
    qpb::PropertyFormView* form() const
    {
        return m_form;
    }

private:
    qpb::PropertyModel m_model;
    qpb::QObjectPropertySource m_source;
    qpb::PropertyFormView* m_form;
    QList<QPointer<Device>> m_devices;
};
