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
#include <unistd.h>

#include "callmanager_interface_singleton.h"
#include "configurationmanager_interface_singleton.h"

CallModel::CallModel(ModelType type, QWidget* parent) : QTreeWidget(parent) {
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

void CallModel::setTitle(QString title) {
   headerItem()->setText(0,title);
}

Call* CallModel::addCall(Call* call, Call* parent) {
   InternalCallModelStruct* aNewStruct = new InternalCallModelStruct;
   aNewStruct->call_real = call;
   
   QTreeWidgetItem* callItem = new QTreeWidgetItem();
   aNewStruct->currentItem = callItem;
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

MapStringString CallModel::getHistoryMap() {
   MapStringString toReturn;
   foreach(Call* call, historyCalls) {
      toReturn[historyCalls.key(call)] = Call::getTypeFromHistoryState(call->getHistoryState()) + "|" + call->getPeerPhoneNumber() + "|" + call->getPeerName() + "|" + call->getStopTimeStamp() + "|" + call->getAccountId();
   }
   return toReturn;
}

Call* CallModel::addDialingCall(const QString & peerName, QString account)
{
   Call* call = Call::buildDialingCall(generateCallId(), peerName, account);
   activeCalls[call->getCallId()] = call;
   addCall(call);
   selectItem(call);
   return call;
}

Call* CallModel::addIncomingCall(const QString & callId)
{
   Call* call = Call::buildIncomingCall(callId);
   activeCalls[call->getCallId()] = call;
   addCall(call);
   selectItem(call);
   return call;
}

Call* CallModel::addRingingCall(const QString & callId)
{
   Call* call = Call::buildRingingCall(callId);
   activeCalls[call->getCallId()] = call;
   addCall(call);
   selectItem(call);
   return call;
}

Call* CallModel::addConference(const QString & confID) {
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
   aNewStruct->currentItem = confItem;
   
   privateCallList_item[confItem] = aNewStruct;
   privateCallList_call[newConf] = aNewStruct;
   privateCallList_callId[newConf->getConfId()] = aNewStruct; //WARNING It may break something is it is done wrong
   
   aNewStruct->call = insertItem(confItem,(QTreeWidgetItem*)0);
   privateCallList_widget[aNewStruct->call] = aNewStruct;
   
   setCurrentItem(confItem);

   foreach (QString callId, callList) {
     insertItem(extractItem(privateCallList_callId[callId]->currentItem),confItem);
   }
   return newConf;
}

bool CallModel::createConferenceFromCall(Call* call1, Call* call2) {
  qDebug() << "Joining call: " << call1->getCallId() << " and " << call2->getCallId();
  CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
  callManager.joinParticipant(call1->getCallId(),call2->getCallId());
  return true;
}

bool CallModel::addParticipant(Call* call2, Call* conference) {
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

bool CallModel::detachParticipant(Call* call) {
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.detachParticipant(call->getCallId());
   //insertItem(extractItem(privateCallList_call[call]->currentItem));
   return true;
}

bool CallModel::mergeConferences(Call* conf1, Call* conf2) {
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.joinConference(conf1->getConfId(),conf2->getConfId());
   return true;
}

void CallModel::conferenceChanged(const QString &confId, const QString &state) {
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   QStringList callList = callManager.getParticipantList(confId);
   qDebug() << "New " + confId + " participant list: " << callList;
   foreach (QString callId, callList) {
      qDebug() << "Item: " << privateCallList_callId[callId]->currentItem;
      if (privateCallList_callId[callId])
         insertItem(extractItem(privateCallList_callId[callId]->currentItem), privateCallList_callId[confId]->currentItem);
      else
         qDebug() << "Call " << callId << " does not exist";
   }
}

int CallModel::size() {
   return activeCalls.size();
}

Call* CallModel::findCallByCallId(QString callId) {
   return activeCalls[callId];
}

QList<Call*> CallModel::getCallList() {
   QList<Call*> callList;
   foreach(Call* call, activeCalls) {
      callList.push_back(call);
   }
   return callList;
}

bool CallModel::selectItem(Call* item) {
   if (privateCallList_call[item]->currentItem) {
      setCurrentItem(privateCallList_call[item]->currentItem);
      return true;
   }
   else
      return false;
}

Call* CallModel::getCurrentItem() {
   if (currentItem() && privateCallList_item[QTreeWidget::currentItem()])
      return privateCallList_item[QTreeWidget::currentItem()]->call_real;
   else
      return false;
}

bool CallModel::removeItem(Call* item) {
   if (indexOfTopLevelItem(privateCallList_call[item]->currentItem) != -1) {//TODO To remove once safe
     removeItemWidget(privateCallList_call[item]->currentItem,0);\
     return true;
   }
   else
      return false;
}

QWidget* CallModel::getWidget() {
   return this;
}

void CallModel::destroyCall(Call* toDestroy) {
   if (privateCallList_call[toDestroy]->currentItem == currentItem())
      setCurrentItem(0);
   
   if (indexOfTopLevelItem(privateCallList_call[toDestroy]->currentItem) != -1)
      takeTopLevelItem(indexOfTopLevelItem(privateCallList_call[toDestroy]->currentItem));
   else
      privateCallList_call[toDestroy]->currentItem->parent()->removeChild(privateCallList_call[toDestroy]->currentItem);
}

QTreeWidgetItem* CallModel::extractItem(QString callId) {
   QTreeWidgetItem* currentItem = privateCallList_callId[callId]->currentItem;
   return extractItem(currentItem);
}

QTreeWidgetItem* CallModel::extractItem(QTreeWidgetItem* item) {
   QTreeWidgetItem* parentItem = item->parent();
   
   if (parentItem) {
      QTreeWidgetItem* toReturn = parentItem->takeChild(parentItem->indexOfChild(item));
      if (parentItem->childCount() == 1) {
         insertItem(extractItem(parentItem->child(0)));
         takeTopLevelItem(indexOfTopLevelItem(parentItem));
         delete parentItem;
      }
      return toReturn;
   }
   else
      return takeTopLevelItem(indexOfTopLevelItem(item));
}

CallTreeItem* CallModel::insertItem(QTreeWidgetItem* item, Call* parent) {
   return insertItem(item,(parent)?privateCallList_call[parent]->currentItem:0);
}

CallTreeItem* CallModel::insertItem(QTreeWidgetItem* item, QTreeWidgetItem* parent) {
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
   
   qDebug() << "DEBUG item: " << item << " widget : " << privateCallList_item[item]->call;
   setItemWidget(item,0,privateCallList_item[item]->call);
   
   expandAll();
   return privateCallList_item[item]->call;
}

void CallModel::clearArtefact(QTreeWidgetItem* item) {
   item->setText(0,"");
}

bool CallModel::dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action) {
   QByteArray encodedData = data->data("text/sflphone.call.id");
   
   clearArtefact(privateCallList_callId[encodedData]->currentItem);
   
   if (!parent) {
      qDebug() << "Call dropped on empty space";
      if (privateCallList_callId[encodedData]->currentItem->parent())
         detachParticipant(privateCallList_callId[encodedData]->call_real);
      return true;
   }
   
   if (privateCallList_item[parent]->call_real->getCallId() == QString(encodedData)) {
      qDebug() << "Call dropped on itself (doing nothing)";
      return true; //Item dropped on itself
   }
   
   if ((parent->parent()) || (parent->childCount())) {
      qDebug() << "Call dropped on a conference";
      
      if ((privateCallList_callId[encodedData]->currentItem->childCount()) && (!parent->childCount())) {
         qDebug() << "Conference dropped on a call (doing nothing)";
         return true;
      }
      
      QTreeWidgetItem* call1 = privateCallList_callId[encodedData]->currentItem;
      QTreeWidgetItem* call2 = (parent->parent())?parent->parent():parent;
      
      if (privateCallList_callId[encodedData]->currentItem->parent()) {
         if (privateCallList_callId[encodedData]->currentItem->parent() == call2->parent()) {
            qDebug() << "Call dropped on it's own conversation (doing nothing)";
            return true;
         }
         else if (privateCallList_item[call1]->currentItem->childCount()) {
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


QMimeData* CallModel::mimeData(const QList<QTreeWidgetItem *> items) const {   
   QMimeData *mimeData = new QMimeData();
   
   //Call ID for internal call merging and spliting
   if (privateCallList_item[items[0]]->call_real->isConference())
      mimeData->setData("text/sflphone.call.id", privateCallList_item[items[0]]->call_real->getConfId().toAscii());
   else
      mimeData->setData("text/sflphone.call.id", privateCallList_item[items[0]]->call_real->getCallId().toAscii());
   
   //Plain text for other applications
   mimeData->setData("text/plain", QString(privateCallList_item[items[0]]->call_real->getPeerName()+"\n"+privateCallList_item[items[0]]->call_real->getPeerPhoneNumber()).toAscii());
   
   //BUG Comment this line if you don't want to see ugly artefact
   items[0]->setText(0, privateCallList_item[items[0]]->call_real->getPeerName() + "\n" + privateCallList_item[items[0]]->call_real->getPeerPhoneNumber());
   return mimeData;
}

QString CallModel::generateCallId()
{
   int id = qrand();
   QString res = QString::number(id);
   return res;
}

void CallModel::clearHistory()
{
   historyCalls.clear();
}