/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

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

//Define
#define CALLMODEL_TEMPLATE template<typename CallWidget, typename Index>
#define CALLMODEL_T CallModel<CallWidget,Index>

//Static member
CALLMODEL_TEMPLATE bool         CALLMODEL_T::m_sInstanceInit        = false     ;
CALLMODEL_TEMPLATE bool         CALLMODEL_T::m_sCallInit            = false     ;
CALLMODEL_TEMPLATE CallMap      CALLMODEL_T::m_lConfList            = CallMap() ;

CALLMODEL_TEMPLATE typename CALLMODEL_T::InternalCall   CALLMODEL_T::m_sPrivateCallList_call   ;
CALLMODEL_TEMPLATE typename CALLMODEL_T::InternalCallId CALLMODEL_T::m_sPrivateCallList_callId ;
CALLMODEL_TEMPLATE typename CALLMODEL_T::InternalIndex  CALLMODEL_T::m_sPrivateCallList_index  ;
CALLMODEL_TEMPLATE typename CALLMODEL_T::InternalWidget CALLMODEL_T::m_sPrivateCallList_widget ;

/*****************************************************************************
 *                                                                           *
 *                               Constructor                                 *
 *                                                                           *
 ****************************************************************************/

///Retrieve current and older calls from the daemon, fill history and the calls TreeView and enable drag n' drop
CALLMODEL_TEMPLATE CALLMODEL_T::CallModel() : CallModelBase(0)
{
   init();
}

///Static destructor
CALLMODEL_TEMPLATE void CALLMODEL_T::destroy()
{
   foreach (Call* call,  m_sPrivateCallList_call.keys()) {
      delete call;
   }
   foreach (InternalStruct* s,  m_sPrivateCallList_call.values()) {
      delete s;
   }
   m_sPrivateCallList_call.clear();
   m_sPrivateCallList_callId.clear();
   m_sPrivateCallList_widget.clear();
   m_sPrivateCallList_index.clear();
}

///Destructor
CALLMODEL_TEMPLATE CALLMODEL_T::~CallModel()
{
   
}

///Open the connection to the daemon and register this client
CALLMODEL_TEMPLATE bool CALLMODEL_T::init()
{
   if (!m_sInstanceInit)
      registerCommTypes();
   m_sInstanceInit = true;
   return true;
} //init

///Fill the call list
///@warning This solution wont scale to multiple call or history model implementation. Some static addCall + foreach for each call would be needed if this case ever become unavoidable
CALLMODEL_TEMPLATE bool CALLMODEL_T::initCall()
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
         CallModelBase::addConferenceS(addConference(confId));
      }
   }
   m_sCallInit = true;
   return true;
} //initCall


/*****************************************************************************
 *                                                                           *
 *                         Access related functions                          *
 *                                                                           *
 ****************************************************************************/

///Return the active call count
CALLMODEL_TEMPLATE int CALLMODEL_T::size()
{
   return m_sActiveCalls.size();
}

///Return a call corresponding to this ID or NULL
CALLMODEL_TEMPLATE Call* CALLMODEL_T::findCallByCallId(const QString& callId)
{
   return m_sActiveCalls[callId];
}

///Return the action call list
CALLMODEL_TEMPLATE CallList CALLMODEL_T::getCallList()
{
   CallList callList;
   foreach(Call* call, m_sActiveCalls) {
      if (dynamic_cast<Call*>(call) && call->getState() != CALL_STATE_OVER) //Prevent a race
         callList.push_back(call);
   }
   return callList;
}

///Return all conferences
CALLMODEL_TEMPLATE CallList CALLMODEL_T::getConferenceList()
{
   CallList confList;

   //That way it can not be invalid
   QStringList confListS = CallManagerInterfaceSingleton::getInstance().getConferenceList();
   foreach (QString confId, confListS) {
        if (m_lConfList[confId] != nullptr)
           confList << m_lConfList[confId];
        else
           confList << addConference(confId);
   }
   return confList;
}


/*****************************************************************************
 *                                                                           *
 *                            Call related code                              *
 *                                                                           *
 ****************************************************************************/

