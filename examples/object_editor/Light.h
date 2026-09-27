#pragma once

#include <QObject>

// An application object whose Q_PROPERTYs are edited through
// qpb::QObjectPropertySource. Q_CLASSINFO adds ranges and display names, and
// (1.3) titles the group with the light's name.
class Light : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("qpb:title", "name")
    Q_CLASSINFO("qpb:intensity", "min=0;max=10;step=0.1;displayName=Intensity")
    Q_CLASSINFO("qpb:photons", "min=0;suffix= photons")
    Q_CLASSINFO("qpb:profile", "type=filepath;filter=IES profiles (*.ies)")
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(Kind kind READ kind WRITE setKind NOTIFY changed)
    Q_PROPERTY(double intensity READ intensity WRITE setIntensity NOTIFY changed)
    Q_PROPERTY(qint64 photons READ photons WRITE setPhotons NOTIFY changed)
    Q_PROPERTY(QString profile READ profile WRITE setProfile NOTIFY changed)
    // Maintained by the light itself: live (1.3), never undone or saved.
    Q_PROPERTY(qint64 emitted READ emitted NOTIFY emittedChanged)

public:
    enum Kind { Point, Spot, Area };
    Q_ENUM(Kind)

    using QObject::QObject;

    QString name() const
    {
        return m_name;
    }
    void setName(const QString& name)
    {
        set(m_name, name);
    }
    bool isEnabled() const
    {
        return m_enabled;
    }
    void setEnabled(bool enabled)
    {
        set(m_enabled, enabled);
    }
    Kind kind() const
    {
        return m_kind;
    }
    void setKind(Kind kind)
    {
        set(m_kind, kind);
    }
    double intensity() const
    {
        return m_intensity;
    }
    void setIntensity(double intensity)
    {
        set(m_intensity, intensity);
    }
    qint64 photons() const
    {
        return m_photons;
    }
    void setPhotons(qint64 photons)
    {
        set(m_photons, photons);
    }
    QString profile() const
    {
        return m_profile;
    }
    qint64 emitted() const
    {
        return m_emitted;
    }
    void addEmitted(qint64 photons)
    {
        m_emitted += photons;
        emit emittedChanged();
    }
    void setProfile(const QString& profile)
    {
        set(m_profile, profile);
    }

signals:
    void changed();
    void emittedChanged();

private:
    template <class T> void set(T& member, const T& value)
    {
        if (member != value) {
            member = value;
            emit changed();
        }
    }

    QString m_name = "Key light";
    bool m_enabled = true;
    Kind m_kind = Spot;
    double m_intensity = 1.5;
    qint64 m_photons = 5'000'000'000;
    QString m_profile;
    qint64 m_emitted = 0;
};
