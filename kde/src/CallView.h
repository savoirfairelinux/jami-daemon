#ifndef CALL_VIEW
#define CALL_VIEW

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QItemDelegate>

#include "lib/CallModel.h"
#include "lib/sflphone_const.h"
#include "lib/callmanager_interface_singleton.h"
#include "widgets/CallTreeItem.h"

class CallTreeItemDelegate : public QItemDelegate
{
   public:
      CallTreeItemDelegate() { }
      QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index ) const 
      {  
         Q_UNUSED(option)
         Q_UNUSED(index)
         return QSize(0,60); 
      }
};

typedef CallModel<CallTreeItem*,QTreeWidgetItem*> TreeWidgetCallModel;

class CallView : public QTreeWidget, public TreeWidgetCallModel {
   Q_OBJECT
   public:
      CallView(QWidget* parent =0,ModelType type = ActiveCall);
      bool selectItem(Call* item);
      Call* getCurrentItem();
      bool removeItem(Call* item);
      QWidget* getWidget();
      void setTitle(QString title);
      bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
      virtual QMimeData* mimeData( const QList<QTreeWidgetItem *> items) const;
      virtual Call* addCall(Call* call, Call* parent =0);
      virtual Call* addConference(const QString &confID);
      virtual bool conferenceChanged(const QString &confId, const QString &state);
      virtual void conferenceRemoved(const QString &confId);
      
   private:
      QTreeWidgetItem* extractItem(QString callId);
      QTreeWidgetItem* extractItem(QTreeWidgetItem* item);
      CallTreeItem* insertItem(QTreeWidgetItem* item, QTreeWidgetItem* parent=0);
      CallTreeItem* insertItem(QTreeWidgetItem* item, Call* parent);
      void clearArtefact(QTreeWidgetItem* item);

   protected:
      void dragEnterEvent(QDragEnterEvent *e) { e->accept(); }
      void dragMoveEvent(QDragMoveEvent *e) { e->accept(); }
      bool callToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
      bool phoneNumberToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
      bool contactToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
      
   public slots:
      void destroyCall(Call* toDestroy);
      void itemDoubleClicked(QTreeWidgetItem* item, int column);
      void itemClicked(QTreeWidgetItem* item, int column);
      //D-Bus handling
//       void callStateChangedSignal(const QString& callId, const QString& state);
//       void incomingCallSignal(const QString& accountId, const QString& callId);
      void conferenceCreatedSignal(const QString& confId);
      void conferenceChangedSignal(const QString& confId, const QString& state);
      void conferenceRemovedSignal(const QString& confId);
//       void incomingMessageSignal(const QString& accountId, const QString& message);
//       void voiceMailNotifySignal(const QString& accountId, int count);
      
   public slots:
      void clearHistory();

   signals:
      void itemChanged(Call*);
      
};
#endif
