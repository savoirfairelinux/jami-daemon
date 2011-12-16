/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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
 **************************************************************************/

//Qt
#include <QtCore/QHash>
#include <QtCore/QDebug>
#include <QtGui/QDragEnterEvent>

//SFLPhone library
#include "Call.h"
#include "AccountList.h"
#include "dbus/metatypes.h"
#include "callmanager_interface_singleton.h"
#include "configurationmanager_interface_singleton.h"
#include "instance_interface_singleton.h"
#include "sflphone_const.h"
#include "typedefs.h"
#include "ContactBackend.h"

//System
#include "unistd.h"

//Static member
template  <typename CallWidget, typename Index> QString CallModel<CallWidget,Index>::m_sPriorAccountId   = ""    ;
template  <typename CallWidget, typename Index> AccountList* CallModel<CallWidget,Index>::m_spAccountList = 0    ;
template  <typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::m_sInstanceInit        = false ;
template  <typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::m_sCallInit            = false ;
template  <typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::m_sHistoryInit         = false ;

template  <typename CallWidget, typename Index> QHash<QString, Call*> CallModel<CallWidget,Index>::m_sActiveCalls  ;
template  <typename CallWidget, typename Index> QHash<QString, Call*> CallModel<CallWidget,Index>::m_sHistoryCalls ;

template  <typename CallWidget, typename Index> typename CallModel<CallWidget,Index>::InternalCall   CallModel<CallWidget,Index>::m_sPrivateCallList_call   ;
template  <typename CallWidget, typename Index> typename CallModel<CallWidget,Index>::InternalCallId CallModel<CallWidget,Index>::m_sPrivateCallList_callId ;
template  <typename CallWidget, typename Index> typename CallModel<CallWidget,Index>::InternalIndex  CallModel<CallWidget,Index>::m_sPrivateCallList_index  ;
template  <typename CallWidget, typename Index> typename CallModel<CallWidget,Index>::InternalWidget CallModel<CallWidget,Index>::m_sPrivateCallList_widget ;

/*****************************************************************************
 *                                                                           *
 *                               Constructor                                 *
 *                                                                           *
 ****************************************************************************/

///Retrieve current and older calls from the daemon, fill history and the calls TreeView and enable drag n' drop
template<typename CallWidget, typename Index> CallModel<CallWidget,Index>::CallModel(ModelType type) : CallModelBase(0)
{
   Q_UNUSED(type)
   init();

}

///Open the connection to the daemon and register this client
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::init() 
{
   if (!m_sInstanceInit) {
      registerCommTypes();
      InstanceInterface& instance = InstanceInterfaceSingleton::getInstance();
      instance.Register(getpid(), APP_NAME);
      
      //Setup accounts
      if (m_spAccountList == NULL)
	 m_spAccountList = new AccountList(true);
   }
   m_sInstanceInit = true;
   return true;
}

///Fill the call list
///@warning This solution wont scale to multiple call or history model implementation. Some static addCall + foreach for each call would be needed if this case ever become unavoidable
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::initCall()
{
   if (!m_sCallInit) {
      CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
      QStringList callList = callManager.getCallList();
      foreach (QString callId, callList) {
         Call* tmpCall = Call::buildExistingCall(callId);
         m_sActiveCalls[tmpCall->getCallId()] = tmpCall;
         addCall(tmpCall);
      }
   
      QStringList confList = callManager.getConferenceList();
      foreach (QString confId, confList) {
          addConference(confId);
      }
   }
   m_sCallInit = true;
   return true;
}

///Set how the call can find more informations about the call it receive
template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::initContact ( ContactBackend* be )
{
   Call::setContactBackend(be);
}

///Fill the history list
///@warning This solution wont scale to multiple call or history model implementation. Some static addCall + foreach for each call would be needed if this case ever become unavoidable
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::initHistory()
{
   if (!m_sHistoryInit) {
      ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
      QStringList historyMap = configurationManager.getHistory().value();
      foreach (QString historyCallId, historyMap) {
         QStringList param = historyCallId.split("|");
         if (param.count() <= 10) {
            //If this ever change, look at the gnome client
            QString history_state = param[0];
            QString peer_number   = param[1];
            QString peer_name     = param[2];
            QString time_start    = param[3];
            QString time_stop     = param[4];
            QString callID        = param[5];
            QString accountID     = param[6];
            QString recordfile    = param[7];
            QString confID        = param[8];
            QString time_added    = param[9];
            m_sHistoryCalls[time_start] = Call::buildHistoryCall(callID, time_start.toUInt(), time_stop.toUInt(), accountID, peer_name, peer_number, history_state);
            addCall(m_sHistoryCalls[time_start]);
         }
      }
   }
   m_sHistoryInit = true;
   return true;
}


