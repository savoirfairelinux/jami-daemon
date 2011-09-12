#include "sflphonEngine.h"
 
#include <Plasma/DataContainer>

#include "../../src/lib/Call.h"
#include "../../src/lib/dbus/metatypes.h"
#include "../../src/lib/instance_interface_singleton.h"
#include "../../src/lib/configurationmanager_interface_singleton.h"
#include "../../src/lib/callmanager_interface_singleton.h"
#include "../../src/lib/sflphone_const.h"
 
SFLPhoneEngine::SFLPhoneEngine(QObject* parent, const QVariantList& args)
    : Plasma::DataEngine(parent, args), CallModelConvenience(ActiveCall)
{
   Q_UNUSED(args)
   initCall();
   initHistory();

   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();

   connect(&callManager, SIGNAL(callStateChanged(const QString&, const QString&)),
           this,         SLOT(callStateChangedSignal(const QString&, const QString&)));
   connect(&callManager, SIGNAL(incomingCall(const QString&, const QString&, const QString&)),
           this,         SLOT(incomingCallSignal(const QString&, const QString&)));
   connect(&callManager, SIGNAL(conferenceCreated(const QString&)),
           this,         SLOT(conferenceCreatedSignal(const QString&)));
   connect(&callManager, SIGNAL(conferenceChanged(const QString&, const QString&)),
           this,         SLOT(conferenceChangedSignal(const QString&, const QString&)));
   connect(&callManager, SIGNAL(conferenceRemoved(const QString&)),
           this,         SLOT(conferenceRemovedSignal(const QString&)));
   connect(&callManager, SIGNAL(incomingMessage(const QString&, const QString&)),
           this,         SLOT(incomingMessageSignal(const QString&, const QString&)));
   connect(&callManager, SIGNAL(voiceMailNotify(const QString&, int)),
           this,         SLOT(voiceMailNotifySignal(const QString&, int)));
   connect(&configurationManager, SIGNAL(accountsChanged()),
           this,         SLOT(accountChanged()));
   connect(&configurationManager, SIGNAL(accountsChanged()),
           getAccountList(), SLOT(updateAccounts()));
    
    //setMinimumPollingInterval(1000);
}
 
bool SFLPhoneEngine::sourceRequestEvent(const QString &name)
{
   if (name == "history") {
      updateHistory();
   }
   else if (name == "calls") {
      updateCallList();
   }
   else if (name == "conferences") {
      updateConferenceList();
   }
   else if (name == "info") {
      updateInfo();
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
   toReturn << "calls" << "history" << "conferences" << "info";
   return toReturn;
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
   foreach (Call* oldCall, historyCalls) {
      historyCall[oldCall->getCallId()]["Name"] = oldCall->getPeerName();
      historyCall[oldCall->getCallId()]["Number"] = oldCall->getPeerPhoneNumber();
      historyCall[oldCall->getCallId()]["Date"] = oldCall->getStopTimeStamp();
      setData("history", I18N_NOOP(oldCall->getCallId()), historyCall[oldCall->getCallId()]);
   }
}

void SFLPhoneEngine::updateCallList()
{
   foreach (Call* call, getCalls()) {
      if ((!isConference(call)) && (call->getState() != CALL_STATE_OVER)) {
         currentCall[call->getCallId()]["Name"] = call->getPeerName();
         currentCall[call->getCallId()]["Number"] = call->getPeerPhoneNumber();
         currentCall[call->getCallId()]["StateName"] = getCallStateName(call->getState());
         currentCall[call->getCallId()]["State"] = call->getState();
         setData("calls", call->getCallId(), currentCall[call->getCallId()]);
      }
   }
}

void SFLPhoneEngine::updateConferenceList()
{
   foreach (Call* call, getCalls()) {
      if (isConference(call)) {
         CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
         currentConferences[call->getConfId()] = callManager.getParticipantList(call->getConfId());
         setData("conferences", call->getConfId(), currentConferences[call->getConfId()]);
      }
   }
}

void SFLPhoneEngine::updateContacts()
{
   
}

void SFLPhoneEngine::updateInfo() 
{
   qDebug() << "Currentaccount: " << getCurrentAccountId();
   setData("info", I18N_NOOP("Account"), getCurrentAccountId());
}

void SFLPhoneEngine::callStateChangedSignal(const QString& callId, const QString& state)
{
   qDebug() << "Signal : Call State Changed for call  " << callId << " . New state : " << state;
   Call* call = findCallByCallId(callId);
   if(!call) {
      if(state == CALL_STATE_CHANGE_RINGING) {
         call = addRingingCall(callId);
         //addCallToCallList(call);
      }
      else {
         qDebug() << "Call doesn't exist in this client. Might have been initialized by another client instance before this one started.";
         return;
      }
   }
   else {
      call->stateChanged(state);
   }
   updateCallList();
}

void SFLPhoneEngine::incomingCallSignal(const QString& accountId, const QString& callId)
{
   Q_UNUSED(accountId)
   addIncomingCall(callId);
   updateCallList();
}

void SFLPhoneEngine::conferenceCreatedSignal(const QString& confId)
{
   addConference(confId);
   updateConferenceList();
}

void SFLPhoneEngine::conferenceChangedSignal(const QString& confId, const QString& state)
{
   conferenceChanged(confId, state);
   updateConferenceList();
}

void SFLPhoneEngine::conferenceRemovedSignal(const QString& confId)
{
   conferenceRemoved(confId);
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

K_EXPORT_PLASMA_DATAENGINE(sflphone, SFLPhoneEngine)
