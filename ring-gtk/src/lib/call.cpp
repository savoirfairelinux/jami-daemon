/****************************************************************************
 *   Copyright (C) 2009-2014 by Savoir-Faire Linux                          *
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
#include "call.h"

//C include
#include <time.h>

//Qt
#include <QtCore/QFile>
#include <QtCore/QTimer>


//SFLPhone library
#include "dbus/callmanager.h"
#include "dbus/configurationmanager.h"
#include "abstractitembackend.h"
#include "contact.h"
#include "uri.h"
#include "account.h"
#include "accountlistmodel.h"
#include "video/videomodel.h"
#include "historymodel.h"
#include "instantmessagingmodel.h"
#include "useractionmodel.h"
#include "callmodel.h"
#include "numbercategory.h"
#include "phonedirectorymodel.h"
#include "phonenumber.h"
#include "video/videorenderer.h"
#include "tlsmethodmodel.h"
#include "audiosettingsmodel.h"
#include "contactmodel.h"

//Track where state changes are performed on finished (over, error, failed) calls
//while not really problematic, it is technically wrong
#define Q_ASSERT_IS_IN_PROGRESS Q_ASSERT(state() != Call::State::OVER);
#define FORCE_ERROR_STATE() {qDebug() << "Fatal error on " << this << __FILE__ << __LINE__;\
   changeCurrentState(Call::State::ERROR);}

const TypedStateMachine< TypedStateMachine< Call::State , Call::Action> , Call::State> Call::actionPerformedStateMap =
{{
//                           ACCEPT                      REFUSE                  TRANSFER                       HOLD                           RECORD              /**/
/*INCOMING     */  {{Call::State::INCOMING      , Call::State::INCOMING    , Call::State::ERROR        , Call::State::INCOMING     ,  Call::State::INCOMING     }},/**/
/*RINGING      */  {{Call::State::ERROR         , Call::State::RINGING     , Call::State::ERROR        , Call::State::ERROR        ,  Call::State::RINGING      }},/**/
/*CURRENT      */  {{Call::State::ERROR         , Call::State::CURRENT     , Call::State::TRANSFERRED  , Call::State::CURRENT      ,  Call::State::CURRENT      }},/**/
/*DIALING      */  {{Call::State::INITIALIZATION, Call::State::OVER        , Call::State::ERROR        , Call::State::ERROR        ,  Call::State::ERROR        }},/**/
/*HOLD         */  {{Call::State::ERROR         , Call::State::HOLD        , Call::State::TRANSF_HOLD  , Call::State::HOLD         ,  Call::State::HOLD         }},/**/
/*FAILURE      */  {{Call::State::ERROR         , Call::State::OVER        , Call::State::ERROR        , Call::State::ERROR        ,  Call::State::ERROR        }},/**/
/*BUSY         */  {{Call::State::ERROR         , Call::State::BUSY        , Call::State::ERROR        , Call::State::ERROR        ,  Call::State::ERROR        }},/**/
/*TRANSFER     */  {{Call::State::TRANSFERRED   , Call::State::TRANSFERRED , Call::State::CURRENT      , Call::State::TRANSFERRED  ,  Call::State::TRANSFERRED  }},/**/
/*TRANSF_HOLD  */  {{Call::State::TRANSF_HOLD   , Call::State::TRANSF_HOLD , Call::State::HOLD         , Call::State::TRANSF_HOLD  ,  Call::State::TRANSF_HOLD  }},/**/
/*OVER         */  {{Call::State::ERROR         , Call::State::ERROR       , Call::State::ERROR        , Call::State::ERROR        ,  Call::State::ERROR        }},/**/
/*ERROR        */  {{Call::State::ERROR         , Call::State::ERROR       , Call::State::ERROR        , Call::State::ERROR        ,  Call::State::ERROR        }},/**/
/*CONF         */  {{Call::State::ERROR         , Call::State::CURRENT     , Call::State::TRANSFERRED  , Call::State::CURRENT      ,  Call::State::CURRENT      }},/**/
/*CONF_HOLD    */  {{Call::State::ERROR         , Call::State::HOLD        , Call::State::TRANSF_HOLD  , Call::State::HOLD         ,  Call::State::HOLD         }},/**/
/*INIT         */  {{Call::State::INITIALIZATION, Call::State::OVER        , Call::State::ERROR        , Call::State::ERROR        ,  Call::State::ERROR        }},/**/
}};//


const TypedStateMachine< TypedStateMachine< function , Call::Action > , Call::State > Call::actionPerformedFunctionMap =
{{
//                        ACCEPT               REFUSE            TRANSFER                 HOLD                  RECORD             /**/
/*INCOMING       */  {{&Call::accept     , &Call::refuse   , &Call::acceptTransf   , &Call::acceptHold  ,  &Call::setRecord     }},/**/
/*RINGING        */  {{&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::setRecord     }},/**/
/*CURRENT        */  {{&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::hold        ,  &Call::setRecord     }},/**/
/*DIALING        */  {{&Call::call       , &Call::cancel   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       }},/**/
/*HOLD           */  {{&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::unhold      ,  &Call::setRecord     }},/**/
/*FAILURE        */  {{&Call::nothing    , &Call::remove   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       }},/**/
/*BUSY           */  {{&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       }},/**/
/*TRANSFERT      */  {{&Call::transfer   , &Call::hangUp   , &Call::transfer       , &Call::hold        ,  &Call::setRecord     }},/**/
/*TRANSFERT_HOLD */  {{&Call::transfer   , &Call::hangUp   , &Call::transfer       , &Call::unhold      ,  &Call::setRecord     }},/**/
/*OVER           */  {{&Call::nothing    , &Call::nothing  , &Call::nothing        , &Call::nothing     ,  &Call::nothing       }},/**/
/*ERROR          */  {{&Call::nothing    , &Call::remove   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       }},/**/
/*CONF           */  {{&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::hold        ,  &Call::setRecord     }},/**/
/*CONF_HOLD      */  {{&Call::nothing    , &Call::hangUp   , &Call::nothing        , &Call::unhold      ,  &Call::setRecord     }},/**/
/*INITIALIZATION */  {{&Call::call       , &Call::cancel   , &Call::nothing        , &Call::nothing     ,  &Call::nothing       }},/**/
}};//


