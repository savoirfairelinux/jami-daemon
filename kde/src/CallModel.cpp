/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Valle <emmanuel.lepage@savoirfairelinux.com >*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/ 
#include <CallModel.h>
#include <QDebug>

#include "callmanager_interface_singleton.h"
#include "configurationmanager_interface_singleton.h"
#include "sflphone_const.h"

/*****************************************************************************
 *                                                                           *
 *                               Constructor                                 *
 *                                                                           *
 ****************************************************************************/

///Retrieve current and older calls from the daemon, fill history and the calls TreeView and enable drag n' drop
CallModel::CallModel(ModelType type, QWidget* parent) : QTreeWidget(parent) 
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   QStringList callList = callManager.getCallList();
   foreach (QString callId, callList) {
      Call* tmpCall = Call::buildExistingCall(callId);
      activeCalls[tmpCall->getCallId()] = tmpCall;
      if (type == ActiveCall)
         addCall(tmpCall);
   }
   
   QStringList confList = callManager.getConferenceList();
   foreach (QString confId, confList) {
      addConference(confId);
   }
   
   //Add older calls
   MapStringString historyMap = configurationManager.getHistory().value();
   qDebug() << "Call History = " << historyMap;
   QMapIterator<QString, QString> i(historyMap);
   while (i.hasNext()) {
      i.next();
      uint startTimeStamp = i.key().toUInt();
      QStringList param = i.value().split("|");
      QString type = param[0];
      QString number = param[1];
      QString name = param[2];
      uint stopTimeStamp = param[3].toUInt();
      QString account = param[4];
      historyCalls[QString::number(startTimeStamp)] = Call::buildHistoryCall(generateCallId(), startTimeStamp, stopTimeStamp, account, name, number, type);
      //if (type == ActiveCall) //TODO uncomment
         //addCall(historyCalls[QString::number(startTimeStamp)]);
   }
   
   //Widget part
   setAcceptDrops(true);
   setDragEnabled(true);
   CallTreeItemDelegate *delegate = new CallTreeItemDelegate();
   setItemDelegate(delegate); 
}



/*****************************************************************************
 *                                                                           *
 *                        Drag and drop related code                         *
 *                                                                           *
 ****************************************************************************/

///Action performed when an item is dropped on the TreeView
bool CallModel::dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action) 
{
   QByteArray encodedData = data->data(MIME_CALLID);
   
   if (!QString(encodedData).isEmpty()) {
   clearArtefact(privateCallList_callId[encodedData]->treeItem);
   
   if (!parent) {
         qDebug() << "Call dropped on empty space";
         if (privateCallList_callId[encodedData]->treeItem->parent())
            detachParticipant(privateCallList_callId[encodedData]->call_real);
         else
            qDebug() << "The call is not in a conversation (doing nothing)";
         return true;
      }
      
      if (privateCallList_item[parent]->call_real->getCallId() == QString(encodedData)) {
         qDebug() << "Call dropped on itself (doing nothing)";
         return true;
      }
      
      if ((parent->parent()) || (parent->childCount())) {
         qDebug() << "Call dropped on a conference";
         
         if ((privateCallList_callId[encodedData]->treeItem->childCount()) && (!parent->childCount())) {
            qDebug() << "Conference dropped on a call (doing nothing)";
            return true;
         }
         
         QTreeWidgetItem* call1 = privateCallList_callId[encodedData]->treeItem;
         QTreeWidgetItem* call2 = (parent->parent())?parent->parent():parent;
         
         if (call1->parent()) {
            if (call1->parent() == call2) {
               qDebug() << "Call dropped on it's own conversation (doing nothing)";
               return true;
            }
            else if (privateCallList_item[call1]->treeItem->childCount()) {
               qDebug() << "Merging two conferences";
               mergeConferences(privateCallList_item[call1]->call_real,privateCallList_item[call2]->call_real);
            }
            else if (call1->parent()) {
               qDebug() << "Moving call from a conference to an other";
               detachParticipant(privateCallList_callId[encodedData]->call_real);
            }
         }
         
         addParticipant(privateCallList_item[call1]->call_real,privateCallList_item[call2]->call_real);
         return true;
      }
      
      qDebug() << "Call dropped on another call";
      createConferenceFromCall(privateCallList_callId[encodedData]->call_real,privateCallList_item[parent]->call_real);
      return true;
   }
   
   return false;
}

