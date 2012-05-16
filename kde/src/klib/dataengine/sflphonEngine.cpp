#include "sflphonEngine.h"

#include <Plasma/DataContainer>

#include "../../lib/Call.h"
#include "../../lib/Account.h"
#include "../../lib/AccountList.h"
#include "../../lib/dbus/metatypes.h"
#include "../../lib/instance_interface_singleton.h"
#include "../../lib/configurationmanager_interface_singleton.h"
#include "../../lib/callmanager_interface_singleton.h"
#include "../../lib/sflphone_const.h"
#include "../../klib/AkonadiBackend.h"
#include "../../klib/HelperFunctions.h"
#include "sflphoneService.h">

CallModel<>* SFLPhoneEngine::m_pModel = NULL;

SFLPhoneEngine::SFLPhoneEngine(QObject* parent, const QVariantList& args)
    : Plasma::DataEngine(parent, args)
{
   Q_UNUSED(args)
   if (not m_pModel) {
      m_pModel = new CallModel<>(CallModel<>::ActiveCall);
      m_pModel->initCall();
      m_pModel->initHistory();
   }

   //CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();

   connect(m_pModel                     , SIGNAL( callStateChanged(Call*))  , this , SLOT(callStateChangedSignal(Call*)  ));
   //connect(&callManager                 , SIGNAL( incomingCall(Call*))      , this , SLOT(incomingCallSignal(Call*)      ));
   //connect(&callManager                 , SIGNAL( conferenceCreated(Call*)) , this , SLOT(conferenceCreatedSignal(Call*) ));
   //connect(&callManager                 , SIGNAL( conferenceChanged(Call*)) , this , SLOT(conferenceChangedSignal(Call*) ));
   connect(AkonadiBackend::getInstance(), SIGNAL( collectionChanged())      , this , SLOT(updateCollection()             ));
   
}

bool SFLPhoneEngine::sourceRequestEvent(const QString &name)
{
   if      ( name == "history"     ) {
      updateHistory();
   }
   else if ( name == "calls"       ) {
      updateCallList();
   }
   else if ( name == "conferences" ) {
      updateConferenceList();
   }
   else if ( name == "info"        ) {
      updateInfo();
   }
   else if ( name == "accounts"    ) {
      updateAccounts();
   }
   else if ( name == "contacts"    ) {
      updateContacts();
   }
   return true;//updateSourceEvent(name);
}

bool SFLPhoneEngine::updateSourceEvent(const QString &name)
{
   Q_UNUSED(name)
   return true;
}

QStringList SFLPhoneEngine::sources() const {
   QStringList toReturn;
   toReturn << "calls" << "history" << "conferences" << "info" << "accounts" << "contacts";
   return toReturn;
}

Plasma::Service* SFLPhoneEngine::serviceForSource(const QString &source)
{
    if (source != "calls") {
        return 0;
    }

    SFLPhoneService *service = new SFLPhoneService(this);
    service->setParent(this);
    return service;
}

QString SFLPhoneEngine::getCallStateName(call_state state)
{
   if (state == CALL_STATE_INCOMING) {
      return I18N_NOOP("Ringing (in)");
   } else if (state == CALL_STATE_RINGING) {
      return I18N_NOOP("Ringing (out)");
   } else if (state == CALL_STATE_CURRENT) {
      return I18N_NOOP("Talking");
   } else if (state == CALL_STATE_DIALING) {
      return I18N_NOOP("Dialing");
   } else if (state == CALL_STATE_HOLD) {
      return I18N_NOOP("Hold");
   } else if (state == CALL_STATE_FAILURE) {
      return I18N_NOOP("Failed");
   } else if (state == CALL_STATE_BUSY) {
      return I18N_NOOP("Busy");
   } else if (state == CALL_STATE_TRANSFER) {
      return I18N_NOOP("Transfer");
   } else if (state == CALL_STATE_TRANSF_HOLD) {
      return I18N_NOOP("Transfer hold");
   } else if (state == CALL_STATE_OVER) {
      return I18N_NOOP("Over");
   } else if (state == CALL_STATE_ERROR) {
      return I18N_NOOP("Error");
   }
   return "";
}

