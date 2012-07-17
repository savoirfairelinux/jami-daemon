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

#include "sflphonEngine.h"

//KDE
#include <Plasma/DataContainer>
#include <Plasma/Service>

//SFLPhone
#include "../../lib/Call.h"
#include "../../lib/Account.h"
#include "../../lib/AccountList.h"
#include "../../lib/Contact.h"
#include "../../lib/dbus/metatypes.h"
#include "../../lib/instance_interface_singleton.h"
#include "../../lib/configurationmanager_interface_singleton.h"
#include "../../lib/callmanager_interface_singleton.h"
#include "../../lib/sflphone_const.h"
#include "../../klib/AkonadiBackend.h"
#include "../../klib/HelperFunctions.h"
#include "../../klib/ConfigurationSkeleton.h"
#include "../../lib/CallModel.h"
#include "../../lib/HistoryModel.h"
#include "sflphoneService.h"

//Static
CallModel<>* SFLPhoneEngine::m_pModel = NULL;


/*****************************************************************************
 *                                                                           *
 *                               Constructor                                 *
 *                                                                           *
 ****************************************************************************/

///Constructor
SFLPhoneEngine::SFLPhoneEngine(QObject* parent, const QVariantList& args)
    : Plasma::DataEngine(parent, args)
{
   Q_UNUSED(args)
   if (not m_pModel) {
      m_pModel = new CallModel<>();
      m_pModel->initCall();
      //m_pModel->initHistory();
   }

   /*                SOURCE                             SIGNAL                 DESTINATION              SLOT                   */
   /**/connect(m_pModel                     , SIGNAL( callStateChanged(Call*))  , this , SLOT( callStateChangedSignal(Call*)  ));
   /**/connect(m_pModel                     , SIGNAL( callAdded(Call*))         , this , SLOT( callStateChangedSignal(Call*)  ));
   /**/connect(m_pModel                     , SIGNAL( callStateChanged(Call*))  , this , SLOT( callStateChangedSignal(Call*)  ));
   /**/connect(AkonadiBackend::getInstance(), SIGNAL( collectionChanged())      , this , SLOT( updateCollection()             ));
   /*                                                                                                                          */
   
   
}


/*****************************************************************************
 *                                                                           *
 *                           Dateengine internal                             *
 *                                                                           *
 ****************************************************************************/

///Fill a source only when it is called for the first time, then do it asyncroniously
bool SFLPhoneEngine::sourceRequestEvent(const QString &name)
{
   /*                SOURCE                        CALLBACK         */
   if      ( name == "history"         ) { updateHistory();          }
   else if ( name == "calls"           ) { updateCallList();         }
   else if ( name == "conferences"     ) { updateConferenceList();   }
   else if ( name == "info"            ) { updateInfo();             }
   else if ( name == "accounts"        ) { updateAccounts();         }
   else if ( name == "contacts"        ) { updateContacts();         }
   else if ( name == "bookmark"        ) { updateBookmarkList();     }
   else if ( name.left(7) == "Number:" ) { generateNumberList(name); }
   /*                                                               */
   
   return true;//updateSourceEvent(name);
}

///Not used
bool SFLPhoneEngine::updateSourceEvent(const QString &name)
{
   Q_UNUSED(name)
   return true;
}

///List all default valid sources, more can be requested dynamically
QStringList SFLPhoneEngine::sources() const {
   QStringList toReturn;
   toReturn << "calls" << "history" << "conferences" << "info" << "accounts" << "contacts" << "bookmark";
   return toReturn;
}

///Return the service used for RPC
Plasma::Service* SFLPhoneEngine::serviceForSource(const QString &source)
{
    if (source != "calls") {
        return 0;
    }

    SFLPhoneService* service = new SFLPhoneService(this);
    service->setParent(this);
    return service;
}

/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Return the model
CallModel<>* SFLPhoneEngine::getModel()
{
   return m_pModel;
}


