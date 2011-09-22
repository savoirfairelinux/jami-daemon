#include "CallView.h"
#include "AkonadiBackend.h"
#include "lib/Contact.h"

#include <QtGui/QInputDialog>

///Retrieve current and older calls from the daemon, fill history and the calls TreeView and enable drag n' drop
CallView::CallView(ModelType type, QWidget* parent) : QTreeWidget(parent), TreeWidgetCallModel(type)
{
   if (type == ActiveCall)
      initCall();
   else if (type == History)
      initHistory();
   
   //Widget part
   setAcceptDrops(true);
   setDragEnabled(true);
   CallTreeItemDelegate *delegate = new CallTreeItemDelegate();
   setItemDelegate(delegate); 
   setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding));

   //User Interface events
   connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(itemDoubleClicked(QTreeWidgetItem*,int)));
   connect(this, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(itemClicked(QTreeWidgetItem*,int)));
   

   
   //D-Bus event    
//    CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
//    connect(&callManager, SIGNAL(callStateChanged(const QString &, const QString &)),
//            this,         SLOT(callStateChangedSignal(const QString &, const QString &)));
//    connect(&callManager, SIGNAL(incomingCall(const QString &, const QString &, const QString &)),
//            this,         SLOT(incomingCallSignal(const QString &, const QString &)));
//    connect(&callManager, SIGNAL(conferenceCreated(const QString &)),
//            this,         SLOT(conferenceCreatedSignal(const QString &)));
//    connect(&callManager, SIGNAL(conferenceChanged(const QString &, const QString &)),
//            this,         SLOT(conferenceChangedSignal(const QString &, const QString &)));
//    connect(&callManager, SIGNAL(conferenceRemoved(const QString &)),
//            this,         SLOT(conferenceRemovedSignal(const QString &)));
//    connect(&callManager, SIGNAL(incomingMessage(const QString &, const QString &)),
//            this,         SLOT(incomingMessageSignal(const QString &, const QString &)));
//    connect(&callManager, SIGNAL(voiceMailNotify(const QString &, int)),
//            this,         SLOT(voiceMailNotifySignal(const QString &, int)));
}


/*****************************************************************************
 *                                                                           *
 *                        Drag and drop related code                         *
 *                                                                           *
 ****************************************************************************/

bool CallView::callToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   Q_UNUSED(index)
   Q_UNUSED(action)
   QByteArray encodedCallId      = data->data( MIME_CALLID      );
   if (!QString(encodedCallId).isEmpty()) {
      clearArtefact(getIndex(encodedCallId));

      if (!parent) {
         qDebug() << "Call dropped on empty space";
         if (getIndex(encodedCallId)->parent()) {
            qDebug() << "Detaching participant";
            detachParticipant(getCall(encodedCallId));
         }
         else
            qDebug() << "The call is not in a conversation (doing nothing)";
         return true;
      }

      if (getCall(parent)->getCallId() == QString(encodedCallId)) {
         qDebug() << "Call dropped on itself (doing nothing)";
         return true;
      }

      if ((parent->childCount()) && (getIndex(encodedCallId)->childCount())) {
         qDebug() << "Merging two conferences";
         mergeConferences(getCall(parent),getCall(encodedCallId));
         return true;
      }
      else if ((parent->parent()) || (parent->childCount())) {
         qDebug() << "Call dropped on a conference";

         if ((getIndex(encodedCallId)->childCount()) && (!parent->childCount())) {
            qDebug() << "Conference dropped on a call (doing nothing)";
            return true;
         }

         QTreeWidgetItem* call1 = getIndex(encodedCallId);
         QTreeWidgetItem* call2 = (parent->parent())?parent->parent():parent;

         if (call1->parent()) {
            qDebug() << "Call 1 is part of a conference";
            if (call1->parent() == call2) {
               qDebug() << "Call dropped on it's own conference (doing nothing)";
               return true;
            }
            else if (getIndex(call1)->childCount()) {
               qDebug() << "Merging two conferences";
               mergeConferences(getCall(call1),getCall(call2));
            }
            else if (call1->parent()) {
               qDebug() << "Moving call from a conference to an other";
               detachParticipant(getCall(encodedCallId));
            }
         }
         qDebug() << "Adding participant";
         int state = getCall(call1)->getState();
         if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD) {
            getCall(call1)->actionPerformed(CALL_ACTION_ACCEPT);
         }
         state = getCall(call2)->getState();
         if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD) {
            getCall(call2)->actionPerformed(CALL_ACTION_ACCEPT);
         }
         addParticipant(getCall(call1),getCall(call2));
         return true;
      }
      else if ((getIndex(encodedCallId)->childCount()) && (!parent->childCount())) {
         qDebug() << "Call dropped on it's own conference (doing nothing)";
         return true;
      }



      qDebug() << "Call dropped on another call";
      createConferenceFromCall(getCall(encodedCallId),getCall(parent));
      return true;
   }
   return false;
}