void SFLPhoneEngine::updateHistory()
{
   CallList list = m_pModel->getHistory().values();
   setHistoryCategory(list,HistorySortingMode::Date);

   foreach (Call* oldCall, list) {
      qDebug() << oldCall->getStopTimeStamp();
      historyCall[oldCall->getCallId()][ "peerName"   ] = oldCall->getPeerName();
      historyCall[oldCall->getCallId()][ "peerNumber" ] = oldCall->getPeerPhoneNumber();
      historyCall[oldCall->getCallId()][ "length"     ] = oldCall->getStopTimeStamp().toInt() - oldCall->getStartTimeStamp().toInt();
      historyCall[oldCall->getCallId()][ "date"       ] = oldCall->getStopTimeStamp();
      if (oldCall->property("section").isValid())
         historyCall[oldCall->getCallId()][ "section"    ] = oldCall->property("section");
      setData("history", oldCall->getCallId() , historyCall[oldCall->getCallId()]);
   }
}

void SFLPhoneEngine::updateCallList()
{
   foreach (Call* call, m_pModel->getCalls()) {
      if ((!m_pModel->isConference(call)) && (call->getState() != CALL_STATE_OVER)) {
         currentCall[call->getCallId()][ "peerName"      ] = call->getPeerName();
         currentCall[call->getCallId()][ "peerNumber"    ] = call->getPeerPhoneNumber();
         currentCall[call->getCallId()][ "stateName"     ] = getCallStateName(call->getState());
         currentCall[call->getCallId()][ "state"         ] = call->getState();
         setData("calls", call->getCallId(), currentCall[call->getCallId()]);
      }
   }
}

void SFLPhoneEngine::updateConferenceList()
{
   foreach (Call* call, m_pModel->getCalls()) {
      if (m_pModel->isConference(call)) {
         CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
         currentConferences[call->getConfId()] = callManager.getParticipantList(call->getConfId());
         setData("conferences", call->getConfId(), currentConferences[call->getConfId()]);
      }
   }
}

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
      //
   }
   removeAllData("contacts");
   int i=0;
   foreach (SerializedContact cont, m_hContacts) {
      cont["section"] = "test";
      setData("contacts", QString::number(i), QVariant(cont));
      i++;
   }
}

void SFLPhoneEngine::updateContacts()
{
   QHash<QString,QVariant> test;
   test[ "nickName"       ] = "";
   test[ "firstName"      ] = "";
   test[ "secondName"     ] = "";
   test[ "formattedName"  ] = "";
   test[ "organization"   ] = "";
   test[ "Uid"            ] = "";
   test[ "preferredEmail" ] = "";
   test[ "type"           ] = "";
   test[ "group"          ] = "";
   test[ "department"     ] = "";
   setData("contacts", "fake",test );
}

void SFLPhoneEngine::updateInfo()
{
   setData("info", I18N_NOOP("Current_account"), m_pModel->getCurrentAccountId());
}

void SFLPhoneEngine::updateAccounts()
{
   const QVector<Account*>& list = m_pModel->getAccountList()->getAccounts();
   foreach(Account* a,list) {
      setData("accounts", a->getAccountDetail(ACCOUNT_ALIAS), a->getAccountId());
   }
}

void SFLPhoneEngine::callStateChangedSignal(Call* call)
{
   Q_UNUSED(call)
   updateCallList();
}

void SFLPhoneEngine::incomingCallSignal(Call* call)
{
   Q_UNUSED(call)
   updateCallList();
}

void SFLPhoneEngine::conferenceCreatedSignal(Call* conf)
{
   Q_UNUSED(conf)
   updateConferenceList();
}

void SFLPhoneEngine::conferenceChangedSignal(Call* conf)
{
   Q_UNUSED(conf)
   updateConferenceList();
}

void SFLPhoneEngine::incomingMessageSignal(const QString& accountId, const QString& message)
{
   Q_UNUSED(accountId)
   Q_UNUSED(message)
   //TODO
}

void SFLPhoneEngine::voiceMailNotifySignal(const QString& accountId, int count)
{
   Q_UNUSED(accountId)
   Q_UNUSED(count)
   //TODO
}

void SFLPhoneEngine::accountChanged()
{
   
}

CallModel<>* SFLPhoneEngine::getModel()
{
   return m_pModel;
}

K_EXPORT_PLASMA_DATAENGINE(sflphone, SFLPhoneEngine)