/*****************************************************************************
 *                                                                           *
 *                         Access related functions                          *
 *                                                                           *
 ****************************************************************************/

///Return the active call count
template<typename CallWidget, typename Index> int CallModel<CallWidget,Index>::size() 
{
   return m_sActiveCalls.size();
}

///Return a call corresponding to this ID or NULL
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::findCallByCallId(const QString& callId)
{
   return m_sActiveCalls[callId];
}

///Return the action call list
template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCallList() 
{
   QList<Call*> callList;
   foreach(Call* call, m_sActiveCalls) {
      callList.push_back(call);
   }
   return callList;
}


/*****************************************************************************
 *                                                                           *
 *                            Call related code                              *
 *                                                                           *
 ****************************************************************************/

///Add a call in the model structure, the call must exist before being added to the model
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::addCall(Call* call, Call* parent) 
{
   Q_UNUSED(parent)
   InternalStruct* aNewStruct = new InternalStruct;
   aNewStruct->call_real  = call;
   aNewStruct->conference = false;
   
   m_sPrivateCallList_call[call]                = aNewStruct;
   m_sPrivateCallList_callId[call->getCallId()] = aNewStruct;

   //setCurrentItem(callItem);
   CallModelBase::addCall(call,parent);
   return call;
}

///Common set of instruction shared by all call adder
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::addCallCommon(Call* call)
{
   m_sActiveCalls[call->getCallId()] = call;
   addCall(call);
   selectItem(call);
   return call;
}

///Create a new dialing call from peer name and the account ID
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::addDialingCall(const QString& peerName, QString account)
{
   QString account2 = account;
   if (account2.isEmpty()) {
      account2 = getCurrentAccountId();
   }
   
   Call* call = Call::buildDialingCall(generateCallId(), peerName, account2);
   return addCallCommon(call);
}

///Create a new incomming call when the daemon is being called
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::addIncomingCall(const QString& callId)
{
   Call* call = Call::buildIncomingCall(callId);
   return addCallCommon(call);
}

///Create a ringing call
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::addRingingCall(const QString& callId)
{
   Call* call = Call::buildRingingCall(callId);
   return addCallCommon(call);
}

///Generate a new random call unique identifier (callId)
template<typename CallWidget, typename Index> QString CallModel<CallWidget,Index>::generateCallId()
{
   int id = qrand();
   QString res = QString::number(id);
   return res;
}

///Remove a call and update the internal structure
template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::removeCall(Call* call)
{
   InternalStruct* internal = m_sPrivateCallList_call[call];

   if (!internal) {
      qDebug() << "Cannot remove call: call not found";
      return;
   }

   if (m_sPrivateCallList_call[call] != NULL) {
      m_sPrivateCallList_call.remove(call);
   }

   if (m_sPrivateCallList_callId[m_sPrivateCallList_callId.key(internal)] == internal) {
      m_sPrivateCallList_callId.remove(m_sPrivateCallList_callId.key(internal));
   }

   if (m_sPrivateCallList_widget[m_sPrivateCallList_widget.key(internal)] == internal) {
      m_sPrivateCallList_widget.remove(m_sPrivateCallList_widget.key(internal));
   }

   if (m_sPrivateCallList_index[m_sPrivateCallList_index.key(internal)] == internal) {
      m_sPrivateCallList_index.remove(m_sPrivateCallList_index.key(internal));
   }
}

///Transfer "toTransfer" to "target" and wait to see it it succeeded
template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::attendedTransfer(Call* toTransfer, Call* target)
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.attendedTransfer(toTransfer->getCallId(),target->getCallId());

   //TODO [Daemon] Implement this correctly
   toTransfer->changeCurrentState(CALL_STATE_OVER);
   target->changeCurrentState(CALL_STATE_OVER);
}

