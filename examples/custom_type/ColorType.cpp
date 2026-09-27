#include "ColorType.h"

#include <qpb/qpb.h>

#include <QColorDialog>
#include <QPainter>
#include <QPixmap>
#include <QStyleOptionViewItem>

namespace example {

ColorButton::ColorButton(QWidget* parent)
    : QToolButton(parent)
{
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(this, &QToolButton::clicked, this, &ColorButton::pickColor);
}

void ColorButton::setColor(const QColor& color)
{
    m_color = color;
    QPixmap swatch(16, 16);
    swatch.fill(color);
    setIcon(swatch);
    setText(color.name(QColor::HexArgb));
}

void ColorButton::pickColor()
{
    QColor picked;
    {
        // Keeps the view from closing the editor while the dialog has focus.
        qpb::EditorDialogScope scope(this);
        picked = QColorDialog::getColor(m_color, this, QString(), QColorDialog::ShowAlphaChannel);
    }
    if (!picked.isValid())
        return;
    setColor(picked);
    qpb::EditorFactory::notifyCommit(this);
}

void registerColorType()
{
    qpb::TypeHandler type;
    type.displayText = [](const QVariant& value, const qpb::Property&) {
        return value.value<QColor>().name(QColor::HexArgb);
    };
    type.validate = [](const QVariant& value, const qpb::Property&) {
        return value.value<QColor>().isValid()
            ? qpb::ValidationResult::valid()
            : qpb::ValidationResult::error(QObject::tr("Invalid color"));
    };
    qpb::TypeRegistry::global().registerType<QColor>(ColorTypeId, type);

    qpb::EditorHandler editor;
    editor.createEditor
        = [](QWidget* parent, const qpb::Property&) { return new ColorButton(parent); };
    editor.setEditorData = [](QWidget* w, const QVariant& value, const qpb::Property&) {
        static_cast<ColorButton*>(w)->setColor(value.value<QColor>());
    };
    editor.editorData = [](QWidget* w, const qpb::Property&) {
        return QVariant::fromValue(static_cast<ColorButton*>(w)->color());
    };
    editor.paint = [](QPainter* painter, const QStyleOptionViewItem& option, const QVariant& value,
                       const qpb::Property&) {
        const QRect swatch = option.rect.adjusted(2, 2, 0, -2);
        painter->fillRect(
            swatch.x(), swatch.y(), swatch.height(), swatch.height(), value.value<QColor>());
        painter->drawText(swatch.adjusted(swatch.height() + 4, 0, 0, 0), Qt::AlignVCenter,
            value.value<QColor>().name(QColor::HexArgb));
    };
    qpb::EditorFactory::global().registerEditor(ColorTypeId, editor);
}

} // namespace example