///Add a call in the model structure, the call must exist before being added to the model
CALLMODEL_TEMPLATE Call* CALLMODEL_T::addCall(Call* call, Call* parent)
{
   Q_UNUSED(parent)
   if (!call)
      return new Call("",""); //Invalid, but better than managing NULL everywhere

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
CALLMODEL_TEMPLATE Call* CALLMODEL_T::addCallCommon(Call* call)
{
   m_sActiveCalls[call->getCallId()] = call;
   addCall(call);
   return call;
} //addCallCommon

///Create a new dialing call from peer name and the account ID
CALLMODEL_TEMPLATE Call* CALLMODEL_T::addDialingCall(const QString& peerName, Account* account)
{
   Account* acc = (account)?account:AccountList::getCurrentAccount();
   if (acc) {
      Call* call = Call::buildDialingCall(generateCallId(), peerName, acc->getAccountId());
      return addCallCommon(call);
   }
   else {
      return nullptr;
   }
}  //addDialingCall

///Create a new incomming call when the daemon is being called
CALLMODEL_TEMPLATE Call* CALLMODEL_T::addIncomingCall(const QString& callId)
{
   Call* call = Call::buildIncomingCall(callId);
   return addCallCommon(call);
}

///Create a ringing call
CALLMODEL_TEMPLATE Call* CALLMODEL_T::addRingingCall(const QString& callId)
{
   Call* call = Call::buildRingingCall(callId);
   return addCallCommon(call);
}

///Generate a new random call unique identifier (callId)
CALLMODEL_TEMPLATE QString CALLMODEL_T::generateCallId()
{
   int id = qrand();
   QString res = QString::number(id);
   return res;
}

///Remove a call and update the internal structure
CALLMODEL_TEMPLATE void CALLMODEL_T::removeCall(Call* call)
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
} //removeCall

///Transfer "toTransfer" to "target" and wait to see it it succeeded
CALLMODEL_TEMPLATE void CALLMODEL_T::attendedTransfer(Call* toTransfer, Call* target)
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.attendedTransfer(toTransfer->getCallId(),target->getCallId());

   //TODO [Daemon] Implement this correctly
   toTransfer->changeCurrentState(CALL_STATE_OVER);
   target->changeCurrentState(CALL_STATE_OVER);
} //attendedTransfer

///Transfer this call to  "target" number
CALLMODEL_TEMPLATE void CALLMODEL_T::transfer(Call* toTransfer, QString target)
{
   qDebug() << "Transferring call " << toTransfer->getCallId() << "to" << target;
   toTransfer->setTransferNumber(target);
   toTransfer->actionPerformed(CALL_ACTION_TRANSFER);
   toTransfer->changeCurrentState(CALL_STATE_OVER);
} //transfer

/*****************************************************************************
 *                                                                           *
 *                         Conference related code                           *
 *                                                                           *
 ****************************************************************************/

///Add a new conference, get the call list and update the interface as needed
CALLMODEL_TEMPLATE Call* CALLMODEL_T::addConference(const QString & confID)
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

   m_lConfList[newConf->getConfId()] = newConf;
   
   return newConf;
} //addConference

///Join two call to create a conference, the conference will be created later (see addConference)
CALLMODEL_TEMPLATE bool CALLMODEL_T::createConferenceFromCall(Call* call1, Call* call2)
{
  qDebug() << "Joining call: " << call1->getCallId() << " and " << call2->getCallId();
  CallManagerInterface &callManager = CallManagerInterfaceSingleton::getInstance();
  callManager.joinParticipant(call1->getCallId(),call2->getCallId());
  return true;
} //createConferenceFromCall

///Add a new participant to a conference
CALLMODEL_TEMPLATE bool CALLMODEL_T::addParticipant(Call* call2, Call* conference)
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
} //addParticipant

///Remove a participant from a conference
CALLMODEL_TEMPLATE bool CALLMODEL_T::detachParticipant(Call* call)
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.detachParticipant(call->getCallId());
   return true;
}

///Merge two conferences
CALLMODEL_TEMPLATE bool CALLMODEL_T::mergeConferences(Call* conf1, Call* conf2)
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.joinConference(conf1->getConfId(),conf2->getConfId());
   return true;
}