///Transfer this call to  "target" number
template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::transfer(Call* toTransfer, QString target)
{
   qDebug() << "Transferring call " << target;
   toTransfer->setTransferNumber(target);
   toTransfer->actionPerformed(CALL_ACTION_ACCEPT);
   toTransfer->changeCurrentState(CALL_STATE_OVER);
}

/*****************************************************************************
 *                                                                           *
 *                         Conference related code                           *
 *                                                                           *
 ****************************************************************************/

///Add a new conference, get the call list and update the interface as needed
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::addConference(const QString & confID) 
{
   qDebug() << "Notified of a new conference " << confID;
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   QStringList callList = callManager.getParticipantList(confID);
   qDebug() << "Paticiapants are:" << callList;
   
   if (!callList.size()) {
      qDebug() << "This conference (" + confID + ") contain no call";
      return 0;
   }

   if (!m_sPrivateCallList_callId[callList[0]]) {
      qDebug() << "Invalid call";
      return 0;
   }
   Call* newConf =  new Call(confID, m_sPrivateCallList_callId[callList[0]]->call_real->getAccountId());
   
   InternalStruct* aNewStruct = new InternalStruct;
   aNewStruct->call_real  = newConf;
   aNewStruct->conference = true;
   
   m_sPrivateCallList_call[newConf]  = aNewStruct;
   m_sPrivateCallList_callId[confID] = aNewStruct;
   
   return newConf;
}

///Join two call to create a conference, the conference will be created later (see addConference)
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::createConferenceFromCall(Call* call1, Call* call2) 
{
  qDebug() << "Joining call: " << call1->getCallId() << " and " << call2->getCallId();
  CallManagerInterface &callManager = CallManagerInterfaceSingleton::getInstance();
  callManager.joinParticipant(call1->getCallId(),call2->getCallId());
  return true;
}

///Add a new participant to a conference
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::addParticipant(Call* call2, Call* conference) 
{
   if (conference->isConference()) {
      CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
      callManager.addParticipant(call2->getCallId(), conference->getConfId());
      return true;
   }
   else {
      qDebug() << "This is not a conference";
      return false;
   }
}

///Remove a participant from a conference
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::detachParticipant(Call* call) 
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.detachParticipant(call->getCallId());
   return true;
}

///Merge two conferences
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::mergeConferences(Call* conf1, Call* conf2) 
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.joinConference(conf1->getConfId(),conf2->getConfId());
   return true;
}

///Executed when the daemon signal a modification in an existing conference. Update the call list and update the TreeView
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::changeConference(const QString& confId, const QString& state)
{
   qDebug() << "Conf changed";
   Q_UNUSED(state)
   
   if (!m_sPrivateCallList_callId[confId]) {
      qDebug() << "The conference does not exist";
      return false;
   }
   
   if (!m_sPrivateCallList_callId[confId]->index) {
      qDebug() << "The conference item does not exist";
      return false;
   }
   return true;
}

///Remove a conference from the model and the TreeView
template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::removeConference(const QString &confId)
{
   qDebug() << "Ending conversation containing " << m_sPrivateCallList_callId[confId]->children.size() << " participants";
   removeConference(getCall(confId));
}

///Remove a conference using it's call object
template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::removeConference(Call* call)
{
   InternalStruct* internal = m_sPrivateCallList_call[call];
   
   if (!internal) {
      qDebug() << "Cannot remove conference: call not found";
      return;
   }
   removeCall(call);
}


/*****************************************************************************
 *                                                                           *
 *                           History related code                            *
 *                                                                           *
 ****************************************************************************/

///Return a list of all previous calls
template<typename CallWidget, typename Index> const QStringList CallModel<CallWidget,Index>::getHistoryCallId() 
{
   QStringList toReturn;
   foreach(Call* call, m_sHistoryCalls) {
      toReturn << call->getCallId();
   }
   return toReturn;
}

///Return the history list
template<typename CallWidget, typename Index> const CallHash& CallModel<CallWidget,Index>::getHistory()
{
   return m_sHistoryCalls;
}

/*****************************************************************************
 *                                                                           *
 *                           Account related code                            *
 *                                                                           *
 ****************************************************************************/

