#include "SliderEditor.h"

#include <qpb/qpb.h>

#include <QSlider>

void registerSliderEditor()
{
    if (qpb::EditorFactory::global().contains(SliderEditor))
        return;
    qpb::EditorHandler editor;
    editor.createEditor = [](QWidget* parent, const qpb::Property&) {
        auto* slider = new QSlider(Qt::Horizontal, parent);
        slider->setAutoFillBackground(true);
        return slider;
    };
    editor.applyAttributes = [](QWidget* w, const qpb::Property& p) {
        auto* slider = static_cast<QSlider*>(w);
        slider->setRange(p.attribute(qpb::Attr::Minimum, 0).toInt(),
            p.attribute(qpb::Attr::Maximum, 100).toInt());
    };
    editor.setEditorData = [](QWidget* w, const QVariant& v, const qpb::Property&) {
        static_cast<QSlider*>(w)->setValue(v.toInt());
    };
    editor.editorData
        = [](QWidget* w, const qpb::Property&) { return static_cast<QSlider*>(w)->value(); };
    qpb::EditorFactory::global().registerEditor(SliderEditor, editor);
}
