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
#include <QtGui/QPainter>
#include <QtGui/QClipboard>
#include <QtGui/QApplication>
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
#include <QtGui/QFontMetrics>

//KDE
#include <KLocale>
#include <KDebug>
#include <KIcon>
#include <KStandardDirs>

//SFLPhone library
#include "lib/sflphone_const.h"
#include "lib/Contact.h"
#include "lib/Call.h"
#include "klib/ConfigurationSkeleton.h"

//SFLPhone
#include "klib/AkonadiBackend.h"
#include "widgets/TranslucentButtons.h"
#include "SFLPhone.h"

///Constant
const char * CallTreeItem::callStateIcons[12] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", "", ICON_CONFERENCE};

///Constructor
CallTreeItem::CallTreeItem(QWidget *parent)
   : QWidget(parent), m_pItemCall(0), m_Init(false),m_pBtnConf(0), m_pBtnTrans(0),m_pTimer(0),m_pPeerL(0),m_pIconL(0),m_pCallNumberL(0),m_pSecureL(0),m_pCodecL(0),m_pHistoryPeerL(0)
   , m_pTransferPrefixL(0),m_pTransferNumberL(0),m_pElapsedL(0),m_Height(0)
{
   setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
   connect(AkonadiBackend::getInstance(),SIGNAL(collectionChanged()),this,SLOT(updated()));
}

