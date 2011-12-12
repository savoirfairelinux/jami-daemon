#include "sflphonEngine.h"
 
#include <Plasma/DataContainer>

#include "../../src/lib/Call.h"
#include "../../src/lib/dbus/metatypes.h"
#include "../../src/lib/instance_interface_singleton.h"
#include "../../src/lib/configurationmanager_interface_singleton.h"
#include "../../src/lib/callmanager_interface_singleton.h"
#include "../../src/lib/sflphone_const.h"
 
SFLPhoneEngine::SFLPhoneEngine(QObject* parent, const QVariantList& args)
    : Plasma::DataEngine(parent, args)
{
   Q_UNUSED(args)
   m_pModel = new CallModelConvenience(CallModelConvenience::ActiveCall);
   m_pModel->initCall();
   m_pModel->initHistory();
   
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();

   connect(m_pModel              , SIGNAL( callStateChanged(Call*))  , this , SLOT(callStateChangedSignal(Call*)  ));
   connect(&callManager          , SIGNAL( incomingCall(Call*))      , this , SLOT(incomingCallSignal(Call*)      ));
   connect(&callManager          , SIGNAL( conferenceCreated(Call*)) , this , SLOT(conferenceCreatedSignal(Call*) ));
   connect(&callManager          , SIGNAL( conferenceChanged(Call*)) , this , SLOT(conferenceChangedSignal(Call*) ));
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
   foreach (Call* oldCall, m_pModel->getHistory()) {
      historyCall[oldCall->getCallId()][ "Name"   ] = oldCall->getPeerName();
      historyCall[oldCall->getCallId()][ "Number" ] = oldCall->getPeerPhoneNumber();
      historyCall[oldCall->getCallId()][ "Date"   ] = oldCall->getStopTimeStamp();
      setData("history", I18N_NOOP(oldCall->getCallId()), historyCall[oldCall->getCallId()]);
   }
}

void SFLPhoneEngine::updateCallList()
{
   foreach (Call* call, m_pModel->getCalls()) {
      if ((!m_pModel->isConference(call)) && (call->getState() != CALL_STATE_OVER)) {
         currentCall[call->getCallId()][ "Name"      ] = call->getPeerName();
         currentCall[call->getCallId()][ "Number"    ] = call->getPeerPhoneNumber();
         currentCall[call->getCallId()][ "StateName" ] = getCallStateName(call->getState());
         currentCall[call->getCallId()][ "State"     ] = call->getState();
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

void SFLPhoneEngine::updateContacts()
{
   
}

void SFLPhoneEngine::updateInfo() 
{
   qDebug() << "Currentaccount: " << m_pModel->getCurrentAccountId();
   setData("info", I18N_NOOP("Account"), m_pModel->getCurrentAccountId());
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

K_EXPORT_PLASMA_DATAENGINE(sflphone, SFLPhoneEngine)
