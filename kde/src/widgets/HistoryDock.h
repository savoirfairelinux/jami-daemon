#ifndef HISTORY_DOCK_H
#define HISTORY_DOCK_H

#include <QtGui/QDockWidget>
#include <QtGui/QTreeWidget>
#include <QtCore/QDate>

//Qt
class QTreeWidgetItem;
class QString;
class QTreeWidget;
class QComboBox;
class QLabel;
class QCheckBox;
class QPushButton;

//KDE
class KLineEdit;
class KDateWidget;

//SFLPhone
class HistoryTreeItem;
class HistoryTree;

typedef QList<HistoryTreeItem*> HistoryList;

class HistoryDock : public QDockWidget {
   Q_OBJECT
public:
   friend class KeyPressEater;
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
   HistoryTree*  m_pItemView;
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
   virtual void keyPressEvent(QKeyEvent* event);
private slots:
   void filter(QString text);
   void updateLinkedFromDate(QDate date);
   void updateLinkedToDate(QDate date);
   void reload();
   void updateContactInfo();
};

class HistoryTree : public QTreeWidget {
   Q_OBJECT
public:
   HistoryTree(QWidget* parent) : QTreeWidget(parent) {}
   virtual QMimeData* mimeData( const QList<QTreeWidgetItem *> items) const;
   bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
};

class KeyPressEater : public QObject
{
   Q_OBJECT
public:
   KeyPressEater(HistoryDock* parent) : QObject(parent) {
      m_pDock =  parent;
   }
protected:
   bool eventFilter(QObject *obj, QEvent *event);
private:
   HistoryDock* m_pDock;
};

#endif