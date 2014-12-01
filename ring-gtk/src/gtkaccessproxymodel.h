#ifndef __GTK_ACCESS_PROXY_MODEL_H__
#define __GTK_ACCESS_PROXY_MODEL_H__

#include <QtCore/QIdentityProxyModel>

class GtkAccessProxyModel : public QIdentityProxyModel
{
	// Q_OBJECT
public:
	QModelIndex indexFromId(int row, int column, quintptr id) const;
};

#endif /* __GTK_ACCESS_PROXY_MODEL_H__ */