const TypedStateMachine< TypedStateMachine< Call::State , Call::DaemonState> , Call::State> Call::stateChangedStateMap =
{{
//                        RINGING                   CURRENT                   BUSY                  HOLD                        HUNGUP                 FAILURE           /**/
/*INCOMING     */ {{Call::State::INCOMING    , Call::State::CURRENT    , Call::State::BUSY   , Call::State::HOLD         ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*RINGING      */ {{Call::State::RINGING     , Call::State::CURRENT    , Call::State::BUSY   , Call::State::HOLD         ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*CURRENT      */ {{Call::State::CURRENT     , Call::State::CURRENT    , Call::State::BUSY   , Call::State::HOLD         ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*DIALING      */ {{Call::State::RINGING     , Call::State::CURRENT    , Call::State::BUSY   , Call::State::HOLD         ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*HOLD         */ {{Call::State::HOLD        , Call::State::CURRENT    , Call::State::BUSY   , Call::State::HOLD         ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*FAILURE      */ {{Call::State::FAILURE     , Call::State::FAILURE    , Call::State::BUSY   , Call::State::FAILURE      ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*BUSY         */ {{Call::State::BUSY        , Call::State::CURRENT    , Call::State::BUSY   , Call::State::BUSY         ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*TRANSFER     */ {{Call::State::TRANSFERRED , Call::State::TRANSFERRED, Call::State::BUSY   , Call::State::TRANSF_HOLD  ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*TRANSF_HOLD  */ {{Call::State::TRANSF_HOLD , Call::State::TRANSFERRED, Call::State::BUSY   , Call::State::TRANSF_HOLD  ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*OVER         */ {{Call::State::OVER        , Call::State::OVER       , Call::State::OVER   , Call::State::OVER         ,  Call::State::OVER  ,  Call::State::OVER     }},/**/
/*ERROR        */ {{Call::State::ERROR       , Call::State::ERROR      , Call::State::ERROR  , Call::State::ERROR        ,  Call::State::ERROR ,  Call::State::ERROR    }},/**/
/*CONF         */ {{Call::State::CURRENT     , Call::State::CURRENT    , Call::State::BUSY   , Call::State::HOLD         ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*CONF_HOLD    */ {{Call::State::HOLD        , Call::State::CURRENT    , Call::State::BUSY   , Call::State::HOLD         ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
/*INIT         */ {{Call::State::RINGING     , Call::State::CURRENT    , Call::State::BUSY   , Call::State::HOLD         ,  Call::State::OVER  ,  Call::State::FAILURE  }},/**/
}};//

const TypedStateMachine< TypedStateMachine< function , Call::DaemonState > , Call::State > Call::stateChangedFunctionMap =
{{
//                      RINGING                  CURRENT             BUSY              HOLD                    HUNGUP           FAILURE            /**/
/*INCOMING       */  {{&Call::nothing    , &Call::start     , &Call::startWeird     , &Call::startWeird   ,  &Call::startStop    , &Call::failure }},/**/
/*RINGING        */  {{&Call::nothing    , &Call::start     , &Call::start          , &Call::start        ,  &Call::startStop    , &Call::failure }},/**/
/*CURRENT        */  {{&Call::nothing    , &Call::nothing   , &Call::warning        , &Call::nothing      ,  &Call::stop         , &Call::nothing }},/**/
/*DIALING        */  {{&Call::nothing    , &Call::warning   , &Call::warning        , &Call::warning      ,  &Call::stop         , &Call::warning }},/**/
/*HOLD           */  {{&Call::nothing    , &Call::nothing   , &Call::warning        , &Call::nothing      ,  &Call::stop         , &Call::nothing }},/**/
/*FAILURE        */  {{&Call::nothing    , &Call::warning   , &Call::warning        , &Call::warning      ,  &Call::stop         , &Call::nothing }},/**/
/*BUSY           */  {{&Call::nothing    , &Call::nothing   , &Call::nothing        , &Call::warning      ,  &Call::stop         , &Call::nothing }},/**/
/*TRANSFERT      */  {{&Call::nothing    , &Call::nothing   , &Call::warning        , &Call::nothing      ,  &Call::stop         , &Call::nothing }},/**/
/*TRANSFERT_HOLD */  {{&Call::nothing    , &Call::nothing   , &Call::warning        , &Call::nothing      ,  &Call::stop         , &Call::nothing }},/**/
/*OVER           */  {{&Call::nothing    , &Call::warning   , &Call::warning        , &Call::warning      ,  &Call::stop         , &Call::warning }},/**/
/*ERROR          */  {{&Call::error      , &Call::error     , &Call::error          , &Call::error        ,  &Call::stop         , &Call::error   }},/**/
/*CONF           */  {{&Call::nothing    , &Call::nothing   , &Call::warning        , &Call::nothing      ,  &Call::stop         , &Call::nothing }},/**/
/*CONF_HOLD      */  {{&Call::nothing    , &Call::nothing   , &Call::warning        , &Call::nothing      ,  &Call::stop         , &Call::nothing }},/**/
/*INIT           */  {{&Call::nothing    , &Call::warning   , &Call::warning        , &Call::warning      ,  &Call::stop         , &Call::warning }},/**/
}};//

const TypedStateMachine< Call::LifeCycleState , Call::State > Call::metaStateMap =
{{
/*               *        Life cycle meta-state              **/
/*INCOMING       */   Call::LifeCycleState::INITIALIZATION ,/**/
/*RINGING        */   Call::LifeCycleState::INITIALIZATION ,/**/
/*CURRENT        */   Call::LifeCycleState::PROGRESS       ,/**/
/*DIALING        */   Call::LifeCycleState::INITIALIZATION ,/**/
/*HOLD           */   Call::LifeCycleState::PROGRESS       ,/**/
/*FAILURE        */   Call::LifeCycleState::FINISHED       ,/**/
/*BUSY           */   Call::LifeCycleState::FINISHED       ,/**/
/*TRANSFERT      */   Call::LifeCycleState::PROGRESS       ,/**/
/*TRANSFERT_HOLD */   Call::LifeCycleState::PROGRESS       ,/**/
/*OVER           */   Call::LifeCycleState::FINISHED       ,/**/
/*ERROR          */   Call::LifeCycleState::FINISHED       ,/**/
/*CONF           */   Call::LifeCycleState::PROGRESS       ,/**/
/*CONF_HOLD      */   Call::LifeCycleState::PROGRESS       ,/**/
/*INIT           */   Call::LifeCycleState::INITIALIZATION ,/**/
}};/*                                                        **/

const TypedStateMachine< TypedStateMachine< bool , Call::LifeCycleState > , Call::State > Call::metaStateTransitionValidationMap =
{{
/*               *     INITIALIZATION    PROGRESS      FINISHED   **/
/*INCOMING       */  {{     true     ,    false    ,    false }},/**/
/*RINGING        */  {{     true     ,    false    ,    false }},/**/
/*CURRENT        */  {{     true     ,    true     ,    false }},/**/
/*DIALING        */  {{     true     ,    false    ,    false }},/**/
/*HOLD           */  {{     true     ,    true     ,    false }},/**/
/*FAILURE        */  {{     true     ,    true     ,    false }},/**/
/*BUSY           */  {{     true     ,    false    ,    false }},/**/
/*TRANSFERT      */  {{     false    ,    true     ,    false }},/**/
/*TRANSFERT_HOLD */  {{     false    ,    true     ,    false }},/**/
/*OVER           */  {{     true     ,    true     ,    true  }},/**/
/*ERROR          */  {{     true     ,    true     ,    false }},/**/
/*CONF           */  {{     true     ,    false    ,    false }},/**/
/*CONF_HOLD      */  {{     true     ,    false    ,    false }},/**/
/*INIT           */  {{     true     ,    false    ,    false }},/**/
}};/*                                                             **/
/*^^ A call _can_ be created on hold (conference) and as over (peer hang up before pickup)
 the progress->failure one is an implementation bug*/


QDebug LIB_EXPORT operator<<(QDebug dbg, const Call::State& c)
{
   dbg.nospace() << QString(Call::toHumanStateName(c));
   return dbg.space();
}

QDebug LIB_EXPORT operator<<(QDebug dbg, const Call::DaemonState& c)
{
   dbg.nospace() << static_cast<int>(c);
   return dbg.space();
}

QDebug LIB_EXPORT operator<<(QDebug dbg, const Call::Action& c)
{
   switch (c) {
      case Call::Action::ACCEPT:
         dbg.nospace() << "ACCEPT";
      case Call::Action::REFUSE:
         dbg.nospace() << "REFUSE";
      case Call::Action::TRANSFER:
         dbg.nospace() << "TRANSFER";
      case Call::Action::HOLD:
         dbg.nospace() << "HOLD";
      case Call::Action::RECORD:
         dbg.nospace() << "RECORD";
      case Call::Action::__COUNT:
         dbg.nospace() << "COUNT";
   };
   dbg.space();
   dbg.nospace() << '(' << static_cast<int>(c) << ')';
   return dbg.space();
}

///Constructor
Call::Call(Call::State startState, const QString& callId, const QString& peerName, PhoneNumber* number, Account* account)
   :  QObject(CallModel::instance()),m_pStopTimeStamp(0),
   m_pImModel(nullptr),m_pTimer(nullptr),m_Recording(false),m_Account(nullptr),
   m_PeerName(peerName),m_pPeerPhoneNumber(number),m_HistoryConst(HistoryTimeCategoryModel::HistoryConst::Never),
   m_CallId(callId),m_CurrentState(startState),m_pStartTimeStamp(0),m_pDialNumber(nullptr),m_pTransferNumber(nullptr),
   m_History(false),m_Missed(false),m_Direction(Call::Direction::OUTGOING),m_pBackend(nullptr),m_Type(Call::Type::CALL)
{
   m_Account = account;
   Q_ASSERT(!callId.isEmpty());
   setObjectName("Call:"+callId);
   changeCurrentState(startState);
   m_pUserActionModel = new UserActionModel(this);

   emit changed();
   emit changed(this);
}

///Destructor
Call::~Call()
{
   if (m_pTimer) delete m_pTimer;
   this->disconnect();
   if ( m_pTransferNumber ) delete m_pTransferNumber;
   if ( m_pDialNumber     ) delete m_pDialNumber;
}

///Constructor
Call::Call(const QString& confId, const QString& account): QObject(CallModel::instance()),
   m_pStopTimeStamp(0),m_pStartTimeStamp(0),m_pImModel(nullptr),
   m_Account(AccountListModel::instance()->getAccountById(account)),m_CurrentState(Call::State::CONFERENCE),
   m_pTimer(nullptr),m_pPeerPhoneNumber(nullptr),m_pDialNumber(nullptr),m_pTransferNumber(nullptr),
   m_HistoryConst(HistoryTimeCategoryModel::HistoryConst::Never),m_History(false),m_Missed(false),
   m_Direction(Call::Direction::OUTGOING),m_pBackend(nullptr), m_CallId(confId),
   m_Type((!confId.isEmpty())?Call::Type::CONFERENCE:Call::Type::CALL)
{
   setObjectName("Conf:"+confId);
   m_pUserActionModel = new UserActionModel(this);

   if (type() == Call::Type::CONFERENCE) {
      time_t curTime;
      ::time(&curTime);
      setStartTimeStamp(curTime);
      initTimer();
      CallManagerInterface& callManager = DBus::CallManager::instance();
      MapStringString        details    = callManager.getConferenceDetails(id())  ;
      m_CurrentState = confStatetoCallState(details[ConfDetailsMapFields::CONF_STATE]);
      emit stateChanged();
   }
}

/*****************************************************************************
 *                                                                           *
 *                               Call builder                                *
 *                                                                           *
 ****************************************************************************/

///Build a call from its ID
Call* Call::buildExistingCall(const QString& callId)
{
   CallManagerInterface& callManager = DBus::CallManager::instance();
   MapStringString       details     = callManager.getCallDetails(callId).value();

   //Too noisy
   //qDebug() << "Constructing existing call with details : " << details;

   const QString peerNumber  = details[ Call::DetailsMapFields::PEER_NUMBER ];
   const QString peerName    = details[ Call::DetailsMapFields::PEER_NAME   ];
   const QString account     = details[ Call::DetailsMapFields::ACCOUNT_ID  ];
   Call::State   startState  = startStateFromDaemonCallState(details[Call::DetailsMapFields::STATE], details[Call::DetailsMapFields::TYPE]);
   Account*      acc         = AccountListModel::instance()->getAccountById(account);
   PhoneNumber*  nb          = PhoneDirectoryModel::instance()->getNumber(peerNumber,acc);
   Call*         call        = new Call(startState, callId, peerName, nb, acc);
   call->m_Recording      = callManager.getIsRecording(callId);
   call->m_HistoryState   = historyStateFromType(details[Call::HistoryMapFields::STATE]);

   if (!details[ Call::DetailsMapFields::TIMESTAMP_START ].isEmpty())
      call->setStartTimeStamp(details[ Call::DetailsMapFields::TIMESTAMP_START ].toInt());
   else {
      time_t curTime;
      ::time(&curTime);
      call->setStartTimeStamp(curTime);
   }

   call->initTimer();

   if (call->peerPhoneNumber()) {
      call->peerPhoneNumber()->addCall(call);
   }

   return call;
} //buildExistingCall

///Build a call from a dialing call (a call that is about to exist)
Call* Call::buildDialingCall(const QString& callId, const QString & peerName, Account* account)
{
   Call* call = new Call(Call::State::DIALING, callId, peerName, nullptr, account);
   call->m_HistoryState = Call::LegacyHistoryState::NONE;
   call->m_Direction = Call::Direction::OUTGOING;
   if (AudioSettingsModel::instance()->isRoomToneEnabled()) {
      AudioSettingsModel::instance()->playRoomTone();
   }
   qDebug() << "Created dialing call" << call;
   return call;
}

///Build a call from a dbus event
Call* Call::buildIncomingCall(const QString& callId)
{
   CallManagerInterface & callManager = DBus::CallManager::instance();
   MapStringString details = callManager.getCallDetails(callId).value();

   const QString from     = details[ Call::DetailsMapFields::PEER_NUMBER ];
   const QString account  = details[ Call::DetailsMapFields::ACCOUNT_ID  ];
   const QString peerName = details[ Call::DetailsMapFields::PEER_NAME   ];
   Account*      acc      = AccountListModel::instance()->getAccountById(account);
   PhoneNumber*  nb       = PhoneDirectoryModel::instance()->getNumber(from,acc);
   Call* call = new Call(Call::State::INCOMING, callId, peerName, nb, acc);
   call->m_HistoryState   = Call::LegacyHistoryState::MISSED;
   call->m_Direction      = Call::Direction::INCOMING;
   if (call->peerPhoneNumber()) {
      call->peerPhoneNumber()->addCall(call);
   }
   return call;
} //buildIncomingCall

///Build a ringing call (from dbus)
Call* Call::buildRingingCall(const QString & callId)
{
   CallManagerInterface& callManager = DBus::CallManager::instance();
   MapStringString details = callManager.getCallDetails(callId).value();

   const QString from     = details[ Call::DetailsMapFields::PEER_NUMBER ];
   const QString account  = details[ Call::DetailsMapFields::ACCOUNT_ID  ];
   const QString peerName = details[ Call::DetailsMapFields::PEER_NAME   ];
   Account*      acc      = AccountListModel::instance()->getAccountById(account);
   PhoneNumber*  nb       = PhoneDirectoryModel::instance()->getNumber(from,acc);

   Call* call = new Call(Call::State::RINGING, callId, peerName, nb, acc);
   call->m_HistoryState = LegacyHistoryState::OUTGOING;
   call->m_Direction    = Call::Direction::OUTGOING;

   if (call->peerPhoneNumber()) {
      call->peerPhoneNumber()->addCall(call);
   }
   return call;
} //buildRingingCall


/*****************************************************************************
 *                                                                           *
 *                                  History                                  *
 *                                                                           *
 ****************************************************************************/

///Build a call that is already over
Call* Call::buildHistoryCall(const QMap<QString,QString>& hc)
{
   const QString& callId          = hc[ Call::HistoryMapFields::CALLID          ]          ;
   const QString& name            = hc[ Call::HistoryMapFields::DISPLAY_NAME    ]          ;
   const QString& number          = hc[ Call::HistoryMapFields::PEER_NUMBER     ]          ;
   const QString& type            = hc[ Call::HistoryMapFields::STATE           ]          ;
   const QString& direction       = hc[ Call::HistoryMapFields::DIRECTION       ]          ;
   const bool     missed          = hc[ Call::HistoryMapFields::MISSED          ] == "1";
   time_t         startTimeStamp  = hc[ Call::HistoryMapFields::TIMESTAMP_START ].toUInt() ;
   time_t         stopTimeStamp   = hc[ Call::HistoryMapFields::TIMESTAMP_STOP  ].toUInt() ;
   QString accId                  = hc[ Call::HistoryMapFields::ACCOUNT_ID      ]          ;

   if (accId.isEmpty()) {
      qWarning() << "An history call has an invalid account identifier";
      accId = QString(Account::ProtocolName::IP2IP);
   }

   //Try to assiciate a contact now, the real contact object is probably not
   //loaded yet, but we can get a placeholder for now
//    const QString& contactUsed    = hc[ Call::HistoryMapFields::CONTACT_USED ]; //TODO
   const QString& contactUid     = hc[ Call::HistoryMapFields::CONTACT_UID  ];

   Contact* ct = nullptr;
   if (!hc[ Call::HistoryMapFields::CONTACT_UID].isEmpty())
      ct = ContactModel::instance()->getPlaceHolder(contactUid.toAscii());

   Account*      acc       = AccountListModel::instance()->getAccountById(accId);
   PhoneNumber*  nb        = PhoneDirectoryModel::instance()->getNumber(number,ct,acc);

   Call*         call      = new Call(Call::State::OVER, callId, (name == "empty")?QString():name, nb, acc );

   call->m_pStopTimeStamp  = stopTimeStamp ;
   call->m_History         = true;
   call->setStartTimeStamp(startTimeStamp);
   call->m_HistoryState    = historyStateFromType(type);
   call->m_Account         = AccountListModel::instance()->getAccountById(accId);

   //BEGIN In ~2015, remove the old logic and clean this
   if (missed || call->m_HistoryState == Call::LegacyHistoryState::MISSED) {
      call->m_Missed = true;
      call->m_HistoryState = Call::LegacyHistoryState::MISSED;
   }
   if (!direction.isEmpty()) {
      if (direction == HistoryStateName::INCOMING) {
         call->m_Direction    = Call::Direction::INCOMING         ;
         call->m_HistoryState = Call::LegacyHistoryState::INCOMING;
      }
      else if (direction == HistoryStateName::OUTGOING) {
         call->m_Direction    = Call::Direction::OUTGOING         ;
         call->m_HistoryState = Call::LegacyHistoryState::OUTGOING;
      }
   }
   else if (call->m_HistoryState == Call::LegacyHistoryState::INCOMING)
      call->m_Direction    = Call::Direction::INCOMING            ;
   else if (call->m_HistoryState == Call::LegacyHistoryState::OUTGOING)
      call->m_Direction    = Call::Direction::OUTGOING            ;
   else //Getting there is a bug. Pick one, even if it is the wrong one
      call->m_Direction    = Call::Direction::OUTGOING            ;
   if (missed)
      call->m_HistoryState = Call::LegacyHistoryState::MISSED;
   //END

   call->setObjectName("History:"+call->m_CallId);

   if (call->peerPhoneNumber()) {
      call->peerPhoneNumber()->addCall(call);

      //Reload the glow and number colors
      connect(call->peerPhoneNumber(),SIGNAL(presentChanged(bool)),call,SLOT(updated()));

      //Change the display name and picture
      connect(call->peerPhoneNumber(),SIGNAL(rebased(PhoneNumber*)),call,SLOT(updated()));
   }

   return call;
}

/// aCall << Call::Action::HOLD
Call* Call::operator<<( Call::Action& c)
{
   performAction(c);
   return this;
}

///Get the history state from the type (see Call.cpp header)
Call::LegacyHistoryState Call::historyStateFromType(const QString& type)
{
   if(type == Call::HistoryStateName::MISSED        )
      return Call::LegacyHistoryState::MISSED   ;
   else if(type == Call::HistoryStateName::OUTGOING )
      return Call::LegacyHistoryState::OUTGOING ;
   else if(type == Call::HistoryStateName::INCOMING )
      return Call::LegacyHistoryState::INCOMING ;
   return  Call::LegacyHistoryState::NONE       ;
}

///Get the start sate from the daemon state
Call::State Call::startStateFromDaemonCallState(const QString& daemonCallState, const QString& daemonCallType)
{
   if(daemonCallState      == Call::DaemonStateInit::CURRENT  )
      return Call::State::CURRENT  ;
   else if(daemonCallState == Call::DaemonStateInit::HOLD     )
      return Call::State::HOLD     ;
   else if(daemonCallState == Call::DaemonStateInit::BUSY     )
      return Call::State::BUSY     ;
   else if(daemonCallState == Call::DaemonStateInit::INACTIVE && daemonCallType == Call::CallDirection::INCOMING )
      return Call::State::INCOMING ;
   else if(daemonCallState == Call::DaemonStateInit::INACTIVE && daemonCallType == Call::CallDirection::OUTGOING )
      return Call::State::RINGING  ;
   else if(daemonCallState == Call::DaemonStateInit::INCOMING )
      return Call::State::INCOMING ;
   else if(daemonCallState == Call::DaemonStateInit::RINGING  )
      return Call::State::RINGING  ;
   else
      return Call::State::FAILURE  ;
} //getStartStateFromDaemonCallState


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Transfer state from internal to daemon internal syntaz
Call::DaemonState Call::toDaemonCallState(const QString& stateName)
{
   if(stateName == Call::StateChange::HUNG_UP        )
      return Call::DaemonState::HUNG_UP ;
   if(stateName == Call::StateChange::RINGING        )
      return Call::DaemonState::RINGING ;
   if(stateName == Call::StateChange::CURRENT        )
      return Call::DaemonState::CURRENT ;
   if(stateName == Call::StateChange::UNHOLD_CURRENT )
      return Call::DaemonState::CURRENT ;
   if(stateName == Call::StateChange::HOLD           )
      return Call::DaemonState::HOLD    ;
   if(stateName == Call::StateChange::BUSY           )
      return Call::DaemonState::BUSY    ;
   if(stateName == Call::StateChange::FAILURE        )
      return Call::DaemonState::FAILURE ;

   qDebug() << "stateChanged signal received with unknown state.";
   return Call::DaemonState::FAILURE    ;
} //toDaemonCallState

///Transform a conference call state to a proper call state
Call::State Call::confStatetoCallState(const QString& stateName)
{
   if      ( stateName == Call::ConferenceStateChange::HOLD   )
      return Call::State::CONFERENCE_HOLD;
   else if ( stateName == Call::ConferenceStateChange::ACTIVE )
      return Call::State::CONFERENCE;
   else
      return Call::State::ERROR; //Well, this may bug a little
}

///Transform a backend state into a translated string
const QString Call::toHumanStateName(const Call::State cur)
{
   switch (cur) {
      case Call::State::INCOMING:
         return tr( "Ringing (in)"      );
         break;
      case Call::State::RINGING:
         return tr( "Ringing (out)"     );
         break;
      case Call::State::CURRENT:
         return tr( "Talking"           );
         break;
      case Call::State::DIALING:
         return tr( "Dialing"           );
         break;
      case Call::State::HOLD:
         return tr( "Hold"              );
         break;
      case Call::State::FAILURE:
         return tr( "Failed"            );
         break;
      case Call::State::BUSY:
         return tr( "Busy"              );
         break;
      case Call::State::TRANSFERRED:
         return tr( "Transfer"          );
         break;
      case Call::State::TRANSF_HOLD:
         return tr( "Transfer hold"     );
         break;
      case Call::State::OVER:
         return tr( "Over"              );
         break;
      case Call::State::ERROR:
         return tr( "Error"             );
         break;
      case Call::State::CONFERENCE:
         return tr( "Conference"        );
         break;
      case Call::State::CONFERENCE_HOLD:
         return tr( "Conference (hold)" );
      case Call::State::__COUNT:
         return tr( "ERROR"             );
      case Call::State::INITIALIZATION:
         return tr( "Initialization"    );
      default:
         return QString::number(static_cast<int>(cur));
   }
}

QString Call::toHumanStateName() const
{
   return toHumanStateName(state());
}

///Get the time (second from 1 jan 1970) when the call ended
time_t Call::stopTimeStamp() const
{
   return m_pStopTimeStamp;
}

///Get the time (second from 1 jan 1970) when the call started
time_t Call::startTimeStamp() const
{
   return m_pStartTimeStamp;
}

///Get the number where the call have been transferred
const QString Call::transferNumber() const
{
   return m_pTransferNumber?m_pTransferNumber->uri():QString();
}

///Get the call / peer number
const QString Call::dialNumber() const
{
   if (m_CurrentState != Call::State::DIALING) return QString();
   if (!m_pDialNumber) {
      const_cast<Call*>(this)->m_pDialNumber = new TemporaryPhoneNumber();
   }
   return m_pDialNumber->uri();
}

///Return the call id
const QString Call::id() const
{
   return m_CallId;
}

PhoneNumber* Call::peerPhoneNumber() const
{
   if (m_CurrentState == Call::State::DIALING) {
      if (!m_pTransferNumber) {
         const_cast<Call*>(this)->m_pTransferNumber = new TemporaryPhoneNumber(m_pPeerPhoneNumber);
      }
      if (!m_pDialNumber)
         const_cast<Call*>(this)->m_pDialNumber = new TemporaryPhoneNumber(m_pPeerPhoneNumber);
      return m_pDialNumber;
   }
   return m_pPeerPhoneNumber?m_pPeerPhoneNumber:const_cast<PhoneNumber*>(PhoneNumber::BLANK());
}

///Get the peer name
const QString Call::peerName() const
{
   return m_PeerName;
}

///Generate the best possible peer name
const QString Call::formattedName() const
{
   if (type() == Call::Type::CONFERENCE)
      return tr("Conference");
   else if (!peerPhoneNumber())
      return "Error";
   else if (peerPhoneNumber()->contact() && !peerPhoneNumber()->contact()->formattedName().isEmpty())
      return peerPhoneNumber()->contact()->formattedName();
   else if (!peerName().isEmpty())
      return m_PeerName;
   else if (peerPhoneNumber())
      return peerPhoneNumber()->uri();
   else
      return tr("Unknown");
}

///If the call have a valid record
bool Call::hasRecording() const
{
   return !recordingPath().isEmpty() && QFile::exists(recordingPath());
}

///Generate an human readable string from the difference between StartTimeStamp and StopTimeStamp (or 'now')
QString Call::length() const
{
   if (m_pStartTimeStamp == m_pStopTimeStamp) return QString(); //Invalid
   int nsec =0;
   if (m_pStopTimeStamp)
      nsec = stopTimeStamp() - startTimeStamp();//If the call is over
   else { //Time to now
      time_t curTime;
      ::time(&curTime);
      nsec = curTime - m_pStartTimeStamp;
   }
   if (nsec/3600)
      return QString("%1:%2:%3 ").arg((nsec%(3600*24))/3600).arg(((nsec%(3600*24))%3600)/60,2,10,QChar('0')).arg(((nsec%(3600*24))%3600)%60,2,10,QChar('0'));
   else
      return QString("%1:%2 ").arg(nsec/60,2,10,QChar('0')).arg(nsec%60,2,10,QChar('0'));
}

///Is this call part of history
bool Call::isHistory()
{
   if (lifeCycleState() == Call::LifeCycleState::FINISHED && !m_History)
      m_History = true;
   return m_History;
}

///Is this call missed
bool Call::isMissed() const
{
   return m_Missed || m_HistoryState == Call::LegacyHistoryState::MISSED;
}

///Is the call incoming or outgoing
Call::Direction Call::direction() const
{
   return m_Direction;
}

///Is the call a conference or something else
Call::Type Call::type() const
{
   return m_Type;
}

///Return the backend used to serialize this call
AbstractHistoryBackend* Call::backend() const
{
   return m_pBackend;
}


///Does this call currently has video
bool Call::hasVideo() const
{
   #ifdef ENABLE_VIDEO
   return VideoModel::instance()->getRenderer(this) != nullptr;
   #else
   return false;
   #endif
}


///Get the current state
Call::State Call::state() const
{
   return m_CurrentState;
}

///Translate the state into its life cycle equivalent
Call::LifeCycleState Call::lifeCycleState() const
{
   return metaStateMap[m_CurrentState];
}

///Get the call recording
bool Call::isRecording() const
{
   return m_Recording;
}

///Get the call account id
Account* Call::account() const
{
   return m_Account;
}

///Get the recording path
const QString Call::recordingPath() const
{
   return m_RecordingPath;
}

///Get the history state
Call::LegacyHistoryState Call::historyState() const
{
   return m_HistoryState;
}

///This function could also be called mayBeSecure or haveChancesToBeEncryptedButWeCantTell.
bool Call::isSecure() const
{

   if (!m_Account) {
      qDebug() << "Account not set, can't check security";
      return false;
   }
   //BUG this doesn't work
   return m_Account && ((m_Account->isTlsEnabled()) || (m_Account->tlsMethod() != TlsMethodModel::Type::DEFAULT));
} //isSecure

///Return the renderer associated with this call or nullptr
VideoRenderer* Call::videoRenderer() const
{
   #ifdef ENABLE_VIDEO
   return VideoModel::instance()->getRenderer(this);
   #else
   return nullptr;
   #endif
}


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Set the transfer number
void Call::setTransferNumber(const QString& number)
{
   if (!m_pTransferNumber) {
      m_pTransferNumber = new TemporaryPhoneNumber();
   }
   m_pTransferNumber->setUri(number);
}

///Set the call number
void Call::setDialNumber(const QString& number)
{
   //This is not supposed to happen, but this is not a serious issue if it does
   if (m_CurrentState != Call::State::DIALING) {
      qDebug() << "Trying to set a dial number to a non-dialing call, doing nothing";
      return;
   }

   if (!m_pDialNumber) {
      m_pDialNumber = new TemporaryPhoneNumber();
   }

   m_pDialNumber->setUri(number);
   emit dialNumberChanged(m_pDialNumber->uri());
   emit changed();
   emit changed(this);
}

///Set the dial number from a full phone number
void Call::setDialNumber(const PhoneNumber* number)
{
   if (m_CurrentState == Call::State::DIALING && !m_pDialNumber) {
      m_pDialNumber = new TemporaryPhoneNumber(number);
   }
   if (m_pDialNumber && number)
      m_pDialNumber->setUri(number->uri());
   emit dialNumberChanged(m_pDialNumber->uri());
   emit changed();
   emit changed(this);
}

///Set the recording path
void Call::setRecordingPath(const QString& path)
{
   m_RecordingPath = path;
   if (!m_RecordingPath.isEmpty()) {
      CallManagerInterface & callManager = DBus::CallManager::instance();
      connect(&callManager,SIGNAL(recordPlaybackStopped(QString)), this, SLOT(stopPlayback(QString))  );
      connect(&callManager,SIGNAL(updatePlaybackScale(QString,int,int))  , this, SLOT(updatePlayback(QString,int,int)));
   }
}

///Set peer name
void Call::setPeerName(const QString& name)
{
   m_PeerName = name;
}

///Set the account (DIALING only, may be ignored)
void Call::setAccount( Account* account)
{
   if (state() == Call::State::DIALING)
      m_Account = account;
}

/// Set the backend to save this call to. It is currently impossible to migrate.
void Call::setBackend(AbstractHistoryBackend* backend)
{
   m_pBackend = backend;
}

/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///The call state just changed (by the daemon)
Call::State Call::stateChanged(const QString& newStateName)
{
   const Call::State previousState = m_CurrentState;
   if (type() != Call::Type::CONFERENCE) {
      Call::DaemonState dcs = toDaemonCallState(newStateName);
      if (dcs == Call::DaemonState::__COUNT || m_CurrentState == Call::State::__COUNT) {
         qDebug() << "Error: Invalid state change";
         return Call::State::FAILURE;
      }
//       if (previousState == stateChangedStateMap[m_CurrentState][dcs]) {
// #ifndef NDEBUG
//          qDebug() << "Trying to change state with the same state" << previousState;
// #endif
//          return previousState;
//       }

      try {
         //Validate if the transition respect the expected life cycle
         if (!metaStateTransitionValidationMap[stateChangedStateMap[m_CurrentState][dcs]][lifeCycleState()]) {
            qWarning() << "Unexpected state transition from" << state() << "to" << stateChangedStateMap[m_CurrentState][dcs];
            Q_ASSERT(false);
         }
         changeCurrentState(stateChangedStateMap[m_CurrentState][dcs]);
      }
      catch(Call::State& state) {
         qDebug() << "State change failed (stateChangedStateMap)" << state;
         FORCE_ERROR_STATE()
         return m_CurrentState;
      }
      catch(Call::DaemonState& state) {
         qDebug() << "State change failed (stateChangedStateMap)" << state;
         FORCE_ERROR_STATE()
         return m_CurrentState;
      }
      catch (...) {
         qDebug() << "State change failed (stateChangedStateMap) other";;
         FORCE_ERROR_STATE()
         return m_CurrentState;
      }

      CallManagerInterface & callManager = DBus::CallManager::instance();
      MapStringString details = callManager.getCallDetails(m_CallId).value();
      if (details[Call::DetailsMapFields::PEER_NAME] != m_PeerName)
         m_PeerName = details[Call::DetailsMapFields::PEER_NAME];

      try {
         (this->*(stateChangedFunctionMap[previousState][dcs]))();
      }
      catch(Call::State& state) {
         qDebug() << "State change failed (stateChangedFunctionMap)" << state;
         FORCE_ERROR_STATE()
         return m_CurrentState;
      }
      catch(Call::DaemonState& state) {
         qDebug() << "State change failed (stateChangedFunctionMap)" << state;
         FORCE_ERROR_STATE()
         return m_CurrentState;
      }
      catch (...) {
         qDebug() << "State change failed (stateChangedFunctionMap) other";;
         FORCE_ERROR_STATE()
         return m_CurrentState;
      }
   }
   else {
      //Until now, it does not worth using stateChangedStateMap, conferences are quite simple
      //update 2014: Umm... wrong
      m_CurrentState = confStatetoCallState(newStateName); //TODO don't do this
      emit stateChanged();
   }
   if (m_CurrentState != Call::State::DIALING && m_pDialNumber) {
      if (!m_pPeerPhoneNumber)
         m_pPeerPhoneNumber = PhoneDirectoryModel::instance()->fromTemporary(m_pDialNumber);
      delete m_pDialNumber;
      m_pDialNumber = nullptr;
   }
   emit changed();
   emit changed(this);
   qDebug() << "Calling stateChanged " << newStateName << " -> " << toDaemonCallState(newStateName) << " on call with state " << previousState << ". Become " << m_CurrentState;
   return m_CurrentState;
} //stateChanged

///An account have been performed
Call::State Call::performAction(Call::Action action)
{
   const Call::State previousState = m_CurrentState;

//    if (actionPerformedStateMap[previousState][action] == previousState) {
// #ifndef NDEBUG
//       qDebug() << "Trying to change state with the same state" << previousState;
// #endif
//       return previousState;
//    }

   //update the state
   try {
      changeCurrentState(actionPerformedStateMap[previousState][action]);
   }
   catch(Call::State& state) {
      qDebug() << "State change failed (actionPerformedStateMap)" << state;
      FORCE_ERROR_STATE()
      return Call::State::ERROR;
   }
   catch (...) {
      qDebug() << "State change failed (actionPerformedStateMap) other";;
      FORCE_ERROR_STATE()
      return m_CurrentState;
   }

   //execute the action associated with this transition
   try {
      (this->*(actionPerformedFunctionMap[previousState][action]))();
   }
   catch(Call::State& state) {
      qDebug() << "State change failed (actionPerformedFunctionMap)" << state;
      FORCE_ERROR_STATE()
      return Call::State::ERROR;
   }
   catch(Call::Action& action) {
      qDebug() << "State change failed (actionPerformedFunctionMap)" << action;
      FORCE_ERROR_STATE()
      return Call::State::ERROR;
   }
   catch (...) {
      qDebug() << "State change failed (actionPerformedFunctionMap) other";;
      FORCE_ERROR_STATE()
      return m_CurrentState;
   }
   qDebug() << "Calling action " << action << " on " << id() << " with state " << previousState << ". Become " << m_CurrentState;
   return m_CurrentState;
} //actionPerformed

///Change the state, do not abuse of this, but it is necessary for error cases
void Call::changeCurrentState(Call::State newState)
{
   if (newState == Call::State::__COUNT) {
      qDebug() << "Error: Call reach invalid state";
      FORCE_ERROR_STATE()
      throw newState;
   }

   m_CurrentState = newState;

   emit stateChanged();
   emit changed();
   emit changed(this);

   initTimer();

   if (lifeCycleState() == Call::LifeCycleState::FINISHED)
      emit isOver(this);
}

///Set the start timestamp and update the cache
void Call::setStartTimeStamp(time_t stamp)
{
   m_pStartTimeStamp = stamp;
   //While the HistoryConst is not directly related to the call concept,
   //It is called to often to ignore
   m_HistoryConst = HistoryTimeCategoryModel::timeToHistoryConst(m_pStartTimeStamp);
}

///Send a text message
void Call::sendTextMessage(const QString& message)
{
   CallManagerInterface& callManager = DBus::CallManager::instance();
   Q_NOREPLY callManager.sendTextMessage(m_CallId,message);
   if (!m_pImModel) {
      m_pImModel = InstantMessagingModelManager::instance()->getModel(this);
   }
   m_pImModel->addOutgoingMessage(message);
}


/*****************************************************************************
 *                                                                           *
 *                              Automate function                            *
 *                                                                           *
 ****************************************************************************/
///@warning DO NOT TOUCH THAT, THEY ARE CALLED FROM AN AUTOMATE, HIGH FRAGILITY

///Do nothing (literally)
void Call::nothing()
{
   //nop
}

void Call::error()
{
   if (videoRenderer()) {
      //Well, in this case we have no choice, it still doesn't belong here
      videoRenderer()->stopRendering();
   }
   throw QString("There was an error handling your call, please restart SFLPhone.Is you encounter this problem often, \
   please open SFLPhone-KDE in a terminal and send this last 100 lines before this message in a bug report at \
   https://projects.savoirfairelinux.com/projects/sflphone/issues");
}

///Change history state to failure
void Call::failure()
{
   m_Missed = true;
   //This is how it always was done
   //The main point is to leave the call in the CallList
   start();
}

///Accept the call
void Call::accept()
{
   Q_ASSERT_IS_IN_PROGRESS

   CallManagerInterface & callManager = DBus::CallManager::instance();
   qDebug() << "Accepting call. callId : " << m_CallId  << "ConfId:" << id();
   Q_NOREPLY callManager.accept(m_CallId);
   time_t curTime;
   ::time(&curTime);
   setStartTimeStamp(curTime);
   this->m_HistoryState = LegacyHistoryState::INCOMING;
   m_Direction = Call::Direction::INCOMING;
}

///Refuse the call
void Call::refuse()
{
   CallManagerInterface & callManager = DBus::CallManager::instance();
   qDebug() << "Refusing call. callId : " << m_CallId  << "ConfId:" << id();
   const bool ret = callManager.refuse(m_CallId);
   time_t curTime;
   ::time(&curTime);
   setStartTimeStamp(curTime);
   this->m_HistoryState = Call::LegacyHistoryState::MISSED;
   m_Missed = true;

   //If the daemon crashed then re-spawned when a call is ringing, this happen.
   if (!ret)
      FORCE_ERROR_STATE()
}

///Accept the transfer
void Call::acceptTransf()
{
   Q_ASSERT_IS_IN_PROGRESS

   if (!m_pTransferNumber) {
      qDebug() << "Trying to transfer to no one";
      return;
   }
   CallManagerInterface & callManager = DBus::CallManager::instance();
   qDebug() << "Accepting call and transferring it to number : " << m_pTransferNumber->uri() << ". callId : " << m_CallId  << "ConfId:" << id();
   callManager.accept(m_CallId);
   Q_NOREPLY callManager.transfer(m_CallId, m_pTransferNumber->uri());
}

///Put the call on hold
void Call::acceptHold()
{
   Q_ASSERT_IS_IN_PROGRESS

   CallManagerInterface & callManager = DBus::CallManager::instance();
   qDebug() << "Accepting call and holding it. callId : " << m_CallId  << "ConfId:" << id();
   callManager.accept(m_CallId);
   Q_NOREPLY callManager.hold(m_CallId);
   this->m_HistoryState = LegacyHistoryState::INCOMING;
   m_Direction = Call::Direction::INCOMING;
}

///Hang up
void Call::hangUp()
{
   Q_ASSERT_IS_IN_PROGRESS

   CallManagerInterface & callManager = DBus::CallManager::instance();
   time_t curTime;
   ::time(&curTime);
   m_pStopTimeStamp = curTime;
   qDebug() << "Hanging up call. callId : " << m_CallId << "ConfId:" << id();
   bool ret;
   if (videoRenderer()) { //TODO remove, cheap hack
      videoRenderer()->stopRendering();
   }
   if (type() != Call::Type::CONFERENCE)
      ret = callManager.hangUp(m_CallId);
   else
      ret = callManager.hangUpConference(id());
   if (!ret) { //Can happen if the daemon crash and open again
      qDebug() << "Error: Invalid call, the daemon may have crashed";
      changeCurrentState(Call::State::OVER);
   }
   if (m_pTimer)
      m_pTimer->stop();
}

///Remove the call without contacting the daemon
void Call::remove()
{
   if (lifeCycleState() != Call::LifeCycleState::FINISHED)
      FORCE_ERROR_STATE()

   CallManagerInterface & callManager = DBus::CallManager::instance();

   //HACK Call hang up again to make sure the busytone stop, this should
   //return true or false, both are valid, no point to check the result
   if (type() != Call::Type::CONFERENCE)
      callManager.hangUp(m_CallId);
   else
      callManager.hangUpConference(id());

   emit isOver(this);
   emit stateChanged();
   emit changed();
   emit changed(this);
}

///Cancel this call
void Call::cancel()
{
   //This one can be over if the peer server failed to comply with the correct sequence
   CallManagerInterface & callManager = DBus::CallManager::instance();
   qDebug() << "Canceling call. callId : " << m_CallId  << "ConfId:" << id();
   emit dialNumberChanged(QString());
//    Q_NOREPLY callManager.hangUp(m_CallId);
   if (!callManager.hangUp(m_CallId)) {
      qWarning() << "HangUp failed, the call was probably already over";
      changeCurrentState(Call::State::OVER);
   }
}

///Put on hold
void Call::hold()
{
   Q_ASSERT_IS_IN_PROGRESS

   CallManagerInterface & callManager = DBus::CallManager::instance();
   qDebug() << "Holding call. callId : " << m_CallId << "ConfId:" << id();
   if (type() != Call::Type::CONFERENCE)
      Q_NOREPLY callManager.hold(m_CallId);
   else
      Q_NOREPLY callManager.holdConference(id());
}

///Start the call
void Call::call()
{
   Q_ASSERT_IS_IN_PROGRESS

   CallManagerInterface& callManager = DBus::CallManager::instance();
   qDebug() << "account = " << m_Account;
   if(!m_Account) {
      qDebug() << "Account is not set, taking the first registered.";
      this->m_Account = AccountListModel::currentAccount();
   }
   //Calls to empty URI should not be allowed, sflphoned will go crazy
   if ((!m_pDialNumber) || m_pDialNumber->uri().isEmpty()) {
      qDebug() << "Trying to call an empty URI";
      changeCurrentState(Call::State::FAILURE);
      if (!m_pDialNumber) {
         emit dialNumberChanged(QString());
      }
      else {
         delete m_pDialNumber;
         m_pDialNumber = nullptr;
      }
      setPeerName(tr("Failure"));
      emit stateChanged();
      emit changed();
   }
   //Normal case
   else if(m_Account) {
      qDebug() << "Calling " << peerPhoneNumber()->uri() << " with account " << m_Account << ". callId : " << m_CallId  << "ConfId:" << id();
      callManager.placeCall(m_Account->id(), m_CallId, m_pDialNumber->uri());
      this->m_pPeerPhoneNumber = PhoneDirectoryModel::instance()->getNumber(m_pDialNumber->uri(),account());
      if (ContactModel::instance()->hasBackends()) {
         if (peerPhoneNumber()->contact())
            m_PeerName = peerPhoneNumber()->contact()->formattedName();
      }
      connect(peerPhoneNumber(),SIGNAL(presentChanged(bool)),this,SLOT(updated()));
      time_t curTime;
      ::time(&curTime);
      setStartTimeStamp(curTime);
      this->m_HistoryState = LegacyHistoryState::OUTGOING;
      m_Direction = Call::Direction::OUTGOING;
      if (peerPhoneNumber()) {
         peerPhoneNumber()->addCall(this);
      }
      if (m_pDialNumber)
         emit dialNumberChanged(QString());
      delete m_pDialNumber;
      m_pDialNumber = nullptr;
   }
   else {
      qDebug() << "Trying to call " << (m_pTransferNumber?QString(m_pTransferNumber->uri()):"ERROR")
         << " with no account registered . callId : " << m_CallId  << "ConfId:" << id();
      this->m_HistoryState = LegacyHistoryState::NONE;
      throw tr("No account registered!");
   }
}

///Trnasfer the call
void Call::transfer()
{
   Q_ASSERT_IS_IN_PROGRESS

   if (m_pTransferNumber) {
      CallManagerInterface & callManager = DBus::CallManager::instance();
      qDebug() << "Transferring call to number : " << m_pTransferNumber->uri() << ". callId : " << m_CallId;
      Q_NOREPLY callManager.transfer(m_CallId, m_pTransferNumber->uri());
      time_t curTime;
      ::time(&curTime);
      m_pStopTimeStamp = curTime;
   }
}

///Unhold the call
void Call::unhold()
{
   Q_ASSERT_IS_IN_PROGRESS

   CallManagerInterface & callManager = DBus::CallManager::instance();
   qDebug() << "Unholding call. callId : " << m_CallId  << "ConfId:" << id();
   if (type() != Call::Type::CONFERENCE)
      Q_NOREPLY callManager.unhold(m_CallId);
   else
      Q_NOREPLY callManager.unholdConference(id());
}

///Record the call
void Call::setRecord()
{
   CallManagerInterface & callManager = DBus::CallManager::instance();
   qDebug() << "Setting record " << !m_Recording << " for call. callId : " << m_CallId  << "ConfId:" << id();
   callManager.toggleRecording(id());
}

///Start the timer
void Call::start()
{
   qDebug() << "Starting call. callId : " << m_CallId  << "ConfId:" << id();
   time_t curTime;
   ::time(&curTime);
   emit changed();
   emit changed(this);
   if (m_pDialNumber) {
      if (!m_pPeerPhoneNumber)
         m_pPeerPhoneNumber = PhoneDirectoryModel::instance()->fromTemporary(m_pDialNumber);
      delete m_pDialNumber;
      m_pDialNumber = nullptr;
   }
   setStartTimeStamp(curTime);
}

///Toggle the timer
void Call::startStop()
{
   qDebug() << "Starting and stoping call. callId : " << m_CallId  << "ConfId:" << id();
   time_t curTime;
   ::time(&curTime);
   setStartTimeStamp(curTime);
   m_pStopTimeStamp  = curTime;
}

///Stop the timer
void Call::stop()
{
   qDebug() << "Stoping call. callId : " << m_CallId  << "ConfId:" << id();
   if (videoRenderer()) { //TODO remove, cheap hack
      videoRenderer()->stopRendering();
   }
   time_t curTime;
   ::time(&curTime);
   m_pStopTimeStamp = curTime;
}

///Handle error instead of crashing
void Call::startWeird()
{
   qDebug() << "Starting call. callId : " << m_CallId  << "ConfId:" << id();
   time_t curTime;
   ::time(&curTime);
   setStartTimeStamp(curTime);
   qDebug() << "Warning : call " << m_CallId << " had an unexpected transition of state at its start.";
}

///Print a warning
void Call::warning()
{
   qWarning() << "Warning : call " << m_CallId << " had an unexpected transition of state.(" << m_CurrentState << ")";
   switch (m_CurrentState) {
      case Call::State::FAILURE        :
      case Call::State::ERROR          :
      case Call::State::__COUNT          :
         //If not stopped, then the counter will keep going
         //Getting here indicate something wrong happened
         //It can be normal, aka, an invalid URI such as '><'
         // or an SFLPhone-KDE bug
         stop();
         break;
      case Call::State::TRANSFERRED    :
      case Call::State::TRANSF_HOLD    :
      case Call::State::DIALING        :
      case Call::State::INITIALIZATION:
      case Call::State::INCOMING       :
      case Call::State::RINGING        :
      case Call::State::CURRENT        :
      case Call::State::HOLD           :
      case Call::State::BUSY           :
      case Call::State::OVER           :
      case Call::State::CONFERENCE     :
      case Call::State::CONFERENCE_HOLD:
      default:
         break;
   }
}

/*****************************************************************************
 *                                                                           *
 *                             Keyboard handling                             *
 *                                                                           *
 ****************************************************************************/

///Input text on the call item
void Call::appendText(const QString& str)
{
   TemporaryPhoneNumber* editNumber = nullptr;

   switch (m_CurrentState) {
   case Call::State::TRANSFERRED :
   case Call::State::TRANSF_HOLD :
      editNumber = m_pTransferNumber;
      break;
   case Call::State::DIALING     :
      editNumber = m_pDialNumber;
      break;
   case Call::State::INITIALIZATION:
   case Call::State::INCOMING:
   case Call::State::RINGING:
   case Call::State::CURRENT:
   case Call::State::HOLD:
   case Call::State::FAILURE:
   case Call::State::BUSY:
   case Call::State::OVER:
   case Call::State::ERROR:
   case Call::State::CONFERENCE:
   case Call::State::CONFERENCE_HOLD:
   case Call::State::__COUNT:
   default:
      qDebug() << "Backspace on call not editable. Doing nothing.";
      return;
   }

   if (editNumber) {
      editNumber->setUri(editNumber->uri()+str);
      if (state() == Call::State::DIALING)
         emit dialNumberChanged(editNumber->uri());
   }
   else
      qDebug() << "TemporaryPhoneNumber not defined";


   emit changed();
   emit changed(this);
}

///Remove the last character
void Call::backspaceItemText()
{
   TemporaryPhoneNumber* editNumber = nullptr;

   switch (m_CurrentState) {
      case Call::State::TRANSFERRED      :
      case Call::State::TRANSF_HOLD      :
         editNumber = m_pTransferNumber;
         break;
      case Call::State::DIALING          :
         editNumber = m_pDialNumber;
         break;
      case Call::State::INITIALIZATION:
      case Call::State::INCOMING:
      case Call::State::RINGING:
      case Call::State::CURRENT:
      case Call::State::HOLD:
      case Call::State::FAILURE:
      case Call::State::BUSY:
      case Call::State::OVER:
      case Call::State::ERROR:
      case Call::State::CONFERENCE:
      case Call::State::CONFERENCE_HOLD:
      case Call::State::__COUNT:
      default                          :
         qDebug() << "Backspace on call not editable. Doing nothing.";
         return;
   }
   if (editNumber) {
      QString text = editNumber->uri();
      const int textSize = text.size();
      if(textSize > 0) {
         editNumber->setUri(text.remove(textSize-1, 1));
         emit changed();
         emit changed(this);
      }
      else {
         changeCurrentState(Call::State::OVER);
      }
   }
   else
      qDebug() << "TemporaryPhoneNumber not defined";
}

///Reset the string a dialing or transfer call
void Call::reset()
{
   TemporaryPhoneNumber* editNumber = nullptr;

   switch (m_CurrentState) {
      case Call::State::TRANSFERRED      :
      case Call::State::TRANSF_HOLD      :
         editNumber = m_pTransferNumber;
         break;
      case Call::State::DIALING          :
         editNumber = m_pDialNumber;
         break;
      case Call::State::INITIALIZATION   :
      case Call::State::INCOMING         :
      case Call::State::RINGING          :
      case Call::State::CURRENT          :
      case Call::State::HOLD             :
      case Call::State::FAILURE          :
      case Call::State::BUSY             :
      case Call::State::OVER             :
      case Call::State::ERROR            :
      case Call::State::CONFERENCE       :
      case Call::State::CONFERENCE_HOLD  :
      case Call::State::__COUNT:
      default                            :
         qDebug() << "Cannot reset" << m_CurrentState << "calls";
         return;
   }
   if (editNumber) {
      editNumber->setUri(QString());
   }
}

/*****************************************************************************
 *                                                                           *
 *                                   SLOTS                                   *
 *                                                                           *
 ****************************************************************************/

void Call::updated()
{
   emit changed();
   emit changed(this);
}

///Play the record, if any
void Call::playRecording()
{
   CallManagerInterface& callManager = DBus::CallManager::instance();
   const bool retval = callManager.startRecordedFilePlayback(recordingPath());
   if (retval)
      emit playbackStarted();
}

///Stop the record, if any
void Call::stopRecording()
{
   CallManagerInterface& callManager = DBus::CallManager::instance();
   Q_NOREPLY callManager.stopRecordedFilePlayback(recordingPath());
   emit playbackStopped(); //TODO remove this, it is a workaround for bug #11942
}

///seek the record, if any
void Call::seekRecording(double position)
{
   CallManagerInterface& callManager = DBus::CallManager::instance();
   Q_NOREPLY callManager.recordPlaybackSeek(position);
}

///Daemon record playback stopped
void Call::stopPlayback(const QString& filePath)
{
   if (filePath == recordingPath()) {
      emit playbackStopped();
   }
}

///Daemon playback position chnaged
void Call::updatePlayback(const QString& path, int position,int size)
{
   if (path == m_RecordingPath) {
      emit playbackPositionChanged(position,size);
   }
}

UserActionModel* Call::userActionModel() const
{
   return m_pUserActionModel;
}

///Check if creating a timer is necessary
void Call::initTimer()
{
   if (lifeCycleState() == Call::LifeCycleState::PROGRESS) {
      if (!m_pTimer) {
         m_pTimer = new QTimer(this);
         m_pTimer->setInterval(1000);
         connect(m_pTimer,SIGNAL(timeout()),this,SLOT(updated()));
      }
      if (!m_pTimer->isActive())
         m_pTimer->start();
   }
   else if (m_pTimer && lifeCycleState() != Call::LifeCycleState::PROGRESS) {
      m_pTimer->stop();
      delete m_pTimer;
      m_pTimer = nullptr;
   }
}

///Common source for model data roles
QVariant Call::roleData(int role) const
{
   const Contact* ct = peerPhoneNumber()?peerPhoneNumber()->contact():nullptr;
   switch (role) {
      case Call::Role::Name:
      case Qt::DisplayRole:
         if (type() == Call::Type::CONFERENCE)
            return tr("Conference");
         else if (state() == Call::State::DIALING)
            return dialNumber();
         else if (m_PeerName.isEmpty())
            return ct?ct->formattedName():peerPhoneNumber()?peerPhoneNumber()->uri():dialNumber();
         else
            return formattedName();
         break;
      case Qt::ToolTipRole:
         return tr("Account: ") + (account()?account()->alias():QString());
         break;
      case Qt::EditRole:
         return dialNumber();
      case Call::Role::Number:
         return peerPhoneNumber()->uri();
         break;
      case Call::Role::Direction2:
         return static_cast<int>(m_Direction); //TODO Qt5, use the Q_ENUM
         break;
      case Call::Role::Date:
         return (int)startTimeStamp();
         break;
      case Call::Role::Length:
         return length();
         break;
      case Call::Role::FormattedDate:
         return QDateTime::fromTime_t(startTimeStamp()).toString();
         break;
      case Call::Role::HasRecording:
         return hasRecording();
         break;
      case Call::Role::Historystate:
         return static_cast<int>(historyState());
         break;
      case Call::Role::Filter: {
         QString normStripppedC;
         foreach(QChar char2,QString(static_cast<int>(historyState())+'\n'+roleData(Call::Role::Name).toString()+'\n'+
            roleData(Call::Role::Number).toString()).toLower().normalized(QString::NormalizationForm_KD) ) {
            if (!char2.combiningClass())
               normStripppedC += char2;
         }
         return normStripppedC;
         }
         break;
      case Call::Role::FuzzyDate:
         return (int)m_HistoryConst; //TODO Qt5, use the Q_ENUM
         break;
      case Call::Role::IsBookmark:
         return false;
         break;
      case Call::Role::Security:
         return isSecure();
         break;
      case Call::Role::Department:
         return ct?ct->department():QVariant();
         break;
      case Call::Role::Email:
         return ct?ct->preferredEmail():QVariant();
         break;
      case Call::Role::Organisation:
         return ct?ct->organization():QVariant();
         break;
      case Call::Role::Object:
         return QVariant::fromValue(const_cast<Call*>(this));
         break;
      case Call::Role::PhoneNu:
         return QVariant::fromValue(const_cast<PhoneNumber*>(peerPhoneNumber()));
         break;
      case Call::Role::PhotoPtr:
         return QVariant::fromValue((void*)(ct?ct->photo():nullptr));
         break;
      case Call::Role::CallState:
         return static_cast<int>(state()); //TODO Qt5, use the Q_ENUM
         break;
      case Call::Role::Id:
         return id();
         break;
      case Call::Role::StartTime:
         return (int) m_pStartTimeStamp;
      case Call::Role::StopTime:
         return (int) m_pStopTimeStamp;
      case Call::Role::IsRecording:
         return isRecording();
      case Call::Role::IsPresent:
         return peerPhoneNumber()->isPresent();
      case Call::Role::IsTracked:
         return peerPhoneNumber()->isTracked();
      case Call::Role::SupportPresence:
         return peerPhoneNumber()->supportPresence();
      case Call::Role::CategoryIcon:
         return peerPhoneNumber()->category()->icon(peerPhoneNumber()->isTracked(),peerPhoneNumber()->isPresent());
      case Call::Role::CallCount:
         return peerPhoneNumber()->callCount();
      case Call::Role::TotalSpentTime:
         return peerPhoneNumber()->totalSpentTime();
      case Call::Role::DropState:
         return property("dropState");
         break;
      case Call::Role::Missed:
         return isMissed();
      case Call::Role::CallLifeCycleState:
         return static_cast<int>(lifeCycleState()); //TODO Qt5, use the Q_ENUM
      case Call::Role::DTMFAnimState:
         return property("DTMFAnimState");
         break;
      case Call::Role::LastDTMFidx:
         return property("latestDtmfIdx");
         break;
      case Call::Role::DropPosition:
         return property("dropPosition");
         break;
      default:
         break;
   };
   return QVariant();
}


void Call::playDTMF(const QString& str)
{
   Q_NOREPLY DBus::CallManager::instance().playDTMF(str);
   emit dtmfPlayed(str);
}

#undef Q_ASSERT_IS_IN_PROGRESS
#undef FORCE_ERROR_STATE
