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

#ifndef CALL_MODEL_H
#define CALL_MODEL_H

#include <QObject>
#include <QHash>
#include <QVector>
#include <QDragEnterEvent>
#include <QDebug>

#include "Call.h"
#include "AccountList.h"
#include "dbus/metatypes.h"
#include "callmanager_interface_singleton.h"
#include "configurationmanager_interface_singleton.h"
#include "instance_interface_singleton.h"
#include "sflphone_const.h"
#include "unistd.h"
#include "typedefs.h"

/** Note from the author: It was previously done by a QAbstractModel + QTreeView, but the sip-call use case is incompatible 
 *  with the MVC model. The MVC never got to a point were it was bug-free and the code was getting dirty. The QTreeWidget
 *  solution may be less "clean" than MVC, but is 3 time smaller and easier to improve (in fact, possible to improve).
 *  
 *  @note This model intend to be reimplemented by the view, not used alone
 *  @note Most of the member are static to preserve ressources and QObject::connect()
 */


// template  <typename T, typename Index> class CallModel;
// template  <typename Widget, typename Index> class InternalCallModelStruct : public QModelIndex {
//    
//    friend class CallModel<Widget,Index>;
//    //InternalCallModelStruct* parent;
//    Widget* call;
//    Call* call_real;
//    Index* treeItem; //For the view
//    QList<InternalCallModelStruct*> children; //For the view
//    bool conference;
// };

template  <typename CallWidget, typename Index>
class LIB_EXPORT CallModel {
   //Q_OBJECT
   public:
      enum ModelType {
         ActiveCall,
         History,
         Address
      };
      
      CallModel(ModelType type);
      virtual ~CallModel() {}

      virtual Call* addCall(Call* call, Call* parent =0);
      int size();
      Call* findCallByCallId(QString callId);
      QList<Call*> getCallList();
      
      Call* addDialingCall(const QString& peerName="", QString account="");
      Call* addIncomingCall(const QString& callId);
      Call* addRingingCall(const QString& callId);
      bool createConferenceFromCall(Call* call1, Call* call2);
      bool mergeConferences(Call* conf1, Call* conf2);
      bool addParticipant(Call* call2, Call* conference);
      bool detachParticipant(Call* call);
      virtual Call* addConference(const QString &confID);
      virtual bool conferenceChanged(const QString &confId, const QString &state);
      virtual void conferenceRemoved(const QString &confId);
      virtual bool selectItem(Call* item) { Q_UNUSED(item); return false;}
      static QString generateCallId();
      void removeConference(Call* call);
      void removeCall(Call* call);
      
      QStringList getHistory();

      //Account related members
      static Account* getCurrentAccount();
      static QString getCurrentAccountId();
      static AccountList* getAccountList();
      static QString getPriorAccoundId();
      static void setPriorAccountId(QString value);

      //Connection related members
      static bool init();
      
      //Magic dispatcher
      Call* getCall(const CallWidget widget) const;
      Index getTreeItem(const CallWidget widget) const;
      QList<Call*> getCalls(const CallWidget widget) const;
      QList<Call*> getCalls();
      bool isConference(const CallWidget widget) const;
      
      Call* getCall(const Call* call) const;
      Index getTreeItem(const Call* call) const;
      QList<Call*> getCalls(const Call* call) const;
      bool isConference(const Call* call) const;
      
      Call* getCall(const Index idx) const;
      Index getTreeItem(const Index idx) const;
      QList<Call*> getCalls(const Index idx) const;
      bool isConference(const Index idx) const;
      
      Call* getCall(const QString callId) const;
      Index getTreeItem(const QString callId) const;
      QList<Call*> getCalls(const QString callId) const;
      bool isConference(const QString callId) const;
      
      Index getIndex(const Call* call) const;
      Index getIndex(const Index idx) const;
      Index getIndex(const CallWidget widget) const;
      Index getIndex(const QString callId) const;
      
      CallWidget getWidget(const Call* call) const;
      CallWidget getWidget(const Index idx) const;
      CallWidget getWidget(const CallWidget widget) const;
      CallWidget getWidget(const QString getWidget) const;
      
      bool updateIndex(Call* call, Index value);
      bool updateWidget(Call* call, CallWidget value);
      
      
   protected:
      struct InternalCallModelStruct {
	 CallWidget call;
	 Call* call_real;
	 Index treeItem; //For the view
	 QList<InternalCallModelStruct*> children; //For the view
	 bool conference;
      };
      typedef QHash<Call*, InternalCallModelStruct*> InternalCall;
      typedef QHash<QString, InternalCallModelStruct*> InternalCallId;
      typedef QHash<CallWidget, InternalCallModelStruct*> InternalWidget;
      typedef QHash<Index, InternalCallModelStruct*> InternalIndex;
      
      static QHash<QString, Call*> activeCalls;
      static QHash<QString, Call*> historyCalls;/*
      static QHash<Call*, InternalCallModelStruct<T, Index>* > privateCallList_call;
      static QHash<QString, InternalCallModelStruct<T, Index>* > privateCallList_callId;
      static QHash<T, InternalCallModelStruct<T, Index>* > privateCallList_widget;
      static QHash<Index, InternalCallModelStruct<T, Index>* > privateCallList_index;*/
      
      static InternalCall privateCallList_call;
      static InternalCallId privateCallList_callId;
      static InternalWidget privateCallList_widget;
      static InternalIndex privateCallList_index;
      
      static QString currentAccountId;
      static QString priorAccountId;
      static AccountList* accountList;
      static bool callInit;
      static bool historyInit;
      virtual bool initCall();
      virtual bool initHistory();

   private:
      static bool instanceInit;
   //public slots:
      //void clearHistory();
};

class CallModelConvenience : public CallModel<QWidget*,QModelIndex*>
{
   public:
      CallModelConvenience(ModelType type) : CallModel<QWidget*,QModelIndex*>(type) {}
};

#include "CallModel.hpp"

#endif
