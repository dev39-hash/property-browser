#ifndef QPB_LANGUAGECHANGE_P_H
#define QPB_LANGUAGECHANGE_P_H

// Internal header, not part of the public API.

#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE
class QCoreApplication;
QT_END_NAMESPACE

namespace qpb::detail {

// Turns the QEvent::LanguageChange that the application object receives when a
// translator is installed or removed into a signal, with one application-wide
// event filter for all models (README.md, "Translations"; D57).
class LanguageChangeNotifier : public QObject
{
    Q_OBJECT

public:
    // nullptr without a QCoreApplication, or outside the application's thread.
    static LanguageChangeNotifier* instance();

signals:
    void languageChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    explicit LanguageChangeNotifier(QCoreApplication* application);
};

} // namespace qpb::detail

#endif // QPB_LANGUAGECHANGE_P_H
