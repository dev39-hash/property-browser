#include <qpb/Translations.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qpointer.h>
#include <QtCore/qthread.h>
#include <QtCore/qtranslator.h>

#include "LanguageChange_p.h"

// The .qm files of qpb/translations, embedded in qpb::core. Static builds must
// register them explicitly, and Q_INIT_RESOURCE cannot be used in a namespace.
static void qpbInitTranslationResources()
{
    Q_INIT_RESOURCE(qpb_translations);
}

namespace qpb {

bool loadTranslation(QTranslator& translator, const QLocale& locale)
{
    static const bool registered = (qpbInitTranslationResources(), true);
    Q_UNUSED(registered)
    return translator.load(locale, QStringLiteral("qpb"), QStringLiteral("_"),
        QStringLiteral(":/qpb/translations"));
}

namespace detail {

LanguageChangeNotifier* LanguageChangeNotifier::instance()
{
    static QPointer<LanguageChangeNotifier> notifier; // dies with the application
    QCoreApplication* application = QCoreApplication::instance();
    if (!application || QThread::currentThread() != application->thread())
        return nullptr;
    if (!notifier)
        notifier = new LanguageChangeNotifier(application);
    return notifier;
}

LanguageChangeNotifier::LanguageChangeNotifier(QCoreApplication* application)
    : QObject(application)
{
    application->installEventFilter(this);
}

bool LanguageChangeNotifier::eventFilter(QObject* watched, QEvent* event)
{
    // An application-wide filter sees the events of every object: only the
    // application's own LanguageChange counts (widgets get one each as well).
    if (event->type() == QEvent::LanguageChange && watched == parent())
        emit languageChanged();
    return QObject::eventFilter(watched, event);
}

} // namespace detail
} // namespace qpb