///Executed when the daemon signal a modification in an existing conference. Update the call list and update the TreeView
CALLMODEL_TEMPLATE bool CALLMODEL_T::changeConference(const QString& confId, const QString& state)
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
} //changeConference

///Remove a conference from the model and the TreeView
CALLMODEL_TEMPLATE void CALLMODEL_T::removeConference(const QString &confId)
{
   if (m_sPrivateCallList_callId[confId])
      qDebug() << "Ending conversation containing " << m_sPrivateCallList_callId[confId]->children.size() << " participants";
   removeConference(getCall(confId));
}

///Remove a conference using it's call object
CALLMODEL_TEMPLATE void CALLMODEL_T::removeConference(Call* call)
{
   InternalStruct* internal = m_sPrivateCallList_call[call];
   
   if (!internal) {
      qDebug() << "Cannot remove conference: call not found";
      return;
   }
   removeCall(call);

   m_lConfList[call->getConfId()] = nullptr;
}


/*****************************************************************************
 *                                                                           *
 *                             Magic Dispatcher                              *
 *                                                                           *
 ****************************************************************************/

///Get a call from it's widget                                     
CALLMODEL_TEMPLATE Call* CALLMODEL_T::getCall         ( const CallWidget widget     ) const
{
   if (m_sPrivateCallList_widget[widget]) {
      return m_sPrivateCallList_widget[widget]->call_real;
   }
   return NULL;
}

///Get a call list from a conference                               
CALLMODEL_TEMPLATE QList<Call*> CALLMODEL_T::getCalls ( const CallWidget widget     ) const
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
CALLMODEL_TEMPLATE QList<Call*> CALLMODEL_T::getCalls (                             )
{
   QList<Call*> toReturn;
   foreach (InternalStruct* child, m_sPrivateCallList_call) {
      toReturn << child->call_real;
   }
   return toReturn;
}

///Is the call associated with that widget a conference            
CALLMODEL_TEMPLATE bool CALLMODEL_T::isConference     ( const CallWidget widget      ) const
{
   if (m_sPrivateCallList_widget[widget]) {
      return m_sPrivateCallList_widget[widget]->conference;
   }
   return false;
}

///Is that call a conference                                       
CALLMODEL_TEMPLATE bool CALLMODEL_T::isConference     ( const Call* call             ) const
{
   if (m_sPrivateCallList_call[(Call*)call]) {
      return m_sPrivateCallList_call[(Call*)call]->conference;
   }
   return false;
}

///Do nothing, provided for API consistency                        
CALLMODEL_TEMPLATE Call* CALLMODEL_T::getCall         ( const Call* call             ) const
{ 
   return call;
}

///Return the calls from the "call" conference                     
CALLMODEL_TEMPLATE QList<Call*> CALLMODEL_T::getCalls ( const Call* call             ) const
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
CALLMODEL_TEMPLATE bool CALLMODEL_T::isConference     ( const Index idx              ) const
{ 
   if (m_sPrivateCallList_index[idx]) {
      return m_sPrivateCallList_index[idx]->conference;
   }
   return false;
}

///Get the call associated with this index                         
CALLMODEL_TEMPLATE Call* CALLMODEL_T::getCall         ( const Index idx              ) const
{ 
   if (m_sPrivateCallList_index[idx]) {
      return m_sPrivateCallList_index[idx]->call_real;
   }
   qDebug() << "Call not found";
   return NULL;
}

///Get the call associated with that conference index              
CALLMODEL_TEMPLATE QList<Call*> CALLMODEL_T::getCalls ( const Index idx              ) const
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
CALLMODEL_TEMPLATE bool CALLMODEL_T::isConference     ( const QString& callId        ) const
{ 
   if (m_sPrivateCallList_callId[callId]) {
      return m_sPrivateCallList_callId[callId]->conference;
   }
   return false;
}

///Get the call associated with this ID                            
CALLMODEL_TEMPLATE Call* CALLMODEL_T::getCall         ( const QString& callId        ) const
{ 
   if (m_sPrivateCallList_callId[callId]) {
      return m_sPrivateCallList_callId[callId]->call_real;
   }
   return NULL;
}