bool CallView::phoneNumberToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   Q_UNUSED(index)
   Q_UNUSED(action)
   QByteArray encodedPhoneNumber = data->data( MIME_PHONENUMBER );
   if (!QString(encodedPhoneNumber).isEmpty()) {
      Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(encodedPhoneNumber);
      QString name;
      if (contact)
         name = contact->getFormattedName();
      else
         name = "Unknow";
      Call* call2 = TreeWidgetCallModel::addDialingCall(name, TreeWidgetCallModel::getCurrentAccountId());
      call2->appendText(QString(encodedPhoneNumber));
      if (!parent) {
         //Dropped on free space
         qDebug() << "Adding new dialing call";
      }
      else if (parent->childCount() || parent->parent()) {
         //Dropped on a conversation
         QTreeWidgetItem* call = (parent->parent())?parent->parent():parent;
         addParticipant(getCall(call),call2);
      }
      else {
         //Dropped on call
         call2->actionPerformed(CALL_ACTION_ACCEPT);
         int state = getCall(parent)->getState();
         if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD) {
            getCall(parent)->actionPerformed(CALL_ACTION_ACCEPT);
         }
         createConferenceFromCall(call2,getCall(parent));
      }
   }
   return false;
}

bool CallView::contactToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   qDebug() << "contactToCall";
   Q_UNUSED(index)
   Q_UNUSED(action)
   QByteArray encodedContact = data->data( MIME_CONTACT );
   if (!QString(encodedContact).isEmpty()) {
      Contact* contact = AkonadiBackend::getInstance()->getContactByUid(encodedContact);
      if (contact) {
         Call* call2;
         if (contact->getPhoneNumbers().count() == 1) {
            call2 = TreeWidgetCallModel::addDialingCall(contact->getFormattedName(), TreeWidgetCallModel::getCurrentAccountId());
            call2->appendText(contact->getPhoneNumbers()[0]->getNumber());
         }
         else if (contact->getPhoneNumbers().count() > 1) {
            bool ok = false;
            QHash<QString,QString> map;
            QStringList list;
            foreach (Contact::PhoneNumber* number, contact->getPhoneNumbers()) {
               map[number->getType()+" ("+number->getNumber()+")"] = number->getNumber();
               list << number->getType()+" ("+number->getNumber()+")";
            }
            QString result = QInputDialog::getItem (this, QString("Select phone number"), QString("This contact have many phone number, please select the one you wish to call"), list, 0, false, &ok);
            if (ok) {
               call2 = TreeWidgetCallModel::addDialingCall(contact->getFormattedName(), TreeWidgetCallModel::getCurrentAccountId());
               call2->appendText(map[result]);
            }
            else {
               qDebug() << "Operation cancelled";
               return false;
            }
         }
         else {
            qDebug() << "This contact have no valid phone number";
            return false;
         }
         if (!parent) {
            //Dropped on free space
            qDebug() << "Adding new dialing call";
         }
         else if (parent->childCount() || parent->parent()) {
            //Dropped on a conversation
            QTreeWidgetItem* call = (parent->parent())?parent->parent():parent;
            addParticipant(getCall(call),call2);
         }
         else {
            //Dropped on call
            call2->actionPerformed(CALL_ACTION_ACCEPT);
            int state = getCall(parent)->getState();
            if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD) {
               getCall(parent)->actionPerformed(CALL_ACTION_ACCEPT);
            }
            createConferenceFromCall(call2,getCall(parent));
         }
      }
   }
   return false;
}

