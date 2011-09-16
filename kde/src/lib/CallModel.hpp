//Static member
template  <typename CallWidget, typename Index> QString CallModel<CallWidget,Index>::priorAccountId;
template  <typename CallWidget, typename Index> AccountList* CallModel<CallWidget,Index>::accountList =0;
template  <typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::instanceInit(false);
template  <typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::callInit(false);
template  <typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::historyInit(false);

template  <typename CallWidget, typename Index> QHash<QString, Call*> CallModel<CallWidget,Index>::activeCalls;
template  <typename CallWidget, typename Index> QHash<QString, Call*> CallModel<CallWidget,Index>::historyCalls;
// template  <typename CallWidget, typename Index> QHash<Call*, InternalCallModelStruct* > CallModel<CallWidget,Index>::privateCallList_call;
// template  <typename CallWidget, typename Index> QHash<QString, InternalCallModelStruct* > CallModel<CallWidget,Index>::privateCallList_callId;/*
template  <typename CallWidget, typename Index> typename CallModel<CallWidget,Index>::InternalCall   CallModel<CallWidget,Index>::privateCallList_call;
template  <typename CallWidget, typename Index> typename CallModel<CallWidget,Index>::InternalCallId CallModel<CallWidget,Index>::privateCallList_callId;
template  <typename CallWidget, typename Index> typename CallModel<CallWidget,Index>::InternalIndex  CallModel<CallWidget,Index>::privateCallList_index;
template  <typename CallWidget, typename Index> typename CallModel<CallWidget,Index>::InternalWidget CallModel<CallWidget,Index>::privateCallList_widget;

/*****************************************************************************
 *                                                                           *
 *                               Constructor                                 *
 *                                                                           *
 ****************************************************************************/

///Retrieve current and older calls from the daemon, fill history and the calls TreeView and enable drag n' drop
template<typename CallWidget, typename Index> CallModel<CallWidget,Index>::CallModel(ModelType type)
{
   Q_UNUSED(type)
   init();

}

//Open the connection to the daemon and register this client
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::init() 
{
   if (!instanceInit) {
      registerCommTypes();
      InstanceInterface& instance = InstanceInterfaceSingleton::getInstance();
      instance.Register(getpid(), APP_NAME);
      
      //Setup accounts
      if (accountList == NULL)
	 accountList = new AccountList(true);
   }
   instanceInit = true;
   return true;
}

//Fill the call list
//@warning This solution wont scale to multiple call or history model implementation. Some static addCall + foreach for each call would be needed if this case ever become unavoidable
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::initCall()
{
   if (!callInit) {
      CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
      QStringList callList = callManager.getCallList();
      foreach (QString callId, callList) {
         Call* tmpCall = Call::buildExistingCall(callId);
         activeCalls[tmpCall->getCallId()] = tmpCall;
         addCall(tmpCall);
      }
   
      QStringList confList = callManager.getConferenceList();
      foreach (QString confId, confList) {
          addConference(confId);
      }
   }
   callInit = true;
   return true;
}

//Fill the history list
//@warning This solution wont scale to multiple call or history model implementation. Some static addCall + foreach for each call would be needed if this case ever become unavoidable
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::initHistory()
{
   if (!historyInit) {
      ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
      QStringList historyMap = configurationManager.getHistory().value();
      qDebug() << "\n\n\n\n\n\n\n\nCall History = " ;
      foreach (QString historyCallId, historyMap) {
         QStringList param = historyCallId.split("|");
         qDebug() << "Param count" << param.size();
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
            historyCalls[time_start] = Call::buildHistoryCall(callID, time_start.toUInt(), time_stop.toUInt(), accountID, peer_name, peer_number, history_state);
            addCall(historyCalls[time_start]);
         }

//          QString type2 = param[0];
//          QString number = param[1];
//          QString name = param[2];
//          uint stopTimeStamp = param[3].toUInt();
//          QString account = param[4];
      }
   }
   historyInit = true;
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
   return activeCalls.size();
}

///Return a call corresponding to this ID or NULL
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::findCallByCallId(QString callId) 
{
   return activeCalls[callId];
}

