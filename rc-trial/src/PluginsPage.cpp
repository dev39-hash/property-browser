#include "PluginsPage.h"

#include <QLineEdit>
#include <QVBoxLayout>

PluginsPage::PluginsPage(QWidget* parent)
    : QWidget(parent)
    , m_model(qpb::PropertyGroup::create("Plugins"))
    , m_view(new qpb::PropertyTreeView)
    , m_search(new QLineEdit)
{
    m_proxy.setSourceModel(&m_model);
    m_proxy.setRecursiveFilteringEnabled(true);
    m_proxy.setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_view->setMode(qpb::PropertyTreeView::Mode::List);
    m_view->setModel(&m_proxy);
    m_search->setPlaceholderText(tr("Search properties"));
    m_search->setClearButtonEnabled(true);
    connect(
        m_search, &QLineEdit::textChanged, &m_proxy, &QSortFilterProxyModel::setFilterFixedString);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_search);
    layout->addWidget(m_view);
}

void PluginsPage::load(const PluginManifest& manifest)
{
    if (m_model.root()->child(manifest.name))
        return;
    auto& group = m_model.root()->addGroup(manifest.name);
    group.addBool("enabled", false);
    group.addDouble("threshold", 0.5).range(0.0, 1.0).decimals(2).step(0.05);
    group.addEnum("mode", manifest.modes, 0);
    group.addFilePath("output", {}).dialogMode(qpb::FileMode::Save);
}

void PluginsPage::unload(const QString& name)
{
    m_model.root()->remove(name);
}
