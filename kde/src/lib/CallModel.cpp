/****************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                               *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

//Parent
#include <CallModel.h>
#include "video_interface_singleton.h"
#include "HistoryModel.h"

bool CallModelBase::dbusInit = false;
CallMap CallModelBase::m_sActiveCalls;

///Constructor
CallModelBase::CallModelBase(QObject* parent) : QObject(parent)
{
   if (!dbusInit) {
      CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
      VideoInterface& interface = VideoInterfaceSingleton::getInstance();
      
      //SLOTS
      /*             SENDER                          SIGNAL                     RECEIVER                    SLOT                   */
      /**/connect(&callManager, SIGNAL(callStateChanged(QString,QString))       , this , SLOT(callStateChanged(QString,QString))   );
      /**/connect(&callManager, SIGNAL(incomingCall(QString,QString,QString))   , this , SLOT(incomingCall(QString,QString))       );
      /**/connect(&callManager, SIGNAL(conferenceCreated(QString))              , this , SLOT(incomingConference(QString))         );
      /**/connect(&callManager, SIGNAL(conferenceChanged(QString,QString))      , this , SLOT(changingConference(QString,QString)) );
      /**/connect(&callManager, SIGNAL(conferenceRemoved(QString))              , this , SLOT(conferenceRemovedSlot(QString))      );
      /**/connect(&callManager, SIGNAL(voiceMailNotify(QString,int))            , this , SLOT(voiceMailNotifySlot(QString,int))    );
      /**/connect(&callManager, SIGNAL(volumeChanged(QString,double))           , this , SLOT(volumeChangedSlot(QString,double))   );
      /**/connect(&interface  , SIGNAL(startedDecoding(QString,QString,int,int)), this , SLOT(startedDecoding(QString,QString))    );
      /**/connect(&interface  , SIGNAL(stoppedDecoding(QString,QString))        , this , SLOT(stoppedDecoding(QString,QString))    );
      /*                                                                                                                           */

      connect(HistoryModel::self(),SIGNAL(newHistoryCall(Call*)),this,SLOT(addPrivateCall(Call*)));
      
      dbusInit = true;

      foreach(Call* call,HistoryModel::getHistory()){
         addCall(call,0);
      }
   }
}

///Destructor
CallModelBase::~CallModelBase()
{
   //if (m_spAccountList) delete m_spAccountList;
}

///When a call state change
void CallModelBase::callStateChanged(const QString &callID, const QString &state)
{
   //This code is part of the CallModel interface too
   qDebug() << "Call State Changed for call  " << callID << " . New state : " << state;
   Call* call = findCallByCallId(callID);
   if(!call) {
      qDebug() << "Call not found";
      if(state == CALL_STATE_CHANGE_RINGING) {
         call = addRingingCall(callID);
      }
      else {
         qDebug() << "Call doesn't exist in this client. Might have been initialized by another client instance before this one started.";
         return;
      }
   }
   else {
      qDebug() << "Call found" << call;
      call->stateChanged(state);
   }

   if (call->getCurrentState() == CALL_STATE_OVER) {
      HistoryModel::add(call);
   }
   
   emit callStateChanged(call);
   
}


/*****************************************************************************
 *                                                                           *
 *                                   Slots                                   *
 *                                                                           *
 ****************************************************************************/

///When a new call is incoming
void CallModelBase::incomingCall(const QString & accountID, const QString & callID)
{
   Q_UNUSED(accountID)
   qDebug() << "Signal : Incoming Call ! ID = " << callID;
   Call* call = addIncomingCall(callID);

   emit incomingCall(call);
}

///When a new conference is incoming
void CallModelBase::incomingConference(const QString &confID)
{
   Call* conf = addConference(confID);
   qDebug() << "Adding conference" << conf << confID;
   emit conferenceCreated(conf);
}

///When a conference change
void CallModelBase::changingConference(const QString &confID, const QString &state)
{
   Call* conf = getCall(confID);
   qDebug() << "Changing conference state" << conf << confID;
   if (conf && dynamic_cast<Call*>(conf)) { //Prevent a race condition between call and conference
      changeConference(confID, state);
      conf->stateChanged(state);
      emit conferenceChanged(conf);
   }
   else {
      qDebug() << "Trying to affect a conference that does not exist (anymore)";
   }
}

///When a conference is removed
void CallModelBase::conferenceRemovedSlot(const QString &confId)
{
   Call* conf = getCall(confId);
   emit aboutToRemoveConference(conf);
   removeConference(confId);
   emit conferenceRemoved(conf);
}

///When a new voice mail is available
void CallModelBase::voiceMailNotifySlot(const QString &accountID, int count)
{
   qDebug() << "Signal : VoiceMail Notify ! " << count << " new voice mails for account " << accountID;
   emit voiceMailNotify(accountID,count);
}

///When the daemon change the volume
void CallModelBase::volumeChangedSlot(const QString & device, double value)
{
   emit volumeChanged(device,value);
}

///Add a call to the model (reimplemented in .hpp)
Call* CallModelBase::addCall(Call* call, Call* parent)
{
   if (call->getCurrentState() != CALL_STATE_OVER)
      emit callAdded(call,parent);

   connect(call, SIGNAL(isOver(Call*)), this, SLOT(removeActiveCall(Call*)));
   return call;
}

///Emit conference created signal
Call* CallModelBase::addConferenceS(Call* conf)
{
   emit conferenceCreated(conf);
   return conf;
}

///Remove it from active calls
void CallModelBase::removeActiveCall(Call* call)
{
   Q_UNUSED(call);
   //There is a race condition
   //m_sActiveCalls[call->getCallId()] = nullptr;
}

///Updating call state when video is added
void CallModelBase::startedDecoding(const QString& callId, const QString& shmKey  )
{
   Q_UNUSED(callId)
   Q_UNUSED(shmKey)
//    Call* call = getCall(callId);
//    if (call) {
//       
//    }
}

///Updating call state when video is removed
void CallModelBase::stoppedDecoding(const QString& callId, const QString& shmKey)
{
   Q_UNUSED(callId)
   Q_UNUSED(shmKey)
//    Call* call = getCall(callId);
//    if (call) {
//       
//    }
}

/*****************************************************************************
 *                                                                           *
 *                                  Getter                                   *
 *                                                                           *
 ****************************************************************************/

///Return a list of registered accounts
// AccountList* CallModelBase::getAccountList()
// {
//    if (m_spAccountList == NULL) {
//       m_spAccountList = new AccountList(true);
//    }
//    return m_spAccountList;
// }


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Add call slot
void CallModelBase::addPrivateCall(Call* call) {
   addCall(call,0);
}

//More code in CallModel.hpp
