#ifndef QPB_TRANSLATIONS_H
#define QPB_TRANSLATIONS_H

#include <qpb/qpbglobal.h>

#include <QtCore/qlocale.h>

QT_BEGIN_NAMESPACE
class QTranslator;
QT_END_NAMESPACE

namespace qpb {

// qpb's own translations (README.md, "Translations"). Since 1.8.
//
// Loads qpb's translation for locale into translator, trying the locale's UI
// languages in order, so "ja-JP" finds Japanese. Returns false when qpb has no
// translation for them; qpb's texts then stay English, the source language.
// The application owns the translator and installs it:
//
//   QTranslator qpbTranslator;
//   if (qpb::loadTranslation(qpbTranslator, QLocale()))
//       app.installTranslator(&qpbTranslator);
//
// 1.8 ships English (plural forms only), Japanese and Vietnamese; they are
// embedded in qpb::core, so there is nothing to deploy.
QPB_CORE_EXPORT bool loadTranslation(QTranslator& translator, const QLocale& locale = QLocale());

} // namespace qpb

#endif // QPB_TRANSLATIONS_H
