#include "widgets_p.h"

#include <qpb/PropertyModel.h>

#include <QtCore/qabstractproxymodel.h>
#include <QtWidgets/qwidget.h>

namespace qpb::detail {

CommitNotifier* CommitNotifier::instance()
{
    static CommitNotifier notifier;
    return &notifier;
}

bool isShowingDialog(const QWidget* editor)
{
    for (const QWidget* widget = editor; widget; widget = widget->parentWidget()) {
        if (widget->property(DialogDepthProperty).toInt() > 0)
            return true;
    }
    return false;
}

const Property* propertyOf(const QModelIndex& index)
{
    return index.data(PropertyModel::PropertyRole).value<const Property*>();
}

PropertyModel* propertyModelOf(const QModelIndex& index, QModelIndex* sourceIndex)
{
    QModelIndex current = index;
    const QAbstractItemModel* model = index.model();
    while (const auto* proxy = qobject_cast<const QAbstractProxyModel*>(model)) {
        current = proxy->mapToSource(current);
        model = proxy->sourceModel();
    }
    auto* propertyModel = qobject_cast<PropertyModel*>(const_cast<QAbstractItemModel*>(model));
    if (propertyModel && sourceIndex)
        *sourceIndex = current;
    return propertyModel;
}

} // namespace qpb::detail
