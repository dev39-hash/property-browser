#include "ColorType.h"

#include <qpb/qpb.h>

#include <QColorDialog>
#include <QPainter>
#include <QPixmap>
#include <QStyleOptionViewItem>

ColorButton::ColorButton(QWidget* parent)
    : QToolButton(parent)
{
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(this, &QToolButton::clicked, this, [this] {
        QColor picked;
        {
            qpb::EditorDialogScope scope(this);
            picked = QColorDialog::getColor(m_color, this);
        }
        if (picked.isValid()) {
            setColor(picked);
            qpb::EditorFactory::notifyCommit(this);
        }
    });
}

void ColorButton::setColor(const QColor& color)
{
    m_color = color;
    QPixmap swatch(14, 14);
    swatch.fill(color);
    setIcon(swatch);
    setText(color.name());
}

void registerColorType()
{
    if (qpb::TypeRegistry::global().contains(ColorType))
        return;
    qpb::TypeHandler type;
    type.displayText
        = [](const QVariant& v, const qpb::Property&) { return v.value<QColor>().name(); };
    qpb::TypeRegistry::global().registerType<QColor>(ColorType, type);

    qpb::EditorHandler editor;
    editor.createEditor
        = [](QWidget* parent, const qpb::Property&) { return new ColorButton(parent); };
    editor.setEditorData = [](QWidget* w, const QVariant& v, const qpb::Property&) {
        static_cast<ColorButton*>(w)->setColor(v.value<QColor>());
    };
    editor.editorData = [](QWidget* w, const qpb::Property&) {
        return QVariant::fromValue(static_cast<ColorButton*>(w)->color());
    };
    editor.paint = [](QPainter* p, const QStyleOptionViewItem& option, const QVariant& v,
                       const qpb::Property&) {
        const QRect r = option.rect.adjusted(2, 3, 0, -3);
        p->fillRect(QRect(r.topLeft(), QSize(r.height(), r.height())), v.value<QColor>());
        p->drawText(
            r.adjusted(r.height() + 4, 0, 0, 0), Qt::AlignVCenter, v.value<QColor>().name());
    };
    qpb::EditorFactory::global().registerEditor(ColorType, editor);
}