///Action performed when an item is dropped on the TreeView
bool CallView::dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action) 
{
   Q_UNUSED(index)
   Q_UNUSED(action)
   
   QByteArray encodedCallId      = data->data( MIME_CALLID      );
   QByteArray encodedPhoneNumber = data->data( MIME_PHONENUMBER );
   QByteArray encodedContact     = data->data( MIME_CONTACT     );

   if (!QString(encodedCallId).isEmpty()) {
      qDebug() << "CallId dropped"<< QString(encodedCallId);
      callToCall(parent, index, data, action);
   }
   else if (!QString(encodedPhoneNumber).isEmpty()) {
      qDebug() << "PhoneNumber dropped"<< QString(encodedPhoneNumber);
      phoneNumberToCall(parent, index, data, action);
   }
   else if (!QString(encodedContact).isEmpty()) {
      qDebug() << "Contact dropped"<< QString(encodedContact);
      contactToCall(parent, index, data, action);
   }
   return false;
}

///Encode data to be tranported during the drag n' drop operation
QMimeData* CallView::mimeData( const QList<QTreeWidgetItem *> items) const
{   
   qDebug() << "A call is being dragged";
   if (items.size() < 1) {
      return NULL;
   }
   
   QMimeData *mimeData = new QMimeData();
   
   //Call ID for internal call merging and spliting
   if (getCall(items[0])->isConference()) {
      mimeData->setData(MIME_CALLID, getCall(items[0])->getConfId().toAscii());
   }
   else {
      mimeData->setData(MIME_CALLID, getCall(items[0])->getCallId().toAscii());
   }
   
   //Plain text for other applications
   mimeData->setData(MIME_PLAIN_TEXT, QString(getCall(items[0])->getPeerName()+"\n"+getCall(items[0])->getPeerPhoneNumber()).toAscii());
   
   //TODO Comment this line if you don't want to see ugly artefact, but the caller details will not be visible while dragged
   items[0]->setText(0, getCall(items[0])->getPeerName() + "\n" + getCall(items[0])->getPeerPhoneNumber());
   return mimeData;
}


/*****************************************************************************
 *                                                                           *
 *                            Call related code                              *
 *                                                                           *
 ****************************************************************************/

///Add a call in the model structure, the call must exist before being added to the model
Call* CallView::addCall(Call* call, Call* parent) 
{
   //InternalCallModelStruct<CallTreeItem>* aNewStruct = privateCallList_call[CallModel<CallTreeItem>::addCall(call, parent)];
   
   QTreeWidgetItem* callItem = new QTreeWidgetItem();
   //aNewStruct->treeItem = callItem;
   updateIndex(call,callItem);
   
   //privateCallList_item[callItem] = aNewStruct;
   
   //aNewStruct->call = ;
   //privateCallList_widget[aNewStruct->call] = aNewStruct;
   insertItem(callItem,parent);
   
   setCurrentItem(callItem);
   
   connect(call, SIGNAL(isOver(Call*)), this, SLOT(destroyCall(Call*)));
   return call;
}

/*****************************************************************************
 *                                                                           *
 *                            View related code                              *
 *                                                                           *
 ****************************************************************************/

///Set the TreeView header text
void CallView::setTitle(QString title) 
{
   headerItem()->setText(0,title);
}

///Select an item in the TreeView
bool CallView::selectItem(Call* item) 
{
   if (getIndex(item)) {
      setCurrentItem(getIndex(item));
      return true;
   }
   else
      return false;
}

///Return the current item
Call* CallView::getCurrentItem() 
{
   if (currentItem() && getCall(QTreeWidget::currentItem()))
      return getCall(QTreeWidget::currentItem());
   else
      return false;
}

///Remove a TreeView item and delete it
bool CallView::removeItem(Call* item) 
{
   if (indexOfTopLevelItem(getIndex(item)) != -1) {//TODO To remove once safe
     removeItemWidget(getIndex(item),0);
     return true;
   }
   else
      return false;
}