///Destructor
CallTreeItem::~CallTreeItem()
{
    if (m_pIconL)           delete m_pIconL           ;
    if (m_pPeerL)           delete m_pPeerL           ;
    if (m_pCallNumberL)     delete m_pCallNumberL     ;
    if (m_pTransferPrefixL) delete m_pTransferPrefixL ;
    if (m_pTransferNumberL) delete m_pTransferNumberL ;
    if (m_pCodecL)          delete m_pCodecL          ;
    if (m_pSecureL)         delete m_pSecureL         ;
    if (m_pHistoryPeerL)    delete m_pHistoryPeerL    ;
    if (m_pElapsedL)        delete m_pElapsedL        ;
    if (m_pTimer)           delete m_pTimer           ;
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

///Return the real size hint
QSize CallTreeItem::sizeHint () const
{
   uint height =0;
   if (m_pItemCall && !m_pItemCall->isConference()) {
      if ( m_pPeerL       ) {
         QFontMetrics fm(m_pPeerL->font());
         height += fm.height();
      }
      if ( m_pCallNumberL ) {
         QFontMetrics fm(m_pCallNumberL->font());
         height += fm.height();
      }
      if ( m_pSecureL     ) {
         QFontMetrics fm(m_pSecureL->font());
         height += fm.height();
      }
      if ( m_pCodecL      ) {
         QFontMetrics fm(m_pCodecL->font());
         height += fm.height();
      }
   }
   else {
      height = 32;
   }

   if (ConfigurationSkeleton::limitMinimumRowHeight() && height < (uint)ConfigurationSkeleton::minimumRowHeight()) {
      height = (uint)ConfigurationSkeleton::minimumRowHeight();
   }

   if (height != m_Height) {
      ((CallTreeItem*)this)->m_Height=height;
      if (m_pIconL && height) {
         m_pIconL->setMinimumSize(m_Height,m_Height);
         m_pIconL->setMaximumSize(m_Height,m_Height);
         m_pIconL->resize(m_Height,m_Height);
         ((CallTreeItem*)this)->updated();
      }
   }
   return QSize(0,height);
}

/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Set the call item
void CallTreeItem::setCall(Call *call)
{
   if (!call)
      return;
   
   m_pItemCall = call;
   setAcceptDrops(true);

   if (m_pItemCall->isConference()) {
      if (!m_Init) {
         m_pHistoryPeerL = new QLabel(i18n("Conference"),this);
         m_pIconL = new QLabel(" ",this);
         QHBoxLayout* mainLayout = new QHBoxLayout();
         mainLayout->addWidget(m_pIconL);
         mainLayout->addWidget(m_pHistoryPeerL);
         setLayout(mainLayout);
         m_Init = true;
      }
      m_pIconL->setVisible(true);
      m_pIconL->setStyleSheet("margin-top:"+(height()-32)/2);
      m_pHistoryPeerL->setVisible(true);
      return;
   }

   m_pTransferPrefixL  = new QLabel(i18n("Transfer to : "));
   m_pTransferNumberL  = new QLabel(" ");
   m_pElapsedL         = new QLabel(" ");
   QSpacerItem* verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);

   m_pTransferPrefixL->setVisible(false);
   m_pTransferNumberL->setVisible(false);
   
   QHBoxLayout* mainLayout = new QHBoxLayout();
   mainLayout->setContentsMargins ( 3, 1, 2, 1);


   m_pBtnConf = new TranslucentButtons(this);
   m_pBtnConf->setVisible(false);
   m_pBtnConf->setParent(this);
   m_pBtnConf->setText(i18n("Conference"));
   m_pBtnConf->setPixmap(new QImage(KStandardDirs::locate("data","sflphone-client-kde/confBlackWhite.png")));
   connect(m_pBtnConf,SIGNAL(dataDropped(QMimeData*)),this,SLOT(conversationEvent(QMimeData*)));

   m_pBtnTrans = new TranslucentButtons(this);
   m_pBtnTrans->setText(i18n("Transfer"));
   m_pBtnTrans->setVisible(false);
   m_pBtnTrans->setPixmap(new QImage(KStandardDirs::locate("data","sflphone-client-kde/transferarraw.png")));
   connect(m_pBtnTrans,SIGNAL(dataDropped(QMimeData*)),this,SLOT(transferEvent(QMimeData*)));

   m_pElapsedL->setStyleSheet("margin-right:5px;");

   mainLayout->setSpacing(4);
   QVBoxLayout* descr = new QVBoxLayout();
   descr->setMargin(1);
   descr->setSpacing(1);
   QHBoxLayout* transfer = new QHBoxLayout();
   transfer->setMargin(0);
   transfer->setSpacing(0);
   
   if (ConfigurationSkeleton::displayCallIcon()) {
      m_pIconL = new QLabel(" ");
      mainLayout->addWidget(m_pIconL);
   }

   if(ConfigurationSkeleton::displayCallPeer()/*&& ! m_pItemCall->getPeerName().isEmpty()*/) {
      m_pPeerL = new QLabel(" ");
      m_pPeerL->setText(m_pItemCall->getPeerName());
      if (m_pItemCall->getPeerName().isEmpty()) {
         m_pPeerL->setVisible(true);
      }
      descr->addWidget(m_pPeerL);
   }

   if (ConfigurationSkeleton::displayCallNumber()) {
      m_pCallNumberL = new QLabel(m_pItemCall->getPeerPhoneNumber());
      descr->addWidget(m_pCallNumberL);
   }

   if (ConfigurationSkeleton::displayCallSecure()) {
      m_pSecureL = new QLabel(" ",this);
      descr->addWidget(m_pSecureL);
   }
   
   if (ConfigurationSkeleton::displayCallCodec()) {
      m_pCodecL = new QLabel(" ",this);
      descr->addWidget(m_pCodecL);
   }
   
   transfer->addWidget(m_pTransferPrefixL);
   transfer->addWidget(m_pTransferNumberL);
   descr->addLayout(transfer);
   descr->addItem(verticalSpacer);
   mainLayout->addLayout(descr);
   mainLayout->addWidget(m_pElapsedL);

   setLayout(mainLayout);

   connect(m_pItemCall, SIGNAL(changed()), this, SLOT(updated()));
   sizeHint();
   
} //setCall

