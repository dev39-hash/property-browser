#pragma once

#include <QString>

// An alternative editor for Int properties, chosen per property with
// IntBuilder::editor(SliderEditor) (written against the public qpb API only).
inline const QString SliderEditor = QStringLiteral("trial.slider");
void registerSliderEditor();
