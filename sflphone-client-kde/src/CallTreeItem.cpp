/***************************************************************************
 *   Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).     *
 *   All rights reserved.                                                  *
 *   Contact: Nokia Corporation (qt-info@nokia.com)                        *
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

#include <QtCore/QStringList>

#include <klocale.h>
#include <kdebug.h>

#include "sflphone_const.h"
#include "CallTreeItem.h"

const char * CallTreeItem::callStateIcons[11] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", ""};

CallTreeItem::CallTreeItem(const QVector<QVariant> &data, CallTreeItem *parent)
   : parentItem(parent),
     itemCall(0),
     itemWidget(0),
     itemData(data)
 {
    
 }
 
CallTreeItem::CallTreeItem(const CallTreeItem *toCopy, CallTreeItem *parent)
    : parentItem(parent),
      itemCall(toCopy->itemCall),
      itemWidget(toCopy->itemWidget),
      itemData(toCopy->itemData)
{
  
}

 CallTreeItem::~CallTreeItem()
 {
     qDeleteAll(childItems);
 }

 CallTreeItem *CallTreeItem::child(int number)
 {
     return childItems.value(number);
 }

 int CallTreeItem::childCount() const
 {
     return childItems.count();
 }

 int CallTreeItem::childNumber() const
 {
     if (parentItem)
         return parentItem->childItems.indexOf(const_cast<CallTreeItem*>(this));
     return 0;
 }

 int CallTreeItem::columnCount() const
 {
     return itemData.count();
 }

 QVariant CallTreeItem::data(int column) const
 {
     return itemData.value(column);
 }

Call* CallTreeItem::call() const
{
   return itemCall;
}

QWidget* CallTreeItem::widget() const
{
   return itemWidget;
}

 bool CallTreeItem::insertChildren(int position, int count, int columns)
 {
     if (position < 0 || position > childItems.size())
         return false;

     for (int row = 0; row < count; ++row) {
         QVector<QVariant> data(columns);
         CallTreeItem *item = new CallTreeItem(data, this);
         childItems.insert(position, item);
     }

     return true;
 }

 bool CallTreeItem::insertColumns(int position, int columns)
 {
     if (position < 0 || position > itemData.size())
         return false;

     for (int column = 0; column < columns; ++column) {
         itemData.insert(position, QVariant());
     }

     foreach (CallTreeItem *child, childItems) {
         child->insertColumns(position, columns);
     }

     return true;
 }

 CallTreeItem *CallTreeItem::parent()
 {
     return parentItem;
 }

 bool CallTreeItem::removeChildren(int position, int count)
 {
     if (position < 0 || position + count > childItems.size())
         return false;

     for (int row = 0; row < count; ++row) {
         delete childItems.takeAt(position);
     }

     return true;
 }

 bool CallTreeItem::removeColumns(int position, int columns)
 {
     if (position < 0 || position + columns > itemData.size())
         return false;

     for (int column = 0; column < columns; ++column) {
         itemData.remove(position);
     }

     foreach (CallTreeItem *child, childItems) {
         child->removeColumns(position, columns);
     }

     return true;
 }

 bool CallTreeItem::setData(int column, const QVariant &value)
 {
    itemData.resize(10);
    if (column < 0 || column >= itemData.size()) {
      qDebug() << "Je suis ici!!!! " << itemData;
        return false;
    }

    itemData[column] = value;
    return true;
 }


void CallTreeItem::setCall(Call *call)
{
   itemCall = call;

   itemWidget = new QWidget();

   labelIcon = new QLabel();
   //labelCallNumber = new QLabel("123"/*itemCall->getPeerPhoneNumber()*/);
        labelCallNumber2 = new QLabel(itemCall->getPeerPhoneNumber());
   labelTransferPrefix = new QLabel(i18n("Transfer to : "));
   labelTransferNumber = new QLabel();
   QSpacerItem * horizontalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Preferred, QSizePolicy::Minimum);
        QSpacerItem * verticalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Expanding, QSizePolicy::Expanding);
   
   QHBoxLayout * mainLayout = new QHBoxLayout();
   mainLayout->setContentsMargins ( 3, 1, 2, 1);
   
   mainLayout->setSpacing(4);
   QVBoxLayout * descr = new QVBoxLayout();
   descr->setMargin(1);
   descr->setSpacing(1);
   QHBoxLayout * transfer = new QHBoxLayout();
   transfer->setMargin(0);
   transfer->setSpacing(0);
   mainLayout->addWidget(labelIcon);
        
   if(! itemCall->getPeerName().isEmpty()) {
      labelPeerName = new QLabel(itemCall->getPeerName());
      descr->addWidget(labelPeerName);
   }

   descr->addWidget(labelCallNumber2);
   transfer->addWidget(labelTransferPrefix);
   transfer->addWidget(labelTransferNumber);
   descr->addLayout(transfer);
        descr->addItem(verticalSpacer);
   mainLayout->addLayout(descr);
   //mainLayout->addItem(horizontalSpacer);
   
   itemWidget->setLayout(mainLayout);
   itemWidget->setMinimumSize(QSize(50, 30));

   connect(itemCall, SIGNAL(changed()),
      this,     SLOT(updated()));

   updated();   
}

void CallTreeItem::updated()
{
   call_state state = itemCall->getState();
   bool recording = itemCall->getRecording();

   if(state != CALL_STATE_OVER) {
      if(state == CALL_STATE_CURRENT && recording) {
         labelIcon->setPixmap(QPixmap(ICON_CURRENT_REC));
      }
      else {
         QString str = QString(callStateIcons[state]);
         labelIcon->setPixmap(QPixmap(str));
      }
      bool transfer = state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD;
      labelTransferPrefix->setVisible(transfer);
      labelTransferNumber->setVisible(transfer);

      if(!transfer) {
         labelTransferNumber->setText("");
      }
      //labelTransferNumber->setText(itemCall->getTransferNumber());
      labelCallNumber2->setText(itemCall->getPeerPhoneNumber());
                
                if(state == CALL_STATE_DIALING) {
                  labelCallNumber2->setText(itemCall->getCallNumber());
                }
   }
   else {
                emit over(itemCall);
                itemWidget->setVisible(false);
       qDebug() << "Updating item of call of state OVER. Doing nothing.";
   }

   
}

