#include "PropertyPanel.h"

#include <QFont>
#include <QItemSelectionModel>
#include <QVBoxLayout>

namespace {

// Time after the last key stroke before the filter is applied.
constexpr int FilterDelayMs = 120;

} // namespace

PropertyPanel::PropertyPanel(QWidget* parent)
    : QWidget(parent)
{
    m_proxy = new qpb::PropertyFilterProxyModel(this);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search properties"));
    m_search->setClearButtonEnabled(true);

    m_tree = new qpb::PropertyTreeView;
    m_tree->setModel(m_proxy);

    auto* description = new QFrame;
    description->setFrameShape(QFrame::StyledPanel);
    description->setMinimumHeight(fontMetrics().lineSpacing() * 4);
    m_name = new QLabel;
    m_name->setObjectName(QStringLiteral("descriptionName"));
    m_name->setTextFormat(Qt::PlainText);
    QFont bold = m_name->font();
    bold.setBold(true);
    m_name->setFont(bold);
    m_toolTip = new QLabel;
    m_toolTip->setObjectName(QStringLiteral("descriptionToolTip"));
    m_toolTip->setWordWrap(true);
    auto* descriptionLayout = new QVBoxLayout(description);
    descriptionLayout->addWidget(m_name);
    descriptionLayout->addWidget(m_toolTip);
    descriptionLayout->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_search);
    layout->addWidget(m_tree, 1);
    layout->addWidget(description);

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(FilterDelayMs);
    connect(m_search, &QLineEdit::textChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_debounce, &QTimer::timeout, this, &PropertyPanel::applyFilter);

    // The tree creates its selection model in setModel() and replaces it when the
    // model changes, so connect to the one that exists now.
    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
        &PropertyPanel::updateDescription);
    connect(m_proxy, &QAbstractItemModel::dataChanged, this, &PropertyPanel::updateDescription);
    connect(m_proxy, &QAbstractItemModel::modelReset, this, &PropertyPanel::updateDescription);

    updateDescription();
}

void PropertyPanel::setModel(qpb::PropertyModel* model)
{
    m_model = model;
    m_proxy->setSourceModel(model);
    m_tree->expandAll();
    updateDescription();
}

qpb::PropertyModel* PropertyPanel::model() const
{
    return m_model;
}

void PropertyPanel::applyFilter()
{
    const QModelIndex current = m_tree->currentIndex();
    const QString currentPath = current.data(qpb::PropertyModel::PathRole).toString();

    m_filtering = true;
    m_proxy->setFilterFixedString(m_search->text());
    m_tree->expandAll();
    // When the current row is filtered away, the selection model moves the current
    // index to a neighbour. The description must show nothing instead of that row.
    const bool kept = m_proxy->rowCount() > 0
        && !m_proxy
                ->match(m_proxy->index(0, 0), qpb::PropertyModel::PathRole, currentPath, 1,
                    Qt::MatchExactly | Qt::MatchRecursive)
                .isEmpty();
    if (!currentPath.isEmpty() && !kept) {
        m_tree->selectionModel()->clearSelection();
        m_tree->selectionModel()->clearCurrentIndex();
    }
    m_filtering = false;

    updateDescription();
}

void PropertyPanel::updateDescription()
{
    if (m_filtering)
        return;

    const QModelIndex current = m_tree->currentIndex();
    if (!current.isValid()) {
        m_name->clear();
        m_toolTip->clear();
        return;
    }

    // Both columns of a row describe the same property; the name column has the
    // display name and the property's own tool tip.
    const QModelIndex nameIndex = current.siblingAtColumn(qpb::PropertyModel::NameColumn);
    m_name->setText(nameIndex.data(Qt::DisplayRole).toString());
    m_toolTip->setText(nameIndex.data(Qt::ToolTipRole).toString());
}