/*****************************************************************************
 *                                                                           *
 *                                Callbacks                                  *
 *                                                                           *
 ****************************************************************************/

///Load/Update history
void SFLPhoneEngine::updateHistory()
{
   CallList list = HistoryModel::getHistory().values();
   setHistoryCategory(list,HistorySortingMode::Date);

   foreach (Call* oldCall, list) {
      HashStringString current;
      /*             KEY                   VALUE                                                               */
      /**/current[ "peerName"   ] = oldCall->getPeerName       ()                                               ;
      /**/current[ "peerNumber" ] = oldCall->getPeerPhoneNumber()                                               ;
      /**/current[ "length"     ] = oldCall->getStopTimeStamp  ().toInt() - oldCall->getStartTimeStamp().toInt();
      /**/current[ "date"       ] = oldCall->getStopTimeStamp  ()                                               ;
      /**/current[ "id"         ] = oldCall->getCallId         ()                                               ;
      /*                                                                                                       */
      if (oldCall->property("section").isValid())
         current[ "section" ] = oldCall->property("section");
      setData("history", oldCall->getCallId() , current);
   }
}

///Load/Update calllist
void SFLPhoneEngine::updateCallList()
{
   //As of KDE 4.8, an empty source are ignored, adding an invisible entry
   QStringList keys;
   keys << "peerName" << "peerNumber" << "stateName" << "state" << "id";
   QHash<QString,QVariant> fake;
   foreach (QString key, keys) {
      fake[key] = "";
   }
   setData("calls", "fake",fake );
   removeAllData("calls");
   foreach (Call* call, m_pModel->getCalls()) {
      if ((!m_pModel->isConference(call)) && (call->getState() != CALL_STATE_OVER)) {
         HashStringString current;
         /*               KEY                     VALUE              */
         /**/current[ "peerName"      ] = call->getPeerName        ( );
         /**/current[ "peerNumber"    ] = call->getPeerPhoneNumber ( );
         /**/current[ "stateName"     ] = call->toHumanStateName   ( );
         /**/current[ "state"         ] = call->getState           ( );
         /**/current[ "id"            ] = call->getCallId          ( );
         /*                                                          */
         setData("calls", call->getCallId(), current);
      }
   }
}

///Load/Update bookmark list
void SFLPhoneEngine::updateBookmarkList()
{
   removeAllData("bookmark");
   int i=0;
   QStringList cl = HistoryModel::getNumbersByPopularity();
   for (;i < ((cl.size() < 10)?cl.size():10);i++) {
      QHash<QString,QVariant> pop;
      Contact* cont = AkonadiBackend::getInstance()->getContactByPhone(cl[i],true);
      /*           KEY                          VALUE                */
      /**/pop["peerName"     ] = (cont)?cont->getFormattedName():cl[i];
      /**/pop["peerNumber"   ] = cl[i]                                ;
      /**/pop["section"      ] = "Popular"                            ;
      /**/pop["listPriority" ] = 1000                                 ;
      /**/pop["id"           ] = i                                    ;
      /*                                                             */
      
      setData("bookmark", QString::number(i), pop);
   }

   //TODO Wont work for now
   foreach (QString nb, ConfigurationSkeleton::bookmarkList()) {
      i++;
      QHash<QString,QVariant> pop;
      /*             KEY          VALUE */
      /**/pop["peerName"     ] = "TODO"  ;
      /**/pop["peerNumber"   ] = nb      ;
      /**/pop["section"      ] = "1"     ;
      /**/pop["listPriority" ] = 0       ;
      /**/pop["id"           ] = i       ;
      /*                                */
      
      setData("bookmark", QString::number(i), pop);
   }
}

///Load/Update conference list (TODO)
void SFLPhoneEngine::updateConferenceList()
{
   /*foreach (Call* call, m_pModel->getCalls()) {
      if (m_pModel->isConference(call)) {
         CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
         currentConferences[call->getConfId()] = callManager.getParticipantList(call->getConfId());
         setData("conferences", call->getConfId(), currentConferences[call->getConfId()]);
      }
   }*/
}

