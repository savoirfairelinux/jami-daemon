//Parent
#include "gtkaccessproxymodel.h"

QModelIndex GtkAccessProxyModel::indexFromId (int row, int column, quintptr id) const
{
	return QAbstractItemModel::createIndex(row, column, id);
}