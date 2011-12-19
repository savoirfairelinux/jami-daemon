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
 **************************************************************************/

//Parent
#include "CallTreeItem.h"

//Qt
#include <QtCore/QStringList>
#include <QtCore/QMimeData>
#include <QtCore/QTimer>
#include <QtGui/QWidget>
#include <QtGui/QLabel>
#include <QtGui/QSpacerItem>
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QPushButton>
#include <QtGui/QTreeWidgetItem>

//KDE
#include <KLocale>
#include <KDebug>
#include <KIcon>
#include <KStandardDirs>

//SFLPhone library
#include "lib/sflphone_const.h"
#include "lib/Contact.h"
#include "lib/Call.h"

//SFLPhone
#include "AkonadiBackend.h"
#include "widgets/TranslucentButtons.h"
#include "SFLPhone.h"

///Constant
const char * CallTreeItem::callStateIcons[12] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", "", ICON_CONFERENCE};

///Constructor
CallTreeItem::CallTreeItem(QWidget *parent)
   : QWidget(parent), m_pItemCall(0), m_Init(false),m_pBtnConf(0), m_pBtnTrans(0)
{
   setMaximumSize(99999,50);
}

///Destructor
CallTreeItem::~CallTreeItem()
{
   
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Return the call item
Call* CallTreeItem::call() const
{
   return m_pItemCall;
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Set the call item
void CallTreeItem::setCall(Call *call)
{
   m_pItemCall = call;
   setAcceptDrops(true);
   
   if (m_pItemCall->isConference()) {
      if (!m_Init) {
         m_pHistoryPeerL = new QLabel(i18n("Conference"),this);
         m_pIconL = new QLabel("",this);
         QHBoxLayout* mainLayout = new QHBoxLayout();
         mainLayout->addWidget(m_pIconL);
         mainLayout->addWidget(m_pHistoryPeerL);
         setLayout(mainLayout);
         m_Init = true;
      }
      m_pIconL->setPixmap(QPixmap(ICON_CONFERENCE).scaled(QSize(48,48)));
      m_pIconL->setVisible(true);
      m_pHistoryPeerL->setVisible(true);
      return;
   }

   m_pIconL            = new QLabel();
   m_pCallNumberL      = new QLabel(m_pItemCall->getPeerPhoneNumber());
   m_pTransferPrefixL  = new QLabel(i18n("Transfer to : "));
   m_pTransferNumberL  = new QLabel();
   m_pPeerL            = new QLabel();
   QSpacerItem* verticalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Expanding, QSizePolicy::Expanding);
   
   QHBoxLayout* mainLayout = new QHBoxLayout();
   mainLayout->setContentsMargins ( 3, 1, 2, 1);

   
   m_pBtnConf = new TranslucentButtons(this);
   m_pBtnConf->setVisible(false);
   m_pBtnConf->setParent(this);
   m_pBtnConf->setText("Conference");
   m_pBtnConf->setPixmap(new QImage(KStandardDirs::locate("data","sflphone-client-kde/confBlackWhite.png")));
   connect(m_pBtnConf,SIGNAL(dataDropped(QMimeData*)),this,SLOT(conversationEvent(QMimeData*)));

   m_pBtnTrans = new TranslucentButtons(this);
   m_pBtnTrans->setText("Transfer");
   m_pBtnTrans->setVisible(false);
   m_pBtnTrans->setPixmap(new QImage(KStandardDirs::locate("data","sflphone-client-kde/transferarraw.png")));
   connect(m_pBtnTrans,SIGNAL(dataDropped(QMimeData*)),this,SLOT(transferEvent(QMimeData*)));
   
   m_pCodecL = new QLabel(this);
   //m_pCodecL->setText("Codec: "+m_pItemCall->getCurrentCodecName());

   m_pSecureL = new QLabel(this);
   
   mainLayout->setSpacing(4);
   QVBoxLayout* descr = new QVBoxLayout();
   descr->setMargin(1);
   descr->setSpacing(1);
   QHBoxLayout* transfer = new QHBoxLayout();
   transfer->setMargin(0);
   transfer->setSpacing(0);
   mainLayout->addWidget(m_pIconL);
   
   if(! m_pItemCall->getPeerName().isEmpty()) {
      m_pPeerL->setText(m_pItemCall->getPeerName());
      descr->addWidget(m_pPeerL);
   }

   descr->addWidget(m_pCallNumberL);
   descr->addWidget(m_pSecureL);
   descr->addWidget(m_pCodecL);
   transfer->addWidget(m_pTransferPrefixL);
   transfer->addWidget(m_pTransferNumberL);
   descr->addLayout(transfer);
   descr->addItem(verticalSpacer);
   mainLayout->addLayout(descr);
   
   setLayout(mainLayout);
   setMinimumSize(QSize(50, 30));

   connect(m_pItemCall, SIGNAL(changed()), this,     SLOT(updated()));

   updated();
}

///Update data
void CallTreeItem::updated()
{
   kDebug() << "\n\n\n\nI am here\n\n\n\n\n" << m_pItemCall->getState() << "\n\n\n";
   kDebug() << "Updating tree item";
   Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(m_pItemCall->getPeerPhoneNumber());
   if (contact) {
      m_pIconL->setPixmap(*contact->getPhoto());
      m_pPeerL->setText("<b>"+contact->getFormattedName()+"</b>");
   }
   else {
      m_pIconL->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));

      if(! m_pItemCall->getPeerName().trimmed().isEmpty()) {
         m_pPeerL->setText("<b>"+m_pItemCall->getPeerName()+"</b>");
      }
      else {
         m_pPeerL->setText(i18n("<b>Unknow</b>"));
      }
   }
      
   call_state state = m_pItemCall->getState();
   bool recording = m_pItemCall->getRecording();
   if(state != CALL_STATE_OVER) {
      if(state == CALL_STATE_CURRENT && recording) {
         m_pIconL->setPixmap(QPixmap(ICON_CURRENT_REC));
      }
      else {
         QString str = QString(callStateIcons[state]);
         m_pIconL->setPixmap(QPixmap(str));
      }
      bool transfer = state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD;
      m_pTransferPrefixL->setVisible(transfer);
      m_pTransferNumberL->setVisible(transfer);

      if(!transfer) {
         m_pTransferNumberL->setText("");
      }
      m_pTransferNumberL->setText(m_pItemCall->getTransferNumber());
      m_pCallNumberL->setText(m_pItemCall->getPeerPhoneNumber());
                
      if(state == CALL_STATE_DIALING) {
         m_pCallNumberL->setText(m_pItemCall->getCallNumber());
      }
      else {
         m_pCodecL->setText("Codec: "+m_pItemCall->getCurrentCodecName());
         if (m_pItemCall->isSecure())
            m_pSecureL->setText("⚷");
      }
   }
   else {
      //kDebug() << "Updating item of call of state OVER. Doing nothing.";
   }
   if (state == CALL_STATE_TRANSFER) {
      kDebug() << "emmiting tranfer signal";
      emit askTransfer(m_pItemCall);
   }
   else {
      kDebug() << "not emmiting tranfer signal";
   }
   changed();
}


