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

//Parent
#include <CallModel.h>
#include <HistoryModel.h>

bool CallModelBase::dbusInit = false;
CallMap CallModelBase::m_sActiveCalls;

///Constructor
CallModelBase::CallModelBase(QObject* parent) : QObject(parent)
{
   if (!dbusInit) {
      CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
      
      //SLOTS
      //             SENDER                                        SIGNAL                                      RECEIVER                             SLOT                                    /
      /**/connect(&callManager, SIGNAL( callStateChanged  (const QString &, const QString &                  ) ), this , SLOT( callStateChanged      ( const QString &, const QString & ) ) );
      /**/connect(&callManager, SIGNAL( incomingCall      (const QString &, const QString &, const QString & ) ), this , SLOT( incomingCall          ( const QString &, const QString & ) ) );
      /**/connect(&callManager, SIGNAL( conferenceCreated (const QString &                                   ) ), this , SLOT( incomingConference    ( const QString &                  ) ) );
      /**/connect(&callManager, SIGNAL( conferenceChanged (const QString &, const QString &                  ) ), this , SLOT( changingConference    ( const QString &, const QString & ) ) );
      /**/connect(&callManager, SIGNAL( conferenceRemoved (const QString &                                   ) ), this , SLOT( conferenceRemovedSlot ( const QString &                  ) ) );
      /**/connect(&callManager, SIGNAL( voiceMailNotify   (const QString &, int                              ) ), this , SLOT( voiceMailNotifySlot   ( const QString &, int             ) ) );
      /**/connect(&callManager, SIGNAL( volumeChanged     (const QString &, double                           ) ), this , SLOT( volumeChangedSlot     ( const QString &, double          ) ) );
      /*                                                                                                                                                                                    */

      connect(HistoryModel::self(),SIGNAL(newHistoryCall(Call*)),this,SLOT(addPrivateCall(Call*)));
      
      connect(&callManager, SIGNAL(registrationStateChanged(QString,QString,int)),this,SLOT(accountChanged(QString,QString,int)));
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
   //This code is part of the CallModel iterface too
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

///Account status changed
void CallModelBase::accountChanged(const QString& account,const QString& state, int code)
{
   Q_UNUSED(code)
   Account* a = AccountList::getInstance()->getAccountById(account);
   if (a) {
      emit accountStateChanged(a,a->getStateName(state));
   }
}

///Remove it from active calls
void CallModelBase::removeActiveCall(Call* call)
{
   Q_UNUSED(call);
   //There is a race condition
   //m_sActiveCalls[call->getCallId()] = nullptr;
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