///Return the TreeView, this
QWidget* CallView::getWidget() 
{
   return this;
}

///Convenience wrapper around extractItem(QTreeWidgetItem*)
QTreeWidgetItem* CallView::extractItem(QString callId) 
{
   QTreeWidgetItem* currentItem = getIndex(callId);
   return extractItem(currentItem);
}

///Extract an item from the TreeView and return it, the item is -not- deleted
QTreeWidgetItem* CallView::extractItem(QTreeWidgetItem* item) 
{
   QTreeWidgetItem* parentItem = item->parent();
   
   if (parentItem) {
      if ((indexOfTopLevelItem(parentItem) == -1 ) || (parentItem->indexOfChild(item) == -1)) {
         qDebug() << "The conversation does not exist";
         return 0;
      }
      
      QTreeWidgetItem* toReturn = parentItem->takeChild(parentItem->indexOfChild(item));

      return toReturn;
   }
   else
      return takeTopLevelItem(indexOfTopLevelItem(item));
}

///Convenience wrapper around insertItem(QTreeWidgetItem*, QTreeWidgetItem*)
CallTreeItem* CallView::insertItem(QTreeWidgetItem* item, Call* parent) 
{
   return insertItem(item,(parent)?getIndex(parent):0);
}

///Insert a TreeView item in the TreeView as child of parent or as a top level item, also restore the item Widget
CallTreeItem* CallView::insertItem(QTreeWidgetItem* item, QTreeWidgetItem* parent) 
{
   if (!item) {
      qDebug() << "This is not a valid call";
      return 0;
   }
   
   if (!parent)
      insertTopLevelItem(0,item);
   else
      parent->addChild(item);
   
   //privateCallList_widget.remove(privateCallList_item[item]->call); //TODO needed?
   CallTreeItem* callItem = new CallTreeItem();
   updateWidget(getCall(item), callItem);
   callItem->setCall(getCall(item));
   //privateCallList_widget[privateCallList_item[item]->call] = privateCallList_item[item];
   
   setItemWidget(item,0,callItem);
   
   expandAll();
   return callItem;
}

///Remove a call from the interface
void CallView::destroyCall(Call* toDestroy) 
{
   if (getIndex(toDestroy) == currentItem())
      setCurrentItem(0);
   
   if (!getIndex(toDestroy))
       qDebug() << "Call not found";
   else if (indexOfTopLevelItem(getIndex(toDestroy)) != -1)
      takeTopLevelItem(indexOfTopLevelItem(getIndex(toDestroy)));
   else if (getIndex(toDestroy)->parent()) //May crash here
      getIndex(toDestroy)->parent()->removeChild(getIndex(toDestroy));
   else
      qDebug() << "Call not found";
}

/// @todo Remove the text partially covering the TreeView item widget when it is being dragged, a beter implementation is needed
void CallView::clearArtefact(QTreeWidgetItem* item) 
{
   item->setText(0,"");
}


/*****************************************************************************
 *                                                                           *
 *                           Event related code                              *
 *                                                                           *
 ****************************************************************************/

void CallView::itemDoubleClicked(QTreeWidgetItem* item, int column) {
   Q_UNUSED(column)
   qDebug() << "Item doubleclicked";
   switch(getCall(item)->getState()) {
      case CALL_STATE_HOLD:
         getCall(item)->actionPerformed(CALL_ACTION_HOLD);
         break;
      case CALL_STATE_DIALING:
         getCall(item)->actionPerformed(CALL_ACTION_ACCEPT);
         break;
      default:
         qDebug() << "Double clicked an item with no action on double click.";
    }
}

void CallView::itemClicked(QTreeWidgetItem* item, int column) {
   Q_UNUSED(column)
   emit itemChanged(getCall(item));
   qDebug() << "Item clicked";
}


/*****************************************************************************
 *                                                                           *
 *                         Conference related code                           *
 *                                                                           *
 ****************************************************************************/

