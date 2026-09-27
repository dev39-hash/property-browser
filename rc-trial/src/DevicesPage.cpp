#include "DevicesPage.h"

#include <QVBoxLayout>

Device::Device(const QString& name, qint64 capacity, QObject* parent)
    : QObject(parent)
    , m_name(name)
    , m_capacity(capacity)
{
    setObjectName(name.toLower().replace(QLatin1Char(' '), QLatin1Char('_')));
}

void Device::setName(const QString& name)
{
    if (m_name != name) {
        m_name = name;
        emit changed();
    }
}

void Device::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (!enabled)
        m_mode = Idle;
    emit changed();
}

void Device::setMode(Mode mode)
{
    if (!m_enabled)
        mode = Idle;
    if (m_mode != mode) {
        m_mode = mode;
        emit changed();
    }
}

void Device::setGain(double gain)
{
    if (!qFuzzyCompare(m_gain, gain)) {
        m_gain = gain;
        emit changed();
    }
}

void Device::setRecordDir(const QString& directory)
{
    if (m_recordDir != directory) {
        m_recordDir = directory;
        emit changed();
    }
}

void Device::record(qint64 bytes)
{
    m_used = qMin(m_capacity, m_used + bytes);
    emit usedChanged();
}

DevicesPage::DevicesPage(QWidget* parent)
    : QWidget(parent)
    , m_model(qpb::PropertyGroup::create("Devices"))
    , m_source(&m_model)
    , m_form(new qpb::PropertyFormView)
{
    m_form->setModel(&m_model);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_form);
}

DevicesPage::~DevicesPage()
{
    qDeleteAll(devices());
}

Device* DevicesPage::addDevice(const QString& name, qint64 capacity)
{
    auto* device = new Device(name, capacity);
    m_devices << device;
    m_source.addObject(device);
    return device;
}

QList<Device*> DevicesPage::devices() const
{
    QList<Device*> result;
    for (const QPointer<Device>& device : m_devices) {
        if (device)
            result << device;
    }
    return result;
}