///Update contact collection
void SFLPhoneEngine::updateCollection()
{
   
   typedef QHash<QString,QVariant> SerializedContact;
   ContactList list = AkonadiBackend::getInstance()->update();
   
   if (!list.size())
      return;
   
   ContactHash hash = HelperFunctions::toHash(list);
   foreach (SerializedContact cont, hash) {
      if (!m_hContacts[hash.key(cont)].size()) {
         m_hContacts[hash.key(cont)] = cont;
      }
   }
   
   removeAllData("contacts");
   int i=0;
   foreach (SerializedContact cont, m_hContacts) {
      cont["section"] = "test";
      setData("contacts", QString::number(i), QVariant(cont));
      i++;
   }
   
   updateBookmarkList();
}

///Dummy implementation of the contact list (TOREMOVE)
void SFLPhoneEngine::updateContacts()
{
   //As of KDE 4.8, an empty source is ignored, adding an invisible entry
   QStringList keys;
   keys << "nickName" << "firstName" << "secondName"     << "formattedName" << "organization" << 
                         "Uid"       << "preferredEmail" << "type"          << "group"        <<
                         "department";
   
   QHash<QString,QVariant> fake;
   foreach(QString key,keys) {
      fake[key]="";
   }
   setData("contacts", "fake",fake );
}

///Update other informations
void SFLPhoneEngine::updateInfo()
{
   setData("info", I18N_NOOP("Current_account"), AccountList::getCurrentAccount()->getAccountId());
}

///Load/Update account list
void SFLPhoneEngine::updateAccounts()
{
   const QVector<Account*>& list = AccountList::getInstance()->getAccounts();
   foreach(Account* a,list) {
      if (dynamic_cast<Account*>(a)) {
         QHash<QString,QVariant> acc;
         acc[ "id"   ] = a->getAccountId()                 ;
         acc[ "alias"] = a->getAccountDetail(ACCOUNT_ALIAS);
         setData("accounts", QString::number(rand()) , acc);
      }
   }
}


/*****************************************************************************
 *                                                                           *
 *                                 Mutators                                  *
 *                                                                           *
 ****************************************************************************/

///Generate a number
void SFLPhoneEngine::generateNumberList(QString name)
{
   QString contactUid = name.right(name.size()-7);
   qDebug() << "LOOKING FOR " << contactUid;
   Contact* cont = AkonadiBackend::getInstance()->getContactByUid(contactUid);
   if (cont) {
      foreach(Contact::PhoneNumber* num,cont->getPhoneNumbers()) {
         QHash<QString,QVariant> hash;
         hash[ "number" ] = num->getNumber() ;
         hash[ "type"   ] = num->getType()   ;
         setData(name, QString::number(rand()) , hash);
      }
   }
   else {
      kDebug() << "Contact not found";
   }
}

/*****************************************************************************
 *                                                                           *
 *                                   Slots                                   *
 *                                                                           *
 ****************************************************************************/

///When call state change
void SFLPhoneEngine::callStateChangedSignal(Call* call)
{
   Q_UNUSED(call)
   updateCallList();
}

///When incomming call
void SFLPhoneEngine::incomingCallSignal(Call* call)
{
   Q_UNUSED(call)
   updateCallList();
}

///When incomming messge
void SFLPhoneEngine::incomingMessageSignal(const QString& accountId, const QString& message)
{
   Q_UNUSED(accountId)
   Q_UNUSED(message)
   //TODO
}

///When voicemail notify
void SFLPhoneEngine::voiceMailNotifySignal(const QString& accountId, int count)
{
   Q_UNUSED(accountId)
   Q_UNUSED(count)
   //TODO
}

K_EXPORT_PLASMA_DATAENGINE(sflphone, SFLPhoneEngine)