///Add a new conference, get the call list and update the interface as needed
Call* CallView::addConference(const QString & confID) 
{
   qDebug() << "Conference created";
   Call* newConf =  TreeWidgetCallModel::addConference(confID);

   //InternalCallModelStruct<CallTreeItem>* aNewStruct = privateCallList_callId[confID];
   
//    if (!aNewStruct) {
//       qDebug() << "Conference failed";
//       return 0;
//    }
   
   QTreeWidgetItem* confItem = new QTreeWidgetItem();
   //aNewStruct->treeItem = confItem;
   updateIndex(newConf,confItem);
   
   //privateCallList_item[confItem] = aNewStruct;
   
   //aNewStruct->call = ;
   insertItem(confItem,(QTreeWidgetItem*)0);
   //privateCallList_widget[aNewStruct->call] = aNewStruct;
   
   setCurrentItem(confItem);

   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   QStringList callList = callManager.getParticipantList(confID);
   
   foreach (QString callId, callList) {
      qDebug() << "Adding " << callId << "to the conversation";
      insertItem(extractItem(getIndex(callId)),confItem);
   }
   
   Q_ASSERT_X(confItem->childCount() == 0, "add conference","Conference created, but without any participants");
   return newConf;
}

///Executed when the daemon signal a modification in an existing conference. Update the call list and update the TreeView
bool CallView::conferenceChanged(const QString &confId, const QString &state) 
{
   qDebug() << "Conference changed";
   if (!TreeWidgetCallModel::conferenceChanged(confId, state))
     return false;

   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   QStringList callList = callManager.getParticipantList(confId);

   QList<QTreeWidgetItem*> buffer;
   foreach (QString callId, callList) {
      if (getCall(callId)) {
         QTreeWidgetItem* item3 = extractItem(getIndex(callId));
         insertItem(item3, getIndex(confId));
         buffer << getIndex(callId);
      }
      else
         qDebug() << "Call " << callId << " does not exist";
   }

   for (int j =0; j < getIndex(confId)->childCount();j++) {
      if (buffer.indexOf(getIndex(confId)->child(j)) == -1)
         insertItem(extractItem(getIndex(confId)->child(j)));
   }
   
   Q_ASSERT_X(getIndex(confId)->childCount() == 0,"changind conference","A conference can't have no participants");
   
   return true;
}

///Remove a conference from the model and the TreeView
void CallView::conferenceRemoved(const QString &confId) 
{
   while (getIndex(confId)->childCount()) {
      insertItem(extractItem(getIndex(confId)->child(0)));
   }
   takeTopLevelItem(indexOfTopLevelItem(getIndex(confId)));
   TreeWidgetCallModel::conferenceRemoved(confId);
   qDebug() << "Conference removed";
}

///Clear the list of old calls //TODO Clear them from the daemon
void CallView::clearHistory()
{
   historyCalls.clear();
}

// void CallView::callStateChangedSignal(const QString& callId, const QString& state)
// {
//   qDebug() << "Signal : Call State Changed for call  " << callId << " . New state : " << state;
//    Call* call = findCallByCallId(callId);
//    if(!call) {
//       if(state == CALL_STATE_CHANGE_RINGING) {
//          call = addRingingCall(callId);
//       }
//       else {
//          qDebug() << "Call doesn't exist in this client. Might have been initialized by another client instance before this one started.";
//          return;
//       }
//    }
//    else {
//       call->stateChanged(state);
//    }
// }
// 
// void CallView::incomingCallSignal(const QString& accountId, const QString& callId)
// {
//    Q_UNUSED(accountId)
//    qDebug() << "Signal : Incoming Call ! ID = " << callId;
//    addIncomingCall(callId);
// }
// 
void CallView::conferenceCreatedSignal(const QString& confId)
{
   addConference(confId);
}

void CallView::conferenceChangedSignal(const QString& confId, const QString& state)
{
   conferenceChanged(confId, state);
}

void CallView::conferenceRemovedSignal(const QString& confId)
{
   conferenceRemoved(confId);
}
// 
// void CallView::incomingMessageSignal(const QString& accountId, const QString& message)
// {
//    Q_UNUSED(accountId)
//    Q_UNUSED(message)
//    //TODO
// }
// 
// void CallView::voiceMailNotifySignal(const QString& accountId, int count)
// {
//    Q_UNUSED(accountId)
//    Q_UNUSED(count)
//    //TODO
// }