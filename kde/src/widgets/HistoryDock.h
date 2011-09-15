#ifndef HISTORY_DOCK_H
#define HISTORY_DOCK_H

#include <QtGui/QDockWidget>

class QTreeWidget;
class KLineEdit;
class QComboBox;
class QLabel;

class HistoryDock : public QDockWidget {
   Q_OBJECT
public:
   HistoryDock(QWidget* parent);
   virtual ~HistoryDock();
private:
   QTreeWidget*  m_pItemView;
   KLineEdit*    m_pFilterLE;
   QComboBox*    m_pSortByCBB;
   QLabel*       m_pSortByL;
};

#endif