///Return the current account id (do not put in the cpp file)
template<typename CallWidget, typename Index> QString CallModel<CallWidget,Index>::getCurrentAccountId()
{
   Account* firstRegistered = getCurrentAccount();
   if(firstRegistered == NULL) {
      return QString();
   }
   else {
      return firstRegistered->getAccountId();
   }
}


///Return the current account
template<typename CallWidget, typename Index> Account* CallModel<CallWidget,Index>::getCurrentAccount()
{
   Account* priorAccount = getAccountList()->getAccountById(m_sPriorAccountId);
   if(priorAccount && priorAccount->getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED ) {
      return priorAccount;
   }
   else {
      qDebug() << "Returning the first account" << getAccountList()->size();
      return getAccountList()->firstRegisteredAccount();
   }
}

///Return a list of registered accounts
template<typename CallWidget, typename Index> AccountList* CallModel<CallWidget,Index>::getAccountList()
{
   if (m_spAccountList == NULL) {
      m_spAccountList = new AccountList(true);
   }
   return m_spAccountList;
}

///Return the previously used account ID
template<typename CallWidget, typename Index> QString CallModel<CallWidget,Index>::getPriorAccoundId() 
{
   return m_sPriorAccountId;
}

///Set the previous account used
template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::setPriorAccountId(const QString& value) {
   m_sPriorAccountId = value;
}

/*****************************************************************************
 *                                                                           *
 *                             Magic Dispatcher                              *
 *                                                                           *
 ****************************************************************************/

///Get a call from it's widget                                     
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::getCall         ( const CallWidget widget     ) const
{
   if (m_sPrivateCallList_widget[widget]) {
      return m_sPrivateCallList_widget[widget]->call_real;
   }
   return NULL;
}

///Get a call list from a conference                               
template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCalls ( const CallWidget widget     ) const
{
   QList<Call*> toReturn;
   if (m_sPrivateCallList_widget[widget] && m_sPrivateCallList_widget[widget]->conference) {
      foreach (InternalStruct* child, m_sPrivateCallList_widget[widget]->children) {
	 toReturn << child.call_real;
      }
   }
   return toReturn;
}

///Get a list of every call                                        
template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCalls (                             )
{
   QList<Call*> toReturn;
   foreach (InternalStruct* child, m_sPrivateCallList_call) {
      toReturn << child->call_real;
   }
   return toReturn;
}

///Is the call associated with that widget a conference            
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::isConference     ( const CallWidget widget      ) const
{
   if (m_sPrivateCallList_widget[widget]) {
      return m_sPrivateCallList_widget[widget]->conference;
   }
   return false;
}

///Is that call a conference                                       
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::isConference     ( const Call* call             ) const
{
   if (m_sPrivateCallList_call[(Call*)call]) {
      return m_sPrivateCallList_call[(Call*)call]->conference;
   }
   return false;
}

///Do nothing, provided for API consistency                        
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::getCall         ( const Call* call             ) const
{ 
   return call;
}

///Return the calls from the "call" conference                     
template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCalls ( const Call* call             ) const
{ 
   QList<Call*> toReturn;
   if (m_sPrivateCallList_call[call] && m_sPrivateCallList_call[call]->conference) {
      foreach (InternalStruct* child, m_sPrivateCallList_call[call]->children) {
	 toReturn << child.call_real;
      }
   }
   return toReturn;
}

///Is the call associated with that Index a conference             
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::isConference     ( const Index idx              ) const
{ 
   if (m_sPrivateCallList_index[idx]) {
      return m_sPrivateCallList_index[idx]->conference;
   }
   return false;
}

///Get the call associated with this index                         
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::getCall         ( const Index idx              ) const
{ 
   if (m_sPrivateCallList_index[idx]) {
      return m_sPrivateCallList_index[idx]->call_real;
   }
   qDebug() << "Call not found";
   return NULL;
}

///Get the call associated with that conference index              
template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCalls ( const Index idx              ) const
{ 
   QList<Call*> toReturn;
   if (m_sPrivateCallList_index[idx] && m_sPrivateCallList_index[idx]->conference) {
      foreach (InternalStruct* child, m_sPrivateCallList_index[idx]->children) {
	 toReturn << child.call_real;
      }
   }
   return toReturn;
}