///Get the calls associated with this ID                           
CALLMODEL_TEMPLATE QList<Call*> CALLMODEL_T::getCalls ( const QString& callId        ) const
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
CALLMODEL_TEMPLATE Index CALLMODEL_T::getIndex        ( const Call* call             ) const
{
   if (m_sPrivateCallList_call[(Call*)call]) {
      return m_sPrivateCallList_call[(Call*)call]->index;
   }
   return NULL;
}

///Get the index associated with this index (dummy implementation) 
CALLMODEL_TEMPLATE Index CALLMODEL_T::getIndex        ( const Index idx              ) const
{
   if (m_sPrivateCallList_index[idx]) {
      return m_sPrivateCallList_index[idx]->index;
   }
   return NULL;
}

///Get the index associated with this call                         
CALLMODEL_TEMPLATE Index CALLMODEL_T::getIndex        ( const CallWidget widget      ) const
{
   if (m_sPrivateCallList_widget[widget]) {
      return m_sPrivateCallList_widget[widget]->index;
   }
   return NULL;
}

///Get the index associated with this ID                           
CALLMODEL_TEMPLATE Index CALLMODEL_T::getIndex        ( const QString& callId        ) const
{
   if (m_sPrivateCallList_callId[callId]) {
      return m_sPrivateCallList_callId[callId]->index;
   }
   return NULL;
}

///Get the widget associated with this call                        
CALLMODEL_TEMPLATE CallWidget CALLMODEL_T::getWidget  ( const Call* call             ) const
{
   if (m_sPrivateCallList_call[call]) {
      return m_sPrivateCallList_call[call]->call;
   }
   return NULL;
}

///Get the widget associated with this ID                          
CALLMODEL_TEMPLATE CallWidget CALLMODEL_T::getWidget  ( const Index idx              ) const
{
   if (m_sPrivateCallList_index[idx]) {
      return m_sPrivateCallList_index[idx]->call;
   }
   return NULL;
}

///Get the widget associated with this widget (dummy)              
CALLMODEL_TEMPLATE CallWidget CALLMODEL_T::getWidget  ( const CallWidget widget      ) const
{
   if (m_sPrivateCallList_widget[widget]) {
      return m_sPrivateCallList_widget[widget]->call;
   }
   return NULL;
}

///Get the widget associated with this ID                          
CALLMODEL_TEMPLATE CallWidget CALLMODEL_T::getWidget  ( const QString& widget        ) const
{
   if (m_sPrivateCallList_widget[widget]) {
      return m_sPrivateCallList_widget[widget]->call;
   }
   return NULL;
}

///Common set of instruction shared by all gui updater
CALLMODEL_TEMPLATE bool CALLMODEL_T::updateCommon(Call* call)
{
   if (!m_sPrivateCallList_call[call] && dynamic_cast<Call*>(call)) {
      m_sPrivateCallList_call   [ call              ]             = new InternalStruct            ;
      m_sPrivateCallList_call   [ call              ]->call_real  = call                          ;
      m_sPrivateCallList_call   [ call              ]->conference = false                         ;
      m_sPrivateCallList_callId [ call->getCallId() ]             = m_sPrivateCallList_call[call] ;
   }
   else
      return false;
   return true;
}

///Update the widget associated with this call                     
CALLMODEL_TEMPLATE bool CALLMODEL_T::updateWidget     (Call* call, CallWidget value )
{
   if (!updateCommon(call)) return false;
   m_sPrivateCallList_call[call]->call = value                         ;
   m_sPrivateCallList_widget[value]    = m_sPrivateCallList_call[call] ;
   return true;
}

///Update the index associated with this call
CALLMODEL_TEMPLATE bool CALLMODEL_T::updateIndex      (Call* call, Index value      )
{
   updateCommon(call);
   if (!m_sPrivateCallList_call[call])
      return false;
   m_sPrivateCallList_call[call]->index = value                         ;
   m_sPrivateCallList_index[value]      = m_sPrivateCallList_call[call] ;
   return true;
}