///Encode data to be tranported during the drag n' drop operation
QMimeData* CallModel::mimeData(const QList<QTreeWidgetItem *> items) const 
{   
   QMimeData *mimeData = new QMimeData();
   
   //Call ID for internal call merging and spliting
   
   if (privateCallList_item[items[0]]->call_real->isConference())
      mimeData->setData(MIME_CALLID, privateCallList_item[items[0]]->call_real->getConfId().toAscii());
   else
      mimeData->setData(MIME_CALLID, privateCallList_item[items[0]]->call_real->getCallId().toAscii());
   
   //Plain text for other applications
   mimeData->setData(MIME_PLAIN_TEXT, QString(privateCallList_item[items[0]]->call_real->getPeerName()+"\n"+privateCallList_item[items[0]]->call_real->getPeerPhoneNumber()).toAscii());
   
   //TODO Comment this line if you don't want to see ugly artefact, but the caller details will not be visible while dragged
   items[0]->setText(0, privateCallList_item[items[0]]->call_real->getPeerName() + "\n" + privateCallList_item[items[0]]->call_real->getPeerPhoneNumber());
   return mimeData;
}



/*****************************************************************************
 *                                                                           *
 *                         Access related functions                          *
 *                                                                           *
 ****************************************************************************/

///Return the active call count
int CallModel::size() 
{
   return activeCalls.size();
}

///Return a call corresponding to this ID or NULL
Call* CallModel::findCallByCallId(QString callId) 
{
   return activeCalls[callId];
}

///Return the action call list
QList<Call*> CallModel::getCallList() 
{
   QList<Call*> callList;
   foreach(Call* call, activeCalls) {
      callList.push_back(call);
   }
   return callList;
}



/*****************************************************************************
 *                                                                           *
 *                            View related code                              *
 *                                                                           *
 ****************************************************************************/

///Set the TreeView header text
void CallModel::setTitle(QString title) 
{
   headerItem()->setText(0,title);
}

///Select an item in the TreeView
bool CallModel::selectItem(Call* item) 
{
   if (privateCallList_call[item]->treeItem) {
      setCurrentItem(privateCallList_call[item]->treeItem);
      return true;
   }
   else
      return false;
}

///Return the current item
Call* CallModel::getCurrentItem() 
{
   if (currentItem() && privateCallList_item[QTreeWidget::currentItem()])
      return privateCallList_item[QTreeWidget::currentItem()]->call_real;
   else
      return false;
}

///Remove a TreeView item and delete it
bool CallModel::removeItem(Call* item) 
{
   if (indexOfTopLevelItem(privateCallList_call[item]->treeItem) != -1) {//TODO To remove once safe
     removeItemWidget(privateCallList_call[item]->treeItem,0);
     return true;
   }
   else
      return false;
}

///Return the TreeView, this
QWidget* CallModel::getWidget() 
{
   return this;
}

///Convenience wrapper around extractItem(QTreeWidgetItem*)
QTreeWidgetItem* CallModel::extractItem(QString callId) 
{
   QTreeWidgetItem* currentItem = privateCallList_callId[callId]->treeItem;
   return extractItem(currentItem);
}

///Extract an item from the TreeView and return it, the item is -not- deleted
QTreeWidgetItem* CallModel::extractItem(QTreeWidgetItem* item) 
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
CallTreeItem* CallModel::insertItem(QTreeWidgetItem* item, Call* parent) 
{
   return insertItem(item,(parent)?privateCallList_call[parent]->treeItem:0);
}

