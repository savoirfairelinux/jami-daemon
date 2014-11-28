#include <QtCore/QIdentityProxyModel>

class GtkAccessProxyModel : public QIdentityProxyModel
{
	// Q_OBJECT
public:
	QModelIndex indexFromId(int row, int column, quintptr id) const;
};