#ifndef PROPERTYPANEL_H
#define PROPERTYPANEL_H

#include <qpb/qpb.h>

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QWidget>

// A property tree with a search box above it and a description frame below it,
// built from the public qpb API only.
//
// - The search box filters the tree by property name. The text is applied
//   shortly after the last key stroke (a single-shot timer), so typing fast does
//   not rebuild the tree on every letter; the matches are then expanded.
// - The description frame shows the display name (bold) and the tool tip of the
//   current row, the name alone when the row has no tool tip. It is empty when
//   there is no current row, also when the filter has hidden it.
class PropertyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget* parent = nullptr);

    // The model must outlive the panel (or be replaced by another setModel call).
    void setModel(qpb::PropertyModel* model);
    qpb::PropertyModel* model() const;

    qpb::PropertyTreeView* treeView() const { return m_tree; }
    QLineEdit* searchEdit() const { return m_search; }

private slots:
    void applyFilter();
    void updateDescription();

private:
    qpb::PropertyModel* m_model = nullptr;
    qpb::PropertyFilterProxyModel* m_proxy = nullptr;
    QLineEdit* m_search = nullptr;
    qpb::PropertyTreeView* m_tree = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_toolTip = nullptr;
    QTimer m_debounce;
    // True while applyFilter() changes the filter: the tree then moves its current
    // row on its own, which the description must not follow.
    bool m_filtering = false;
};

#endif // PROPERTYPANEL_H
