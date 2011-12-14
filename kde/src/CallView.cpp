/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
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
 **************************************************************************/

//Parent
#include "CallView.h"

//Qt
#include <QtGui/QInputDialog>
#include <QtGui/QTreeWidget>
#include <QtGui/QTreeWidgetItem>

//KDE
#include <KDebug>

//SFLPhone library
#include "lib/Contact.h"
#include "lib/sflphone_const.h"
#include "lib/callmanager_interface_singleton.h"

//SFLPhone
#include "widgets/CallTreeItem.h"
#include "SFLPhone.h"
#include "SFLPhoneView.h"
#include "AkonadiBackend.h"


///Retrieve current and older calls from the daemon, fill history and the calls TreeView and enable drag n' drop
CallView::CallView(QWidget* parent) : QTreeWidget(parent)
{
   //Widget part
   setAcceptDrops(true);
   setDragEnabled(true);
   CallTreeItemDelegate *delegate = new CallTreeItemDelegate();
   setItemDelegate(delegate); 
   setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding));

   //User Interface even
   //              SENDER                                   SIGNAL                              RECEIVER                     SLOT                        /
   /**/connect(this              , SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)              ) , this, SLOT( itemDoubleClicked(QTreeWidgetItem*,int)) );
   /**/connect(this              , SIGNAL(itemClicked(QTreeWidgetItem*,int)                    ) , this, SLOT( itemClicked(QTreeWidgetItem*,int))       );
   /**/connect(this              , SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)) , this, SLOT( itemClicked(QTreeWidgetItem*))           );
   /**/connect(SFLPhone::model() , SIGNAL(conferenceCreated(Call*)                             ) , this, SLOT( addConference(Call*))                    );
   /**/connect(SFLPhone::model() , SIGNAL(conferenceChanged(Call*)                             ) , this, SLOT( conferenceChanged(Call*))                );
   /**/connect(SFLPhone::model() , SIGNAL(aboutToRemoveConference(Call*)                       ) , this, SLOT( conferenceRemoved(Call*))                );
   /**/connect(SFLPhone::model() , SIGNAL(callAdded(Call*,Call*)                               ) , this, SLOT( addCall(Call*,Call*))                    );
   /*                                                                                                                                                   */

}


/*****************************************************************************
 *                                                                           *
 *                        Drag and drop related code                         *
 *                                                                           *
 ****************************************************************************/

///A call is dropped on another call
bool CallView::callToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   Q_UNUSED(index)
   Q_UNUSED(action)
   QByteArray encodedCallId      = data->data( MIME_CALLID      );
   if (!QString(encodedCallId).isEmpty()) {
      if (SFLPhone::model()->getIndex(encodedCallId))
        clearArtefact(SFLPhone::model()->getIndex(encodedCallId));

      if (!parent) {
         kDebug() << "Call dropped on empty space";
         if (SFLPhone::model()->getIndex(encodedCallId)->parent()) {
            kDebug() << "Detaching participant";
            SFLPhone::model()->detachParticipant(SFLPhone::model()->getCall(encodedCallId));
         }
         else
            kDebug() << "The call is not in a conversation (doing nothing)";
         return true;
      }

      if (SFLPhone::model()->getCall(parent)->getCallId() == QString(encodedCallId)) {
         kDebug() << "Call dropped on itself (doing nothing)";
         return true;
      }

      if ((parent->childCount()) && (SFLPhone::model()->getIndex(encodedCallId)->childCount())) {
         kDebug() << "Merging two conferences";
         SFLPhone::model()->mergeConferences(SFLPhone::model()->getCall(parent),SFLPhone::model()->getCall(encodedCallId));
         return true;
      }
      else if ((parent->parent()) || (parent->childCount())) {
         kDebug() << "Call dropped on a conference";

         if ((SFLPhone::model()->getIndex(encodedCallId)->childCount()) && (!parent->childCount())) {
            kDebug() << "Conference dropped on a call (doing nothing)";
            return true;
         }

         QTreeWidgetItem* call1 = SFLPhone::model()->getIndex(encodedCallId);
         QTreeWidgetItem* call2 = (parent->parent())?parent->parent():parent;

         if (call1->parent()) {
            kDebug() << "Call 1 is part of a conference";
            if (call1->parent() == call2) {
               kDebug() << "Call dropped on it's own conference (doing nothing)";
               return true;
            }
            else if (SFLPhone::model()->getIndex(call1)->childCount()) {
               kDebug() << "Merging two conferences";
               SFLPhone::model()->mergeConferences(SFLPhone::model()->getCall(call1),SFLPhone::model()->getCall(call2));
            }
            else if (call1->parent()) {
               kDebug() << "Moving call from a conference to an other";
               SFLPhone::model()->detachParticipant(SFLPhone::model()->getCall(encodedCallId));
            }
         }
         kDebug() << "Adding participant";
         int state = SFLPhone::model()->getCall(call1)->getState();
         if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD) {
            SFLPhone::model()->getCall(call1)->actionPerformed(CALL_ACTION_ACCEPT);
         }
         state = SFLPhone::model()->getCall(call2)->getState();
         if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD) {
            SFLPhone::model()->getCall(call2)->actionPerformed(CALL_ACTION_ACCEPT);
         }
         SFLPhone::model()->addParticipant(SFLPhone::model()->getCall(call1),SFLPhone::model()->getCall(call2));
         return true;
      }
      else if ((SFLPhone::model()->getIndex(encodedCallId)->childCount()) && (!parent->childCount())) {
         kDebug() << "Call dropped on it's own conference (doing nothing)";
         return true;
      }

      kDebug() << "Call dropped on another call";
      SFLPhone::model()->createConferenceFromCall(SFLPhone::model()->getCall(encodedCallId),SFLPhone::model()->getCall(parent));
      return true;
   }
   return false;
}

