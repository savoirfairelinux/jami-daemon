#ifndef HISTORY_DOCK_H
#define HISTORY_DOCK_H

#include <QtGui/QDockWidget>
#include <QDebug>
#include <QDate>
#include <QtGui/QTreeWidgetItem>
#include <QtCore/QString>

class QTreeWidget;
class KLineEdit;
class QComboBox;
class QLabel;
class KDateWidget;
class QCheckBox;
class QPushButton;
class QDate;
class HistoryTreeItem;

typedef QList<HistoryTreeItem*> HistoryList;

class HistoryDock : public QDockWidget {
   Q_OBJECT
public:
   HistoryDock(QWidget* parent);
   virtual ~HistoryDock();
private:
   enum SortBy {
      Date       = 0,
      Name       = 1,
      Popularity = 2,
      Duration   = 3
   };

   void updateLinkedDate(KDateWidget* item, QDate& prevDate, QDate& newDate);
   QString getIdentity(HistoryTreeItem* item);
   
   QTreeWidget*  m_pItemView;
   KLineEdit*    m_pFilterLE;
   QComboBox*    m_pSortByCBB;
   QLabel*       m_pSortByL;
   QLabel*       m_pFromL;
   QLabel*       m_pToL;
   KDateWidget*  m_pFromDW;
   KDateWidget*  m_pToDW;
   QCheckBox*    m_pAllTimeCB;
   QPushButton*  m_pLinkPB;
   HistoryList   m_pHistory;
   QDate         m_pCurrentFromDate;
   QDate         m_pCurrentToDate;
public slots:
   void enableDateRange(bool enable);
private slots:
   void filter(QString text);
   void updateLinkedFromDate(QDate date);
   void updateLinkedToDate(QDate date);
   void reload();
   void updateContactInfo();
};

#endif