///Is the call associated with that ID a conference                
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::isConference     ( const QString& callId        ) const
{ 
   if (m_sPrivateCallList_callId[callId]) {
      return m_sPrivateCallList_callId[callId]->conference;
   }
   return false;
}

///Get the call associated with this ID                            
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::getCall         ( const QString& callId        ) const
{ 
   if (m_sPrivateCallList_callId[callId]) {
      return m_sPrivateCallList_callId[callId]->call_real;
   }
   return NULL;
}

///Get the calls associated with this ID                           
template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCalls ( const QString& callId        ) const
{
   QList<Call*> toReturn;
   if (m_sPrivateCallList_callId[callId] && m_sPrivateCallList_callId[callId]->conference) {
      foreach (InternalStruct* child, m_sPrivateCallList_callId[callId]->children) {
	 toReturn << child.callId_real;
      }
   }
   return toReturn;
}

///Get the index associated with this call                         
template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getIndex        ( const Call* call             ) const
{
   if (m_sPrivateCallList_call[(Call*)call]) {
      return m_sPrivateCallList_call[(Call*)call]->index;
   }
   return NULL;
}

///Get the index associated with this index (dummy implementation) 
template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getIndex        ( const Index idx              ) const
{
   if (m_sPrivateCallList_index[idx]) {
      return m_sPrivateCallList_index[idx]->index;
   }
   return NULL;
}

///Get the index associated with this call                         
template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getIndex        ( const CallWidget widget      ) const
{
   if (m_sPrivateCallList_widget[widget]) {
      return m_sPrivateCallList_widget[widget]->index;
   }
   return NULL;
}

///Get the index associated with this ID                           
template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getIndex        ( const QString& callId        ) const
{
   if (m_sPrivateCallList_callId[callId]) {
      return m_sPrivateCallList_callId[callId]->index;
   }
   return NULL;
}

///Get the widget associated with this call                        
template<typename CallWidget, typename Index> CallWidget CallModel<CallWidget,Index>::getWidget  ( const Call* call             ) const
{
   if (m_sPrivateCallList_call[call]) {
      return m_sPrivateCallList_call[call]->call;
   }
   return NULL;
}

///Get the widget associated with this ID                          
template<typename CallWidget, typename Index> CallWidget CallModel<CallWidget,Index>::getWidget  ( const Index idx              ) const
{
   if (m_sPrivateCallList_index[idx]) {
      return m_sPrivateCallList_index[idx]->call;
   }
   return NULL;
}

///Get the widget associated with this widget (dummy)              
template<typename CallWidget, typename Index> CallWidget CallModel<CallWidget,Index>::getWidget  ( const CallWidget widget      ) const
{
   if (m_sPrivateCallList_widget[widget]) {
      return m_sPrivateCallList_widget[widget]->call;
   }
   return NULL;
}

///Get the widget associated with this ID                          
template<typename CallWidget, typename Index> CallWidget CallModel<CallWidget,Index>::getWidget  ( const QString& widget        ) const
{
   if (m_sPrivateCallList_widget[widget]) {
      return m_sPrivateCallList_widget[widget]->call;
   }
   return NULL;
}

///Common set of instruction shared by all gui updater
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::updateCommon(Call* call)
{
   if (!m_sPrivateCallList_call[call]) {
      m_sPrivateCallList_call   [ call              ]             = new InternalStruct            ;
      m_sPrivateCallList_call   [ call              ]->call_real  = call                          ;
      m_sPrivateCallList_call   [ call              ]->conference = false                         ;
      m_sPrivateCallList_callId [ call->getCallId() ]             = m_sPrivateCallList_call[call] ;
   }
   return true;
}

///Update the widget associated with this call                     
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::updateWidget     (Call* call, CallWidget value )
{
   updateCommon(call);
   m_sPrivateCallList_call[call]->call = value                         ;
   m_sPrivateCallList_widget[value]    = m_sPrivateCallList_call[call] ;
   return true;
}


///Update the index associated with this call
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::updateIndex      (Call* call, Index value      )
{
   updateCommon(call);
   m_sPrivateCallList_call[call]->index = value                         ;
   m_sPrivateCallList_index[value]      = m_sPrivateCallList_call[call] ;
   return true;
}