///Update data
void CallTreeItem::updated()
{
   call_state state = m_pItemCall->getState();
   bool recording = m_pItemCall->getRecording();
   Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(m_pItemCall->getPeerPhoneNumber(),true);
   if (contact && m_pPeerL) {
      m_pPeerL->setText("<b>"+contact->getFormattedName()+"</b>");
      m_pPeerL->setVisible(true);
   }
   else {
      if( m_pPeerL && ! m_pItemCall->getPeerName().trimmed().isEmpty()) {
         m_pPeerL->setText("<b>"+m_pItemCall->getPeerName()+"</b>");
         m_pPeerL->setVisible(true);
      }
      else if (m_pPeerL && !(state == CALL_STATE_RINGING || state == CALL_STATE_DIALING)) {
         m_pPeerL->setText(i18n("<b>Unknown</b>"));
         m_pPeerL->setVisible(true);
      }
      else if (m_pPeerL) {
         m_pPeerL->setText(i18n(""));
         m_pPeerL->setVisible(false);
      }
   }

   if(state != CALL_STATE_OVER) {
      if(m_pIconL && state == CALL_STATE_CURRENT && recording) {
         if (contact && !m_pItemCall->isConference()) {
            QPixmap pxm = (*contact->getPhoto()).scaled(QSize(m_Height,m_Height));
            QPainter painter(&pxm);
            QPixmap status(ICON_CURRENT_REC);
            painter.drawPixmap(pxm.width()-status.width(),pxm.height()-status.height(),status);
            m_pIconL->setPixmap(pxm);
         }
         else if (!m_pItemCall->isConference()) {
            m_pIconL->setPixmap(QPixmap(ICON_CURRENT_REC));
         }
      }
      else if (m_pIconL) {
         QString str = QString(callStateIcons[state]);
         if (contact && !m_pItemCall->isConference()) {
            QPixmap pxm = (*contact->getPhoto()).scaled(QSize(m_Height,m_Height));
            QPainter painter(&pxm);
            QPixmap status(str);
            painter.drawPixmap(pxm.width()-status.width(),pxm.height()-status.height(),status);
            m_pIconL->setPixmap(pxm);
         }
         else if (!m_pItemCall->isConference()) {
            m_pIconL->setPixmap(QPixmap(str));
         }
      }

      if (m_pIconL && m_pItemCall->isConference()) {
         m_pIconL->setPixmap(QPixmap(ICON_CONFERENCE).scaled(QSize(m_Height,m_Height)));
      }

      bool transfer = state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD;
      if (m_pTransferPrefixL && m_pTransferNumberL) {
         m_pTransferPrefixL->setVisible(transfer);
         m_pTransferNumberL->setVisible(transfer);
      }

      if(!transfer && m_pTransferNumberL) {
         m_pTransferNumberL->setText("");
      }
      if (m_pTransferNumberL)
         m_pTransferNumberL->setText(m_pItemCall->getTransferNumber());
      
      if (m_pCallNumberL)
         m_pCallNumberL->setText(m_pItemCall->getPeerPhoneNumber());

      if(m_pCallNumberL && state == CALL_STATE_DIALING) {
         m_pCallNumberL->setText(m_pItemCall->getCallNumber());
      }
      else {
         if (m_pCodecL)
            m_pCodecL->setText("Codec: "+m_pItemCall->getCurrentCodecName());
         if (m_pSecureL && m_pItemCall->isSecure())
            m_pSecureL->setText("⚷");
      }
   }
   else {
      //kDebug() << "Updating item of call of state OVER. Doing nothing.";
   }
   if (state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD) {
      kDebug() << "emmiting tranfer signal";
      emit askTransfer(m_pItemCall);
   }
   else {
      kDebug() << "not emmiting tranfer signal";
   }
   changed();

   if (state == CALL_STATE_CURRENT && !m_pTimer) {
      m_pTimer = new QTimer(this);
      m_pTimer->setInterval(1000);
      connect(m_pTimer,SIGNAL(timeout()),this,SLOT(incrementTimer()));
      m_pTimer->start();
   }
   else if (m_pTimer && (state == CALL_STATE_OVER || state == CALL_STATE_ERROR || state == CALL_STATE_FAILURE )) {
      m_pTimer->stop();
   }
} //updated


/*****************************************************************************
 *                                                                           *
 *                               Drag and drop                               *
 *                                                                           *
 ****************************************************************************/