///A string containing a call number is dropped on a call
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
      Call* call2 = SFLPhone::model()->addDialingCall(name, SFLPhone::model()->getCurrentAccountId());
      call2->appendText(QString(encodedPhoneNumber));
      if (!parent) {
         //Dropped on free space
         kDebug() << "Adding new dialing call";
      }
      else if (parent->childCount() || parent->parent()) {
         //Dropped on a conversation
         QTreeWidgetItem* call = (parent->parent())?parent->parent():parent;
         SFLPhone::model()->addParticipant(SFLPhone::model()->getCall(call),call2);
      }
      else {
         //Dropped on call
         call2->actionPerformed(CALL_ACTION_ACCEPT);
         int state = SFLPhone::model()->getCall(parent)->getState();
         if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD) {
            SFLPhone::model()->getCall(parent)->actionPerformed(CALL_ACTION_ACCEPT);
         }
         SFLPhone::model()->createConferenceFromCall(call2,SFLPhone::model()->getCall(parent));
      }
   }
   return false;
}

///A contact ID is dropped on a call
bool CallView::contactToCall(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   kDebug() << "contactToCall";
   Q_UNUSED( index  )
   Q_UNUSED( action )
   QByteArray encodedContact = data->data( MIME_CONTACT );
   if (!QString(encodedContact).isEmpty()) {
      Contact* contact = AkonadiBackend::getInstance()->getContactByUid(encodedContact);
      if (contact) {
         Call* call2;
         if (contact->getPhoneNumbers().count() == 1) {
            call2 = SFLPhone::model()->addDialingCall(contact->getFormattedName(), SFLPhone::model()->getCurrentAccountId());
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
               call2 = SFLPhone::model()->addDialingCall(contact->getFormattedName(), SFLPhone::model()->getCurrentAccountId());
               call2->appendText(map[result]);
            }
            else {
               kDebug() << "Operation cancelled";
               return false;
            }
         }
         else {
            kDebug() << "This contact have no valid phone number";
            return false;
         }
         if (!parent) {
            //Dropped on free space
            kDebug() << "Adding new dialing call";
         }
         else if (parent->childCount() || parent->parent()) {
            //Dropped on a conversation
            QTreeWidgetItem* call = (parent->parent())?parent->parent():parent;
            SFLPhone::model()->addParticipant(SFLPhone::model()->getCall(call),call2);
         }
         else {
            //Dropped on call
            call2->actionPerformed(CALL_ACTION_ACCEPT);
            int state = SFLPhone::model()->getCall(parent)->getState();
            if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD) {
               SFLPhone::model()->getCall(parent)->actionPerformed(CALL_ACTION_ACCEPT);
            }
            SFLPhone::model()->createConferenceFromCall(call2,SFLPhone::model()->getCall(parent));
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
      kDebug() << "CallId dropped"<< QString(encodedCallId);
      callToCall(parent, index, data, action);
   }
   else if (!QString(encodedPhoneNumber).isEmpty()) {
      kDebug() << "PhoneNumber dropped"<< QString(encodedPhoneNumber);
      phoneNumberToCall(parent, index, data, action);
   }
   else if (!QString(encodedContact).isEmpty()) {
      kDebug() << "Contact dropped"<< QString(encodedContact);
      contactToCall(parent, index, data, action);
   }
   return false;
}

