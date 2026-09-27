#pragma once

#include <QColor>
#include <QToolButton>

// The application's own color property type (written against the public qpb API only).
inline const QString ColorType = QStringLiteral("trial.color");
void registerColorType();

class ColorButton : public QToolButton
{
    Q_OBJECT
public:
    explicit ColorButton(QWidget* parent = nullptr);
    QColor color() const
    {
        return m_color;
    }
    void setColor(const QColor& color);

private:
    QColor m_color;
};