///Return the action call list
template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCallList() 
{
   QList<Call*> callList;
   foreach(Call* call, activeCalls) {
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
   InternalCallModelStruct* aNewStruct = new InternalCallModelStruct;
   aNewStruct->call_real = call;
   aNewStruct->conference = false;
   
   privateCallList_call[call] =  aNewStruct;
   privateCallList_callId[call->getCallId()] = aNewStruct;

   //setCurrentItem(callItem);
   
   return call;
}

///Create a new dialing call from peer name and the account ID
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::addDialingCall(const QString & peerName, QString account)
{
   Call* call = Call::buildDialingCall(generateCallId(), peerName, account);
   activeCalls[call->getCallId()] = call;
   addCall(call);
   selectItem(call);
   return call;
}

///Create a new incomming call when the daemon is being called
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::addIncomingCall(const QString & callId)
{
   Call* call = Call::buildIncomingCall(callId);
   activeCalls[call->getCallId()] = call;
   addCall(call);
   selectItem(call);
   return call;
}

///Create a ringing call
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::addRingingCall(const QString & callId)
{
   Call* call = Call::buildRingingCall(callId);
   activeCalls[call->getCallId()] = call;
   addCall(call);
   selectItem(call);
   return call;
}

///Generate a new random call unique identifier (callId)
template<typename CallWidget, typename Index> QString CallModel<CallWidget,Index>::generateCallId()
{
   int id = qrand();
   QString res = QString::number(id);
   return res;
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

   if (!privateCallList_callId[callList[0]]) {
      qDebug() << "Invalid call";
      return 0;
   }
   Call* newConf =  new Call(confID, privateCallList_callId[callList[0]]->call_real->getAccountId());
   
   InternalCallModelStruct* aNewStruct = new InternalCallModelStruct;
   aNewStruct->call_real = newConf;
//    aNewStruct->conference = true;
   
   //Index* confItem = new Index();
   //aNewStruct->treeItem = confItem;
   
   privateCallList_call[newConf] = aNewStruct;
   privateCallList_callId[newConf->getConfId()] = aNewStruct; //WARNING It may break something is it is done wrong
   
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
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::conferenceChanged(const QString& confId, const QString& state) 
{
   Q_UNUSED(state)
   
   if (!privateCallList_callId[confId]) {
      qDebug() << "The conference does not exist";
      return false;
   }
   
   if (!privateCallList_callId[confId]->treeItem) {
      qDebug() << "The conference item does not exist";
      return false;
   }
   return true;
}

///Remove a conference from the model and the TreeView
template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::conferenceRemoved(const QString &confId) 
{
   qDebug() << "Ending conversation containing " << privateCallList_callId[confId]->children.size() << " participants";
   removeConference(getCall(confId));
}


template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::removeConference(Call* call)
{
   InternalCallModelStruct* internal = privateCallList_call[call];
   
   if (!internal) {
      qDebug() << "Cannot remove conference: call not found";
      return;
   }
//    foreach (InternalCallModelStruct* child, internal) {
//       removeCall(internal->call_real);
//    }
   removeCall(call);
}

template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::removeCall(Call* call)
{
   InternalCallModelStruct* internal = privateCallList_call[call];
   
   if (!internal) {
      qDebug() << "Cannot remove call: call not found";
      return;
   }
   
   if (privateCallList_call[call] != NULL) {
      privateCallList_call.remove(call);
   }
   
   if (privateCallList_callId[privateCallList_callId.key(internal)] == internal) {
      privateCallList_callId.remove(privateCallList_callId.key(internal));
   }
   
   if (privateCallList_widget[privateCallList_widget.key(internal)] == internal) {
      privateCallList_widget.remove(privateCallList_widget.key(internal));
   }
   
   if (privateCallList_index[privateCallList_index.key(internal)] == internal) {
      privateCallList_index.remove(privateCallList_index.key(internal));
   }
}


/*****************************************************************************
 *                                                                           *
 *                           History related code                            *
 *                                                                           *
 ****************************************************************************/

///Return a list of all previous calls
template<typename CallWidget, typename Index> QStringList CallModel<CallWidget,Index>::getHistoryCallId() 
{
   QStringList toReturn;
   foreach(Call* call, historyCalls) {
      toReturn << call->getCallId();
   }
   return toReturn;
}

template<typename CallWidget, typename Index> const QHash<QString, Call*> CallModel<CallWidget,Index>::getHistory()
{
   return historyCalls;
}

/*****************************************************************************
 *                                                                           *
 *                           Account related code                            *
 *                                                                           *
 ****************************************************************************/

//Return the current account id (do not put in the cpp file)
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


      //Return the current account
template<typename CallWidget, typename Index> Account* CallModel<CallWidget,Index>::getCurrentAccount()
{
   Account* priorAccount = getAccountList()->getAccountById(priorAccountId);
   if(priorAccount && priorAccount->getAccountDetail(ACCOUNT_STATUS) == ACCOUNT_STATE_REGISTERED ) {
      return priorAccount;
   }
   else {
      qDebug() << "Returning the first account" << getAccountList()->size();
      return getAccountList()->firstRegisteredAccount();
   }
}

//Return a list of registered accounts
template<typename CallWidget, typename Index> AccountList* CallModel<CallWidget,Index>::getAccountList()
{
   if (accountList == NULL) {
      accountList = new AccountList(true);
   }
   return accountList;
}

//Return the previously used account ID
template<typename CallWidget, typename Index> QString CallModel<CallWidget,Index>::getPriorAccoundId() 
{
   return priorAccountId;
}

//Set the previous account used
template<typename CallWidget, typename Index> void CallModel<CallWidget,Index>::setPriorAccountId(QString value) {
   priorAccountId = value;
}

/*****************************************************************************
 *                                                                           *
 *                             Magic Dispatcher                              *
 *                                                                           *
 ****************************************************************************/
template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::getCall(const CallWidget widget) const
{
   if (privateCallList_widget[widget]) {
      return privateCallList_widget[widget]->call_real;
   }
   return NULL;
}

template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getTreeItem(const CallWidget widget) const
{
   if (privateCallList_widget[widget]) {
      return privateCallList_widget[widget]->treeItem;
   }
   return NULL;
}

template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCalls(const CallWidget widget) const
{
   QList<Call*> toReturn;
   if (privateCallList_widget[widget] && privateCallList_widget[widget]->conference) {
      foreach (InternalCallModelStruct* child, privateCallList_widget[widget]->children) {
	 toReturn << child.call_real;
      }
   }
   return toReturn;
}

template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCalls()
{
   QList<Call*> toReturn;
   foreach (InternalCallModelStruct* child, privateCallList_call) {
      toReturn << child->call_real;
   }
   return toReturn;
}

template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::isConference(const CallWidget widget) const
{
   if (privateCallList_widget[widget]) {
      return privateCallList_widget[widget]->conference;
   }
   return false;
}

template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::isConference(const Call* call) const
{
   if (privateCallList_call[(Call*)call]) {
      return privateCallList_call[(Call*)call]->conference;
   }
   return false;
}

template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::getCall(const Call* call) const
{ 
   return call;
}

template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getTreeItem(const Call* call) const
{ 
   if (privateCallList_call[call]) {
      return privateCallList_call[call]->treeItem;
   }
   return NULL;
}

template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCalls(const Call* call) const
{ 
   QList<Call*> toReturn;
   if (privateCallList_call[call] && privateCallList_call[call]->conference) {
      foreach (InternalCallModelStruct* child, privateCallList_call[call]->children) {
	 toReturn << child.call_real;
      }
   }
   return toReturn;
}
template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::isConference(const Index idx) const
{ 
   if (privateCallList_index[idx]) {
      return privateCallList_index[idx]->conference;
   }
   return false;
}

template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::getCall(const Index idx) const
{ 
   if (privateCallList_index[idx]) {
      return privateCallList_index[idx]->call_real;
   }
   qDebug() << "Call not found";
   return NULL;
}

template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getTreeItem(const Index idx) const
{ 
   if (privateCallList_index[idx]) {
      return privateCallList_index[idx]->treeItem;
   }
   return NULL;
}

template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCalls(const Index idx) const
{ 
   QList<Call*> toReturn;
   if (privateCallList_index[idx] && privateCallList_index[idx]->conference) {
      foreach (InternalCallModelStruct* child, privateCallList_index[idx]->children) {
	 toReturn << child.call_real;
      }
   }
   return toReturn;
}

template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::isConference(const QString callId) const
{ 
   if (privateCallList_callId[callId]) {
      return privateCallList_callId[callId]->conference;
   }
   return false;
}

template<typename CallWidget, typename Index> Call* CallModel<CallWidget,Index>::getCall(const QString callId) const
{ 
   if (privateCallList_callId[callId]) {
      return privateCallList_callId[callId]->call_real;
   }
   return NULL;
}

template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getTreeItem(const QString callId) const
{ 
   if (privateCallList_callId[callId]) {
      return privateCallList_callId[callId]->treeItem;
   }
   return NULL;
}

template<typename CallWidget, typename Index> QList<Call*> CallModel<CallWidget,Index>::getCalls(const QString callId) const
{
   QList<Call*> toReturn;
   if (privateCallList_callId[callId] && privateCallList_callId[callId]->conference) {
      foreach (InternalCallModelStruct* child, privateCallList_callId[callId]->children) {
	 toReturn << child.callId_real;
      }
   }
   return toReturn;
}

template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::updateIndex(Call* call, Index value)
{ 
   if (!privateCallList_call[call]) {
      privateCallList_call[call] = new InternalCallModelStruct;
      privateCallList_call[call]->call_real = call;
      privateCallList_call[call]->conference = false;
      privateCallList_callId[call->getCallId()] = privateCallList_call[call];
   }
   privateCallList_call[call]->treeItem = value;
   privateCallList_index[value] = privateCallList_call[call];
   return true;
}

template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getIndex(const Call* call) const
{
   if (privateCallList_call[(Call*)call]) {
      return privateCallList_call[(Call*)call]->treeItem;
   }
   return NULL;
}

template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getIndex(const Index idx) const
{
   if (privateCallList_index[idx]) {
      return privateCallList_index[idx]->treeItem;
   }
   return NULL;
}

template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getIndex(const CallWidget widget) const
{
   if (privateCallList_widget[widget]) {
      return privateCallList_widget[widget]->treeItem;
   }
   return NULL;
}

template<typename CallWidget, typename Index> Index CallModel<CallWidget,Index>::getIndex(const QString callId) const
{
   if (privateCallList_callId[callId]) {
      return privateCallList_callId[callId]->treeItem;
   }
   return NULL;
}

template<typename CallWidget, typename Index> CallWidget CallModel<CallWidget,Index>::getWidget(const Call* call) const
{
   if (privateCallList_call[call]) {
      return privateCallList_call[call]->call;
   }
   return NULL;
}

template<typename CallWidget, typename Index> CallWidget CallModel<CallWidget,Index>::getWidget(const Index idx) const
{
   if (privateCallList_index[idx]) {
      return privateCallList_index[idx]->call;
   }
   return NULL;
}

template<typename CallWidget, typename Index> CallWidget CallModel<CallWidget,Index>::getWidget(const CallWidget widget) const
{
   if (privateCallList_widget[widget]) {
      return privateCallList_widget[widget]->call;
   }
   return NULL;
}

template<typename CallWidget, typename Index> CallWidget CallModel<CallWidget,Index>::getWidget(const QString widget) const
{
   if (privateCallList_widget[widget]) {
      return privateCallList_widget[widget]->call;
   }
   return NULL;
}

template<typename CallWidget, typename Index> bool CallModel<CallWidget,Index>::updateWidget(Call* call, CallWidget value)
{
   if (!privateCallList_call[call]) {
      privateCallList_call[call] = new InternalCallModelStruct;
      privateCallList_call[call]->call_real = call;
      privateCallList_call[call]->conference = false;
      privateCallList_callId[call->getCallId()] = privateCallList_call[call];
   }
   privateCallList_call[call]->call = value;
   privateCallList_widget[value] = privateCallList_call[call];
   return true;
}