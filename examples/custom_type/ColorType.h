#ifndef EXAMPLES_COLORTYPE_H
#define EXAMPLES_COLORTYPE_H

#include <qpb/Types.h>

#include <QColor>
#include <QToolButton>

// A custom property type registered entirely outside the library
// (docs/SPEC.md S2: at most 100 lines for ColorType.h + ColorType.cpp).

namespace example {

inline constexpr QLatin1StringView ColorTypeId {"example.color"};

// Registers ColorTypeId with qpb::TypeRegistry and qpb::EditorFactory.
// Call once at startup, before creating properties of that type.
void registerColorType();

// Editor: a button showing the color; clicking it opens a QColorDialog.
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
    void pickColor();

    QColor m_color;
};

} // namespace example

#endif // EXAMPLES_COLORTYPE_H
