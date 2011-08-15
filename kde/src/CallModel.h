/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Valle <emmanuel.lepage@savoirfairelinux.com >*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/ 

#ifndef CALL_MODEL
#define CALL_MODEL

#include <QObject>
#include <QHash>
#include <QTreeWidgetItem>
#include <QVector>
#include <QTreeWidget>
#include <QItemDelegate>
#include <QDragEnterEvent>

#include "CallTreeItem.h"
#include "Call.h"
#include "dbus/metatypes.h"

/** Note from the author: It was previously done by a QAbstractModel + QTreeView, but the sip-call use case is incompatible 
 *  with the MVC model. The MVC never got to a point were it was bug-free and the code was getting dirty. The QTreeWidget
 *  solution may be less "clean" than MVC, but is 3 time smaller and easier to improve (in fact, possible to improve).
 *  
 *  This model is the view itself (private inheritance) so drag and drop can interact directly with the model without cross
 *  layer hack. This call merge the content of 4 previous classes (CallTreeModel, CallTreeView, CallList and most of the 
 *  previous CallTreeItem).
 */

struct InternalCallModelStruct {
   InternalCallModelStruct* parent;
   CallTreeItem* call;
   Call* call_real;
   QTreeWidgetItem* treeItem;
   QList<InternalCallModelStruct*> children;
   bool conference;
};

struct InternalCallModelStruct;


class CallTreeItemDelegate : public QItemDelegate
{
   public:
      CallTreeItemDelegate() { }
      QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index ) const { return QSize(0,60); }
};



class CallModel : private QTreeWidget {
   Q_OBJECT
      public:
      enum ModelType {
         ActiveCall,
         History,
         Address
      };
      
      CallModel(ModelType type, QWidget* parent =0);
      Call* addCall(Call* call, Call* parent =0);
      int size();
      Call* findCallByCallId(QString callId);
      QList<Call*> getCallList();
      bool selectItem(Call* item);
      Call* getCurrentItem();
      bool removeItem(Call* item);
      QWidget* getWidget();
      void setTitle(QString title);
      bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
      QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const;
      
      Call* addDialingCall(const QString & peerName = "", QString account = "");
      Call* addIncomingCall(const QString & callId/*, const QString & from, const QString & account*/);
      Call* addRingingCall(const QString & callId);
      bool createConferenceFromCall(Call* call1, Call* call2);
      Call* addConference(const QString &confID);
      bool mergeConferences(Call* conf1, Call* conf2);
      bool addParticipant(Call* call2, Call* conference);
      bool detachParticipant(Call* call);
      void conferenceChanged(const QString &confId, const QString &state);
      void conferenceRemoved(const QString &confId);
      
      MapStringString getHistoryMap();
      
   protected:
      void dragEnterEvent(QDragEnterEvent *e) { e->accept(); }
      void dragMoveEvent(QDragMoveEvent *e) { e->accept(); }
      
   private:
      QHash<QString, Call*> activeCalls;
      QHash<QString, Call*> historyCalls;
      QHash<CallTreeItem* , InternalCallModelStruct*> privateCallList_widget;
      QHash<QTreeWidgetItem* , InternalCallModelStruct*> privateCallList_item;
      QHash<Call* , InternalCallModelStruct*> privateCallList_call;
      QHash<QString , InternalCallModelStruct*> privateCallList_callId;
      InternalCallModelStruct find(const CallTreeItem* call);
      QTreeWidgetItem* extractItem(QString callId);
      QTreeWidgetItem* extractItem(QTreeWidgetItem* item);
      CallTreeItem* insertItem(QTreeWidgetItem* item, QTreeWidgetItem* parent=0);
      CallTreeItem* insertItem(QTreeWidgetItem* item, Call* parent);
      QString generateCallId();
      void clearArtefact(QTreeWidgetItem* item);
      
   private slots:
      void destroyCall(Call* toDestroy);
      
   public slots:
      void clearHistory();
};
#endif
