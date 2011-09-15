#ifndef HISTORY_DOCK_H
#define HISTORY_DOCK_H

#include <QtGui/QDockWidget>

class QTreeWidget;
class KLineEdit;
class QComboBox;
class QLabel;
class KDateWidget;
class QCheckBox;

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
   QLabel*       m_pFromL;
   QLabel*       m_pToL;
   KDateWidget*  m_pFromDW;
   KDateWidget*  m_pToDW;
   QCheckBox*    m_pAllTimeCB;
public slots:
   void enableDateRange(bool enable);
};

#endif