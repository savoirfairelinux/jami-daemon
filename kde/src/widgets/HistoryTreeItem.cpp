/***************************************************************************
 *   Author : Mathieu Leduc-Hamel mathieu.leduc-hamel@savoirfairelinux.com *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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
#include <QtGui/QGridLayout>

#include <klocale.h>
#include <kdebug.h>
#include <unistd.h>

#include "lib/sflphone_const.h"
#include "HistoryTreeItem.h"

const char * HistoryTreeItem::callStateIcons[12] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", "", ICON_CONFERENCE};

HistoryTreeItem::HistoryTreeItem(QWidget *parent)
   : QWidget(parent), itemCall(0), init(false)
{
   
}

HistoryTreeItem::~HistoryTreeItem()
{
   
}

Call* HistoryTreeItem::call() const
{
   return itemCall;
}

void HistoryTreeItem::setCall(Call *call)
{
   itemCall = call;
   
   if (itemCall->isConference()) {
      labelIcon->setVisible(true);
      return;
   }
      labelIcon = new QLabel(this);
      labelPeerName = new QLabel();
   labelIcon = new QLabel();
   labelIcon->setMinimumSize(70,48);
   labelIcon->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
   
   labelCallNumber2 = new QLabel(itemCall->getPeerPhoneNumber());
   QSpacerItem* verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);
   
      
   labelIcon->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));
        
   if(! itemCall->getPeerName().trimmed().isEmpty()) {
      labelPeerName = new QLabel("<b>"+itemCall->getPeerName()+"</b>");
   }
   else {
      labelPeerName = new QLabel("<b>Unknow</b>");
   }

   m_pTimeL = new QLabel();
   m_pTimeL->setText(QDateTime::fromTime_t(itemCall->getStartTimeStamp().toUInt()).toString());

   m_pDurationL = new QLabel();
   int dur = itemCall->getStopTimeStamp().toInt() - itemCall->getStartTimeStamp().toInt();
   m_pDurationL->setText(QString("%1").arg(dur/3600,2)+":"+QString("%1").arg((dur%3600)/60,2)+":"+QString("%1").arg((dur%3600)%60,2)+" ");

   QGridLayout* mainLayout = new QGridLayout(this);
   mainLayout->addWidget(labelIcon,0,0,4,1);
   mainLayout->addWidget(labelPeerName,0,1);
   mainLayout->addWidget(labelCallNumber2,1,1);
   mainLayout->addWidget(m_pTimeL,2,1);
   mainLayout->addItem(verticalSpacer,3,1);
   mainLayout->addWidget(m_pDurationL,0,2,4,1);
   
   setLayout(mainLayout);
   setMinimumSize(QSize(50, 30));

   connect(itemCall, SIGNAL(changed()),
           this,     SLOT(updated()));

   updated();

   m_pTimeStamp = itemCall->getStartTimeStamp().toUInt();
   m_pDuration = dur;
   m_pName = itemCall->getPeerName();
   m_pPhoneNumber = itemCall->getPeerPhoneNumber();
}

void HistoryTreeItem::updated()
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
      labelCallNumber2->setText(itemCall->getPeerPhoneNumber());
                
      if(state == CALL_STATE_DIALING) {
         labelCallNumber2->setText(itemCall->getCallNumber());
      }
   }
   else {
      qDebug() << "Updating item of call of state OVER. Doing nothing.";
   }
   
}

uint HistoryTreeItem::getTimeStamp()
{
   return m_pTimeStamp;
}

uint HistoryTreeItem::getDuration()
{
   return m_pDuration;
}

QString HistoryTreeItem::getName()
{
   return m_pName;
}

QString HistoryTreeItem::getPhoneNumber()
{
   return m_pPhoneNumber;
}


QTreeWidgetItem* HistoryTreeItem::getItem()
{
   return m_pItem;
}

void HistoryTreeItem::setItem(QTreeWidgetItem* item)
{
   m_pItem = item;
}