/*****************************************************************************
 *                                                                           *
 *                               Drag and drop                               *
 *                                                                           *
 ****************************************************************************/

///Called when a drag and drop occure while the item have not been dropped yet
void CallTreeItem::dragEnterEvent ( QDragEnterEvent *e )
{
   kDebug() << "Drag enter";
   if (SFLPhone::model()->getIndex(this)->parent() &&
      SFLPhone::model()->getIndex(e->mimeData()->data( MIME_CALLID))->parent() &&
      SFLPhone::model()->getIndex(this)->parent() == SFLPhone::model()->getIndex(e->mimeData()->data( MIME_CALLID))->parent() &&
      e->mimeData()->data( MIME_CALLID) != SFLPhone::model()->getCall(this)->getCallId()) {
      m_pBtnTrans->setVisible(true);
      emit showChilds(this);
      m_pBtnConf->forceDragState(e);
      m_isHover = true;
      e->accept();
   }
   else if (e->mimeData()->hasFormat( MIME_CALLID) && m_pBtnTrans && (e->mimeData()->data( MIME_CALLID) != m_pItemCall->getCallId())) {
      m_pBtnConf->setVisible(true);
      m_pBtnTrans->setVisible(true);
      emit showChilds(this);
      m_pBtnConf->forceDragState(e);
      m_isHover = true;
      e->accept();
   }
   else
      e->ignore();
}

///The cursor move on a potential drag event
void CallTreeItem::dragMoveEvent  ( QDragMoveEvent  *e )
{
   QPoint pos = e->pos();
   m_pBtnConf->setHoverState (pos.x() < rect().width()/2);
   m_pBtnTrans->setHoverState(pos.x() > rect().width()/2);
   m_isHover = true;
   e->accept();
}

///A potential drag event is cancelled
void CallTreeItem::dragLeaveEvent ( QDragLeaveEvent *e )
{
   QTimer::singleShot(500, this, SLOT(hide()));
   kDebug() << "Drag leave";
   m_isHover = false;
   e->accept();
}

///Something is being dropped
void CallTreeItem::dropEvent(QDropEvent *e)
{
   kDebug() << "Drop accepted" << e->pos();
   QTimer::singleShot(500, this, SLOT(hide()));
   m_isHover = false;
   if (e->pos().x() < rect().width()/2) {
      emit conversationDropEvent(m_pItemCall,(QMimeData*)e->mimeData());
   }
   else {
      emit transferDropEvent(m_pItemCall,(QMimeData*)e->mimeData());
   }
   //emit dataDropped((QMimeData*)e->mimeData());
}

void CallTreeItem::resizeEvent ( QResizeEvent *e )
{
   kDebug() << "Resize";
   if (m_pBtnConf) {
      m_pBtnConf->setMinimumSize(width()/2-15,height()-4);
      m_pBtnConf->setMaximumSize(width()/2-15,height()-4);
      m_pBtnTrans->setMinimumSize(width()/2-15,height()-4);
      m_pBtnTrans->setMaximumSize(width()/2-15,height()-4);
      m_pBtnTrans->move(width()/2+10,m_pBtnTrans->y()+2);
      m_pBtnConf->move(10,m_pBtnConf->y()+2);
   }
   
   e->accept();
}

void CallTreeItem::transferEvent(QMimeData* data)
{
   emit transferDropEvent(m_pItemCall,data);
}

void CallTreeItem::conversationEvent(QMimeData* data)
{
   kDebug() << "Proxying conversation mime";
   emit conversationDropEvent(m_pItemCall,data);
}

void CallTreeItem::hide()
{
   if (!m_isHover) {
      m_pBtnConf->setVisible(false);
      m_pBtnTrans->setVisible(false);
   }
}