///Called when a drag and drop occure while the item have not been dropped yet
void CallTreeItem::dragEnterEvent ( QDragEnterEvent *e )
{
   kDebug() << "Drag enter";
   if (SFLPhone::model()->getIndex(this) && SFLPhone::model()->getIndex(this)->parent() &&
      SFLPhone::model()->getIndex(e->mimeData()->data( MIME_CALLID))->parent() &&
      SFLPhone::model()->getIndex(this)->parent() == SFLPhone::model()->getIndex(e->mimeData()->data( MIME_CALLID))->parent() &&
      e->mimeData()->data( MIME_CALLID) != SFLPhone::model()->getCall(this)->getCallId()) {
      m_pBtnTrans->setVisible(true);
      emit showChilds(this);
      m_isHover = true;
      e->accept();
   }
   else if (e->mimeData()->hasFormat( MIME_CALLID) && m_pBtnTrans && (e->mimeData()->data( MIME_CALLID) != m_pItemCall->getCallId())) {
      m_pBtnConf->setVisible(true);
      m_pBtnTrans->setVisible(true);
      emit showChilds(this);
      m_isHover = true;
      e->accept();
   }
   else
      e->ignore();
} //dragEnterEvent

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
   kDebug() << "Drop accepted";
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
   if (m_pBtnConf) {
      m_pBtnConf->setMinimumSize(width()/2-15,height()-4);
      m_pBtnConf->setMaximumSize(width()/2-15,height()-4);
      m_pBtnTrans->setMinimumSize(width()/2-15,height()-4);
      m_pBtnTrans->setMaximumSize(width()/2-15,height()-4);
      m_pBtnTrans->move(width()/2+10,m_pBtnTrans->y()+2);
      m_pBtnConf->move(10,m_pBtnConf->y()+2);
   }

   e->accept();
} //resizeEvent

void CallTreeItem::mouseDoubleClickEvent(QMouseEvent *e )
{
   if (m_pItemCall && m_pItemCall->isConference() && m_pItemCall->getState() == CALL_STATE_CONFERENCE_HOLD) {
      e->accept();
      m_pItemCall->actionPerformed(CALL_ACTION_HOLD);
   }
   else {
      e->ignore();
   }
}

///Called when a call is dropped on transfer
void CallTreeItem::transferEvent(QMimeData* data)
{
   emit transferDropEvent(m_pItemCall,data);
}

///Called when a call is dropped on conversation
void CallTreeItem::conversationEvent(QMimeData* data)
{
   kDebug() << "Proxying conversation mime";
   emit conversationDropEvent(m_pItemCall,data);
}

///Copy
void CallTreeItem::copy()
{
   kDebug() << "Copying contact";
   QMimeData* mimeData = new QMimeData();
   mimeData->setData(MIME_CALLID, m_pItemCall->getCallId().toUtf8());
   QString numbers(m_pItemCall->getPeerName()+": "+m_pItemCall->getPeerPhoneNumber());
   QString numbersHtml("<b>"+m_pItemCall->getPeerName()+"</b><br />"+m_pItemCall->getPeerPhoneNumber());
   mimeData->setData("text/plain", numbers.toUtf8());
   mimeData->setData("text/html", numbersHtml.toUtf8());
   QClipboard* clipboard = QApplication::clipboard();
   clipboard->setMimeData(mimeData);
}

///Called when the overlay need to be hidden
void CallTreeItem::hide()
{
   if (!m_isHover) {
      m_pBtnConf->setVisible(false);
      m_pBtnTrans->setVisible(false);
   }
}

///Increment the current call elapsed time label
void CallTreeItem::incrementTimer()
{
   int nsec = QDateTime::fromTime_t(m_pItemCall->getStartTimeStamp().toInt()).time().secsTo( QTime::currentTime() );
   if (nsec/3600)
      m_pElapsedL->setText(QString("%1").arg(nsec/3600).trimmed()+":"+QString("%1").arg((nsec%3600)/60,2,10,QChar('0')).trimmed()+":"+QString("%1").arg((nsec%3600)%60,2,10,QChar('0')).trimmed()+" ");
   else
      m_pElapsedL->setText(QString("%1").arg((nsec)/60).trimmed()+":"+QString("%1").arg((nsec)%60,2,10,QChar('0')).trimmed()+" ");
}