///Insert a TreeView item in the TreeView as child of parent or as a top level item, also restore the item Widget
CallTreeItem* CallModel::insertItem(QTreeWidgetItem* item, QTreeWidgetItem* parent) 
{
   if (!item) {
      qDebug() << "This is not a valid call";
      return 0;
   }
   
   if (!parent)
      insertTopLevelItem(0,item);
   else
      parent->addChild(item);
   
   privateCallList_widget.remove(privateCallList_item[item]->call);
   privateCallList_item[item]->call = new CallTreeItem();
   privateCallList_item[item]->call->setCall(privateCallList_item[item]->call_real);
   privateCallList_widget[privateCallList_item[item]->call] = privateCallList_item[item];
   
   setItemWidget(item,0,privateCallList_item[item]->call);
   
   expandAll();
   return privateCallList_item[item]->call;
}

///Remove a call from the interface
void CallModel::destroyCall(Call* toDestroy) 
{
   if (privateCallList_call[toDestroy]->treeItem == currentItem())
      setCurrentItem(0);
   
   if (indexOfTopLevelItem(privateCallList_call[toDestroy]->treeItem) != -1)
      takeTopLevelItem(indexOfTopLevelItem(privateCallList_call[toDestroy]->treeItem));
   else if (privateCallList_call[toDestroy]->treeItem->parent())
      privateCallList_call[toDestroy]->treeItem->parent()->removeChild(privateCallList_call[toDestroy]->treeItem);
   else
      qDebug() << "Call not found";
}

/// @todo Remove the text partially covering the TreeView item widget when it is being dragged, a beter implementation is needed
void CallModel::clearArtefact(QTreeWidgetItem* item) 
{
   item->setText(0,"");
}



/*****************************************************************************
 *                                                                           *
 *                            Call related code                              *
 *                                                                           *
 ****************************************************************************/

///Add a call in the model structure, the call must exist before being added to the model
Call* CallModel::addCall(Call* call, Call* parent) 
{
   InternalCallModelStruct* aNewStruct = new InternalCallModelStruct;
   aNewStruct->call_real = call;
   
   QTreeWidgetItem* callItem = new QTreeWidgetItem();
   aNewStruct->treeItem = callItem;
   aNewStruct->conference = false;
   
   privateCallList_item[callItem] = aNewStruct;
   privateCallList_call[call] = aNewStruct;
   privateCallList_callId[call->getCallId()] = aNewStruct;
   
   aNewStruct->call = insertItem(callItem,parent);
   privateCallList_widget[aNewStruct->call] = aNewStruct;
   
   setCurrentItem(callItem);
   
   connect(call, SIGNAL(isOver(Call*)), this, SLOT(destroyCall(Call*)));
   return call;
}

///Create a new dialing call from peer name and the account ID
Call* CallModel::addDialingCall(const QString & peerName, QString account)
{
   Call* call = Call::buildDialingCall(generateCallId(), peerName, account);
   activeCalls[call->getCallId()] = call;
   addCall(call);
   selectItem(call);
   return call;
}

///Create a new incomming call when the daemon is being called
Call* CallModel::addIncomingCall(const QString & callId)
{
   Call* call = Call::buildIncomingCall(callId);
   activeCalls[call->getCallId()] = call;
   addCall(call);
   selectItem(call);
   return call;
}

///Create a ringing call
Call* CallModel::addRingingCall(const QString & callId)
{
   Call* call = Call::buildRingingCall(callId);
   activeCalls[call->getCallId()] = call;
   addCall(call);
   selectItem(call);
   return call;
}

///Generate a new random call unique identifier (callId)
QString CallModel::generateCallId()
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
Call* CallModel::addConference(const QString & confID) 
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
   aNewStruct->conference = true;
   
   QTreeWidgetItem* confItem = new QTreeWidgetItem();
   aNewStruct->treeItem = confItem;
   
   privateCallList_item[confItem] = aNewStruct;
   privateCallList_call[newConf] = aNewStruct;
   privateCallList_callId[newConf->getConfId()] = aNewStruct; //WARNING It may break something is it is done wrong
   
   aNewStruct->call = insertItem(confItem,(QTreeWidgetItem*)0);
   privateCallList_widget[aNewStruct->call] = aNewStruct;
   
   setCurrentItem(confItem);

   foreach (QString callId, callList) {
     insertItem(extractItem(privateCallList_callId[callId]->treeItem),confItem);
   }
   return newConf;
}

