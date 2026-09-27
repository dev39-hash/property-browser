#ifndef QPB_WIDGETS_P_H
#define QPB_WIDGETS_P_H

// Internal header, not part of the public API.

#include <qpb/qpbglobal.h>

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>

QT_BEGIN_NAMESPACE
class QAbstractItemModel;
class QModelIndex;
class QWidget;
QT_END_NAMESPACE

namespace qpb {

class Property;
class PropertyModel;

namespace detail {

// Carries EditorFactory::notifyCommit() to the delegates (and later form
// views) that own the editor.
class CommitNotifier : public QObject
{
    Q_OBJECT

public:
    static CommitNotifier* instance();

signals:
    void commitRequested(QWidget* editor);
};

// Editors opened by a PropertyDelegate carry this dynamic property (the
// delegate's address) so events from their child widgets can be mapped back.
inline constexpr char EditorOwnerProperty[] = "_qpb_editorOwner";

// The (persistent) index an editor was opened for (dynamic property).
inline constexpr char EditorIndexProperty[] = "_qpb_editorIndex";

// Number of EditorDialogScope objects alive for an editor (dynamic property).
inline constexpr char DialogDepthProperty[] = "_qpb_dialogDepth";

// True while an EditorDialogScope exists for editor or one of its ancestors.
bool isShowingDialog(const QWidget* editor);

// The property behind a (possibly proxied) index, or nullptr.
const Property* propertyOf(const QModelIndex& index);

// Maps a (possibly proxied) index to its PropertyModel and source index.
// Returns nullptr if no PropertyModel is found at the bottom of the proxy chain.
PropertyModel* propertyModelOf(const QModelIndex& index, QModelIndex* sourceIndex = nullptr);

} // namespace detail
} // namespace qpb

#endif // QPB_WIDGETS_P_H