///Encode data to be tranported during the drag n' drop operation
QMimeData* CallView::mimeData( const QList<QTreeWidgetItem *> items) const
{   
   kDebug() << "A call is being dragged";
   if (items.size() < 1) {
      return NULL;
   }
   
   QMimeData *mimeData = new QMimeData();
   
   //Call ID for internal call merging and spliting
   if (SFLPhone::model()->getCall(items[0])->isConference()) {
      mimeData->setData(MIME_CALLID, SFLPhone::model()->getCall(items[0])->getConfId().toAscii());
   }
   else {
      mimeData->setData(MIME_CALLID, SFLPhone::model()->getCall(items[0])->getCallId().toAscii());
   }
   
   //Plain text for other applications
   mimeData->setData(MIME_PLAIN_TEXT, QString(SFLPhone::model()->getCall(items[0])->getPeerName()+"\n"+SFLPhone::model()->getCall(items[0])->getPeerPhoneNumber()).toAscii());
   
   //TODO Comment this line if you don't want to see ugly artefact, but the caller details will not be visible while dragged
   items[0]->setText(0, SFLPhone::model()->getCall(items[0])->getPeerName() + "\n" + SFLPhone::model()->getCall(items[0])->getPeerPhoneNumber());
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
   QTreeWidgetItem* callItem = new QTreeWidgetItem();
   SFLPhone::model()->updateIndex(call,callItem);
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
void CallView::setTitle(const QString& title) 
{
   headerItem()->setText(0,title);
}

///Select an item in the TreeView
bool CallView::selectItem(Call* item) 
{
   if (SFLPhone::model()->getIndex(item)) {
      setCurrentItem(SFLPhone::model()->getIndex(item));
      return true;
   }
   else
      return false;
}

///Return the current item
Call* CallView::getCurrentItem() 
{
   if (currentItem() && SFLPhone::model()->getCall(QTreeWidget::currentItem()))
      return SFLPhone::model()->getCall(QTreeWidget::currentItem());
   else
      return false;
}

///Remove a TreeView item and delete it
bool CallView::removeItem(Call* item) 
{
   if (indexOfTopLevelItem(SFLPhone::model()->getIndex(item)) != -1) {//TODO To remove once safe
     removeItemWidget(SFLPhone::model()->getIndex(item),0);
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
QTreeWidgetItem* CallView::extractItem(const QString& callId) 
{
   QTreeWidgetItem* currentItem = SFLPhone::model()->getIndex(callId);
   return extractItem(currentItem);
}

///Extract an item from the TreeView and return it, the item is -not- deleted
QTreeWidgetItem* CallView::extractItem(QTreeWidgetItem* item) 
{
   QTreeWidgetItem* parentItem = item->parent();
   
   if (parentItem) {
      if ((indexOfTopLevelItem(parentItem) == -1 ) || (parentItem->indexOfChild(item) == -1)) {
         kDebug() << "The conversation does not exist";
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
   return insertItem(item,(parent)?SFLPhone::model()->getIndex(parent):0);
}

///Insert a TreeView item in the TreeView as child of parent or as a top level item, also restore the item Widget
CallTreeItem* CallView::insertItem(QTreeWidgetItem* item, QTreeWidgetItem* parent) 
{
   if (!item) {
      kDebug() << "This is not a valid call";
      return 0;
   }
   
   if (!parent)
      insertTopLevelItem(0,item);
   else
      parent->addChild(item);
   
   CallTreeItem* callItem = new CallTreeItem();
   SFLPhone::model()->updateWidget(SFLPhone::model()->getCall(item), callItem);
   callItem->setCall(SFLPhone::model()->getCall(item));
   
   setItemWidget(item,0,callItem);
   
   expandAll();
   return callItem;
}

///Remove a call from the interface
void CallView::destroyCall(Call* toDestroy) 
{
   if (SFLPhone::model()->getIndex(toDestroy) == currentItem())
      setCurrentItem(0);
   
   if (!SFLPhone::model()->getIndex(toDestroy))
       kDebug() << "Call not found";
   else if (indexOfTopLevelItem(SFLPhone::model()->getIndex(toDestroy)) != -1)
      takeTopLevelItem(indexOfTopLevelItem(SFLPhone::model()->getIndex(toDestroy)));
   else if (SFLPhone::model()->getIndex(toDestroy)->parent()) //May crash here
      SFLPhone::model()->getIndex(toDestroy)->parent()->removeChild(SFLPhone::model()->getIndex(toDestroy));
   else
      kDebug() << "Call not found";
}

/// @todo Remove the text partially covering the TreeView item widget when it is being dragged, a beter implementation is needed
void CallView::clearArtefact(QTreeWidgetItem* item) 
{
   if (item)
      item->setText(0,"");
}


/*****************************************************************************
 *                                                                           *
 *                           Event related code                              *
 *                                                                           *
 ****************************************************************************/

void CallView::itemDoubleClicked(QTreeWidgetItem* item, int column) {
   Q_UNUSED(column)
   kDebug() << "Item doubleclicked" << SFLPhone::model()->getCall(item)->getState();
   switch(SFLPhone::model()->getCall(item)->getState()) {
      case CALL_STATE_INCOMING:
         SFLPhone::model()->getCall(item)->actionPerformed(CALL_ACTION_ACCEPT);
         break;
      case CALL_STATE_HOLD:
         SFLPhone::model()->getCall(item)->actionPerformed(CALL_ACTION_HOLD);
         break;
      case CALL_STATE_DIALING:
         SFLPhone::model()->getCall(item)->actionPerformed(CALL_ACTION_ACCEPT);
         break;
      default:
         kDebug() << "Double clicked an item with no action on double click.";
    }
}

void CallView::itemClicked(QTreeWidgetItem* item, int column) {
   Q_UNUSED(column)
   emit itemChanged(SFLPhone::model()->getCall(item));
   kDebug() << "Item clicked";
}


/*****************************************************************************
 *                                                                           *
 *                         Conference related code                           *
 *                                                                           *
 ****************************************************************************/

///Add a new conference, get the call list and update the interface as needed
Call* CallView::addConference(Call* conf) 
{
   kDebug() << "Conference created";
   Call* newConf =  conf;//SFLPhone::model()->addConference(confID);//TODO ELV?
   
   QTreeWidgetItem* confItem = new QTreeWidgetItem();
   SFLPhone::model()->updateIndex(conf,confItem);

   insertItem(confItem,(QTreeWidgetItem*)0);

   
   setCurrentItem(confItem);

   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   QStringList callList = callManager.getParticipantList(conf->getConfId());
   
   foreach (QString callId, callList) {
      kDebug() << "Adding " << callId << "to the conversation";
      insertItem(extractItem(SFLPhone::model()->getIndex(callId)),confItem);
   }
   
   Q_ASSERT_X(confItem->childCount() == 0, "add conference","Conference created, but without any participants");
   return newConf;
}

///Executed when the daemon signal a modification in an existing conference. Update the call list and update the TreeView
bool CallView::conferenceChanged(Call* conf) 
{
   kDebug() << "Conference changed";
   //if (!SFLPhone::model()->conferenceChanged(confId, state))
   //  return false;

   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   QStringList callList = callManager.getParticipantList(conf->getConfId());

   QList<QTreeWidgetItem*> buffer;
   foreach (QString callId, callList) {
      if (SFLPhone::model()->getCall(callId)) {
         QTreeWidgetItem* item3 = extractItem(SFLPhone::model()->getIndex(callId));
         insertItem(item3, SFLPhone::model()->getIndex(conf));
         buffer << SFLPhone::model()->getIndex(callId);
      }
      else
         kDebug() << "Call " << callId << " does not exist";
   }

   for (int j =0; j < SFLPhone::model()->getIndex(conf)->childCount();j++) {
      if (buffer.indexOf(SFLPhone::model()->getIndex(conf)->child(j)) == -1)
         insertItem(extractItem(SFLPhone::model()->getIndex(conf)->child(j)));
   }
   
   Q_ASSERT_X(SFLPhone::model()->getIndex(conf)->childCount() == 0,"changind conference","A conference can't have no participants");
   
   return true;
}

///Remove a conference from the model and the TreeView
void CallView::conferenceRemoved(Call* conf) 
{
   kDebug() << "Attempting to remove conference";
   QTreeWidgetItem* idx = SFLPhone::model()->getIndex(conf);
   if (idx) {
   while (idx->childCount()) {
      insertItem(extractItem(SFLPhone::model()->getIndex(conf)->child(0)));
   }
   takeTopLevelItem(indexOfTopLevelItem(SFLPhone::model()->getIndex(conf)));
   //SFLPhone::model()->conferenceRemoved(confId);
   kDebug() << "Conference removed";
   }
   else {
      kDebug() << "Conference not found";
   }
}

///Clear the list of old calls //TODO Clear them from the daemon
void CallView::clearHistory()
{
   //SFLPhone::model()->getHistory().clear();
}

///Redirect keypresses to parent
void CallView::keyPressEvent(QKeyEvent* event) {
   SFLPhone::app()->view()->keyPressEvent(event);
}