///Join two call to create a conference, the conference will be created later (see addConference)
bool CallModel::createConferenceFromCall(Call* call1, Call* call2) 
{
  qDebug() << "Joining call: " << call1->getCallId() << " and " << call2->getCallId();
  CallManagerInterface &callManager = CallManagerInterfaceSingleton::getInstance();
  callManager.joinParticipant(call1->getCallId(),call2->getCallId());
  return true;
}

///Add a new participant to a conference
bool CallModel::addParticipant(Call* call2, Call* conference) 
{
   if (conference->isConference()) {
      CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
      callManager.addParticipant(call2->getCallId(), conference->getConfId());
      return true;
   }
   else {
      qDebug() << "This is not a conference";
      return false;
   }
}

///Remove a participant from a conference
bool CallModel::detachParticipant(Call* call) 
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.detachParticipant(call->getCallId());
   return true;
}

///Merge two conferences
bool CallModel::mergeConferences(Call* conf1, Call* conf2) 
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.joinConference(conf1->getConfId(),conf2->getConfId());
   return true;
}

///Executed when the daemon signal a modification in an existing conference. Update the call list and update the TreeView
void CallModel::conferenceChanged(const QString &confId, const QString &state) 
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   QStringList callList = callManager.getParticipantList(confId);
   qDebug() << "New " + confId + " participant list: " << callList;
   
   if (!privateCallList_callId[confId]) {
      qDebug() << "The conference does not exist";
      return;
   }
   
   if (!privateCallList_callId[confId]->treeItem) {
      qDebug() << "The conference item does not exist";
      return;
   }

   QList<QTreeWidgetItem*> buffer;
   foreach (QString callId, callList) {
      if (privateCallList_callId[callId]) {
         QTreeWidgetItem* item3 = extractItem(privateCallList_callId[callId]->treeItem);
         insertItem(item3, privateCallList_callId[confId]->treeItem);
         buffer << privateCallList_callId[callId]->treeItem;
      }
      else
         qDebug() << "Call " << callId << " does not exist";
   }

   for (int j =0; j < privateCallList_callId[confId]->treeItem->childCount();j++) {
      if (buffer.indexOf(privateCallList_callId[confId]->treeItem->child(j)) == -1)
         insertItem(extractItem(privateCallList_callId[confId]->treeItem->child(j)));
   }
}

///Remove a conference from the model and the TreeView
void CallModel::conferenceRemoved(const QString &confId) 
{
   qDebug() << "Ending conversation containing " << privateCallList_callId[confId]->treeItem->childCount() << " participants";
   for (int j =0; j < privateCallList_callId[confId]->treeItem->childCount();j++) {
      insertItem(extractItem(privateCallList_callId[confId]->treeItem->child(j)));
   }
   privateCallList_call.remove(privateCallList_callId[confId]->call_real);
   privateCallList_widget.remove(privateCallList_callId[confId]->call);
   privateCallList_item.remove(privateCallList_callId[confId]->treeItem);
   takeTopLevelItem(indexOfTopLevelItem(privateCallList_callId[confId]->treeItem));
   delete privateCallList_callId[confId]->treeItem;
   privateCallList_callId.remove(confId);
}



/*****************************************************************************
 *                                                                           *
 *                           History related code                            *
 *                                                                           *
 ****************************************************************************/

///Return a list of all previous calls
MapStringString CallModel::getHistoryMap() 
{
   MapStringString toReturn;
   foreach(Call* call, historyCalls) {
      toReturn[historyCalls.key(call)] = Call::getTypeFromHistoryState(call->getHistoryState()) + "|" + call->getPeerPhoneNumber() + "|" + call->getPeerName() + "|" + call->getStopTimeStamp() + "|" + call->getAccountId();
   }
   return toReturn;
}

///Clear the list of old calls //TODO Clear them from the daemon
void CallModel::clearHistory()
{
   historyCalls.clear();
}
