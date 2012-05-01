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
#include "HistoryTreeItem.h"

//Qt
#include <QtGui/QGridLayout>
#include <QtGui/QMenu>
#include <QtGui/QLabel>
#include <QtGui/QSpacerItem>
#include <QtGui/QHBoxLayout>
#include <QtGui/QToolButton>
#include <QtGui/QMessageBox>
#include <QtGui/QPainter>
#include <QtGui/QColor>
#include <QFontMetrics>
#include <QtCore/QStringList>
#include <QtCore/QFile>
#include <Phonon/MediaObject>
#include <Phonon/BackendCapabilities>
#include <Phonon/AudioOutput>
#include <Phonon/SeekSlider>

//KDE
#include <KLocale>
#include <KDebug>
#include <KAction>
#include <KIcon>
#include <KMessageBox>

//SFLPhone library
#include "lib/sflphone_const.h"
#include "lib/Contact.h"
#include "lib/Call.h"

//SFLPhone
#include "AkonadiBackend.h"
#include "SFLPhone.h"
#include "widgets/BookmarkDock.h"

const char * HistoryTreeItem::callStateIcons[12] = {ICON_INCOMING, ICON_RINGING, ICON_CURRENT, ICON_DIALING, ICON_HOLD, ICON_FAILURE, ICON_BUSY, ICON_TRANSFER, ICON_TRANSF_HOLD, "", "", ICON_CONFERENCE};

class PlayerWidget : public QWidget {
public:
   PlayerWidget(QWidget* parent = 0) : QWidget(parent) {}
protected:
   virtual void paintEvent(QPaintEvent* /*event*/)
   {
      QColor backgroundColor = palette().light().color();
      backgroundColor.setAlpha(200);
      QPainter customPainter(this);
      customPainter.fillRect(rect(),backgroundColor);
   }
};


///Constructor
HistoryTreeItem::HistoryTreeItem(QWidget *parent ,QString phone)
   : QWidget(parent), m_pItemCall(0),m_pMenu(0),m_pAudioSlider(0),m_pTimeLeftL(0),m_pTimePlayedL(0),m_pPlayer(0)
{
   setContextMenuPolicy(Qt::CustomContextMenu);

   m_pCallAgain    = new KAction(this);
   m_pAddContact   = new KAction(this);
   m_pCopy         = new KAction(this);
   m_pEmail        = new KAction(this);
   m_pAddToContact = new KAction(this);
   m_pBookmark     = new KAction(this);

   m_pCallAgain->setShortcut    ( Qt::CTRL + Qt::Key_Enter       );
   m_pCallAgain->setText        ( i18n("Call Again")             );
   m_pCallAgain->setIcon        ( KIcon(ICON_DIALING)            );

   m_pAddToContact->setShortcut ( Qt::CTRL + Qt::Key_E           );
   m_pAddToContact->setText     ( i18n("Add Number to Contact")  );
   m_pAddToContact->setIcon     ( KIcon("list-resource-add")     );
   m_pAddToContact->setDisabled ( true                           );

   m_pAddContact->setShortcut   ( Qt::CTRL + Qt::Key_E           );
   m_pAddContact->setText       ( i18n("Add Contact")            );
   m_pAddContact->setIcon       ( KIcon("contact-new")           );

   m_pCopy->setShortcut         ( Qt::CTRL + Qt::Key_C           );
   m_pCopy->setText             ( i18n("Copy")                   );
   m_pCopy->setIcon             ( KIcon("edit-copy")             );
   m_pCopy->setDisabled         ( true                           );

   m_pEmail->setShortcut        ( Qt::CTRL + Qt::Key_M           );
   m_pEmail->setText            ( i18n("Send Email")             );
   m_pEmail->setIcon            ( KIcon("mail-message-new")      );
   m_pEmail->setDisabled        ( true                           );

   m_pBookmark->setShortcut     ( Qt::CTRL + Qt::Key_D           );
   m_pBookmark->setText         ( i18n("Bookmark")               );
   m_pBookmark->setIcon         ( KIcon("bookmarks")             );

   m_pPlay = new QToolButton(this);

   m_pPlay->setIcon(KIcon("media-playback-start"));
   m_pPlay->setMinimumSize(30,30);
   m_pPlay->setMaximumSize(30,30);
   m_pPlay->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed));
   m_pPlay->setVisible(false);

   m_pRemove =  new QToolButton(this);
   m_pRemove->setIcon(KIcon("list-remove"));
   m_pRemove->setMinimumSize(30,30);
   m_pRemove->setMaximumSize(30,30);
   m_pRemove->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed));
   m_pRemove->setVisible(false);

   connect(m_pCallAgain    , SIGNAL(triggered())                        , this , SLOT(callAgain()         ));
   connect(m_pAddContact   , SIGNAL(triggered())                        , this , SLOT(addContact()        ));
   connect(m_pCopy         , SIGNAL(triggered())                        , this , SLOT(copy()              ));
   connect(m_pEmail        , SIGNAL(triggered())                        , this , SLOT(sendEmail()         ));
   connect(m_pAddToContact , SIGNAL(triggered())                        , this , SLOT(addToContact()      ));
   connect(m_pBookmark     , SIGNAL(triggered())                        , this , SLOT(bookmark()          ));
   connect(m_pPlay         , SIGNAL(clicked()  )                        , this , SLOT(showRecordPlayer()  ));
   connect(m_pRemove       , SIGNAL(clicked()  )                        , this , SLOT(removeRecording()   ));
   connect(this            , SIGNAL(customContextMenuRequested(QPoint)) , this , SLOT(showContext(QPoint) ));

   m_pIconL         = new QLabel( this );
   m_pPeerNameL     = new QLabel( this );
   m_pCallNumberL   = new QLabel( this );
   m_pDurationL     = new QLabel( this );
   m_pTimeL         = new QLabel( this );

   m_pIconL->setMinimumSize(70,0);
   m_pIconL->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::MinimumExpanding);
   QSpacerItem* verticalSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding);

   m_pMainLayout = new QGridLayout(this);
   m_pMainLayout->addWidget ( m_pIconL       , 0 , 0 , 4 , 1 );
   m_pMainLayout->addWidget ( m_pPeerNameL   , 0 , 1         );
   m_pMainLayout->addWidget ( m_pCallNumberL , 1 , 1         );
   m_pMainLayout->addWidget ( m_pTimeL       , 2 , 1         );
   m_pMainLayout->addItem   ( verticalSpacer , 4 , 1         );
   m_pMainLayout->addWidget ( m_pPlay        , 0 , 2 , 4 , 1 );
   m_pMainLayout->addWidget ( m_pRemove      , 0 , 3 , 4 , 1 );
   m_pMainLayout->addWidget ( m_pDurationL   , 0 , 4 , 4 , 1 );
   setLayout(m_pMainLayout);
   setMinimumSize(QSize(50, 30));

   if (!phone.isEmpty()) {
      getContactInfo(phone);
      m_pCallNumberL->setText(phone);
      m_PhoneNumber = phone;
   }
}

///Destructor
HistoryTreeItem::~HistoryTreeItem()
{

}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Return the call item
Call* HistoryTreeItem::call() const
{
   return m_pItemCall;
}

///The item have to be updated
void HistoryTreeItem::updated()
{
   if (!getContactInfo(m_pItemCall->getPeerPhoneNumber())) {
      if(! m_pItemCall->getPeerName().trimmed().isEmpty()) {
         m_pPeerNameL->setText("<b>"+m_pItemCall->getPeerName()+"</b>");
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
      m_pCallNumberL->setText(m_pItemCall->getPeerPhoneNumber());

      if(state == CALL_STATE_DIALING) {
         m_pCallNumberL->setText(m_pItemCall->getCallNumber());
      }
   }

}

///Show the context menu
void HistoryTreeItem::showContext(const QPoint& pos)
{
   if (!m_pMenu) {
      m_pMenu = new QMenu(this);
      m_pMenu->addAction( m_pCallAgain    );
      m_pMenu->addAction( m_pAddContact   );
      m_pMenu->addAction( m_pAddToContact );
      m_pMenu->addAction( m_pCopy         );
      m_pMenu->addAction( m_pEmail        );
      m_pMenu->addAction( m_pBookmark     );
   }
   m_pMenu->exec(mapToGlobal(pos));
}

///Send an email
void HistoryTreeItem::sendEmail()
{
   //TODO
   kDebug() << "Sending email";
}

///Call the caller again
void HistoryTreeItem::callAgain()
{
   if (m_pItemCall) {
      kDebug() << "Calling "<< m_pItemCall->getPeerPhoneNumber();
   }
   SFLPhone::model()->addDialingCall(m_Name, SFLPhone::app()->model()->getCurrentAccountId())->setCallNumber(m_PhoneNumber);
}

///Copy the call
void HistoryTreeItem::copy()
{
   //TODO
   kDebug() << "Copying contact";
}

///Create a contact from those informations
void HistoryTreeItem::addContact()
{
   kDebug() << "Adding contact";
   Contact* aContact = new Contact();
   aContact->setPhoneNumbers(PhoneNumbers() << new Contact::PhoneNumber(m_PhoneNumber, "Home"));
   aContact->setFormattedName(m_Name);
   AkonadiBackend::getInstance()->addNewContact(aContact);
}

///Add this call number to an existing contact
void HistoryTreeItem::addToContact()
{
   //TODO
   kDebug() << "Adding to contact";
}

///Bookmark this contact
void HistoryTreeItem::bookmark()
{
   SFLPhone::app()->bookmarkDock()->addBookmark(m_PhoneNumber);
}

void HistoryTreeItem::removeRecording()
{
   int ret = KMessageBox::questionYesNo(this, "Are you sure you want to delete this recording?", "Delete recording");
   if (ret == KMessageBox::Yes) {
      kDebug() << "Deleting file";
      QFile::remove(m_pItemCall->getRecordingPath());
   }
}

///Hide or show the media player
void HistoryTreeItem::showRecordPlayer()
{
   if (!m_pAudioSlider) {
      m_pPlayer      = new PlayerWidget       ( this      );
      QWidget* r1w   = new QWidget            ( this      );
      QWidget* r2w   = new QWidget            ( this      );
      QVBoxLayout* l = new QVBoxLayout        ( m_pPlayer );
      QHBoxLayout* r1= new QHBoxLayout        ( r1w       );
      QHBoxLayout* r2= new QHBoxLayout        ( r2w       );
      m_pAudioSlider = new Phonon::SeekSlider ( this      );
      m_pTimeLeftL   = new QLabel             ( "00:00"   );
      m_pTimePlayedL = new QLabel             ( "00:00"   );
      m_pPause       = new QToolButton        (           );
      m_pStop        = new QToolButton        (           );
      m_pNote        = new QToolButton        (           );
      
      l->addWidget(r1w);
      l->addWidget(r2w);
      
      m_pPlayer->setAttribute( Qt::WA_TranslucentBackground, true );
      m_pPlayer->setMinimumSize(0,25);
      m_pPlayer->setStyleSheet("margin-top:5px");

      l-> setContentsMargins(0,0,0,0);
      r1->setContentsMargins(0,0,0,0);
      r2->setContentsMargins(0,0,0,0);
      
      m_pPause->setIcon ( KIcon( "media-playback-pause" ));
      m_pStop->setIcon  ( KIcon( "media-playback-stop"  ));
      m_pNote->setIcon  ( KIcon( "view-pim-notes"       ));
      
      m_pPause->setMinimumSize(30,30);
      m_pStop->setMinimumSize (30,30);
      m_pNote->setMinimumSize (30,30);
      QSpacerItem* hSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
      
      r1->addWidget( m_pTimePlayedL );
      r1->addWidget( m_pAudioSlider );
      r1->addWidget( m_pTimeLeftL   );
      r2->addWidget( m_pPause       );
      r2->addWidget( m_pStop        );
      r2->addItem  ( hSpacer        );
      r2->addWidget( m_pNote        );
      
      m_pPlayer->setMinimumSize(width(),height());
      m_pPlayer->setMaximumSize(width(),height());
      
      l->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));
      m_pPlayer->setVisible(true);

      m_pAudioOutput             = new Phonon::AudioOutput(Phonon::MusicCategory, this);
      m_pMediaObject             = new Phonon::MediaObject(this);
      m_pMetaInformationResolver = new Phonon::MediaObject(this);
      Phonon::createPath(m_pMediaObject, m_pAudioOutput);

      m_pAudioSlider->setMediaObject(m_pMediaObject);
      
      m_pMediaObject->setTickInterval(1000);

      connect( m_pStop        , SIGNAL(clicked()    ) , this , SLOT( stopPlayer()      ));
      connect( m_pPause       , SIGNAL(clicked()    ) , this , SLOT( playPausePlayer() ));
      connect( m_pNote        , SIGNAL(clicked()    ) , this , SLOT( editNote()        ));
      connect( m_pMediaObject , SIGNAL(tick(qint64) ) , this , SLOT( tick(qint64)      ));
      
      connect(m_pMediaObject  , SIGNAL(stateChanged(Phonon::State,Phonon::State)),
              this, SLOT(stateChanged(Phonon::State,Phonon::State)));
      
   }
   qDebug() << "Path:" << m_pItemCall->getRecordingPath();
   m_pPlayer->setVisible(true);
   Phonon::MediaSource source(m_pItemCall->getRecordingPath());
   m_lSources.append(source);
   if (m_lSources.size() > 0)
      m_pMetaInformationResolver->setCurrentSource(m_lSources.first());
   m_pMediaObject->play();
   
}

///Called when the user press the stop button
void HistoryTreeItem::stopPlayer()
{
   m_pPlayer->setVisible(false);
   m_pMediaObject->stop();
}

///Called then the user press the Play/Pause button
void HistoryTreeItem::playPausePlayer()
{
   if (m_pMediaObject->state() == Phonon::PlayingState)
      m_pMediaObject->pause();
   else
      m_pMediaObject->play();
}

///Add or edit the note associated with this call
void HistoryTreeItem::editNote()
{
   
}

///Update player labels
void HistoryTreeItem::tick(qint64 time)
{
   QTime displayTime(0, (time / 60000) % 60, (time / 1000) % 60);
   m_pTimePlayedL->setText(displayTime.toString("mm:ss"));
}

///Called on player state change
void HistoryTreeItem::stateChanged(Phonon::State newState, Phonon::State /* oldState */)
{
   qDebug() << "Player state changed";
   switch (newState) {
      case Phonon::ErrorState:
            if (m_pMediaObject->errorType() == Phonon::FatalError) {
               QMessageBox::warning(this, tr("Fatal Error"),
               m_pMediaObject->errorString());
            } else {
               QMessageBox::warning(this, tr("Error"),
               m_pMediaObject->errorString());
            }
            break;
      case Phonon::PlayingState:
               m_pPause->setIcon(KIcon("media-playback-pause"));
               break;
      case Phonon::StoppedState:
               m_pPause->setIcon(KIcon("media-playback-play"));
               m_pTimePlayedL->setText("00:00");
               break;
      case Phonon::PausedState:
               m_pPause->setIcon(KIcon("media-playback-play"));
               break;
      case Phonon::BufferingState:
               break;
      default:
            ;
   }
}

///Reference code for metastate change
void HistoryTreeItem::metaStateChanged(Phonon::State newState, Phonon::State oldState)
{
   if (newState == Phonon::ErrorState) {
      QMessageBox::warning(this, tr("Error opening files"),
            m_pMetaInformationResolver->errorString());
      while (!m_lSources.isEmpty() &&
               !(m_lSources.takeLast() == m_pMetaInformationResolver->currentSource())) {}  /* loop */;
      return;
   }

   if (newState != Phonon::StoppedState && newState != Phonon::PausedState)
      return;

   if (m_pMetaInformationResolver->currentSource().type() == Phonon::MediaSource::Invalid)
            return;

   QMap<QString, QString> metaData = m_pMetaInformationResolver->metaData();

   m_pMediaObject->setCurrentSource(m_pMetaInformationResolver->currentSource());

   Phonon::MediaSource source = m_pMetaInformationResolver->currentSource();
   int index = m_lSources.indexOf(m_pMetaInformationResolver->currentSource()) + 1;
   if (m_lSources.size() > index) {
      m_pMetaInformationResolver->setCurrentSource(m_lSources.at(index));
   }
}

///Resize the player
void HistoryTreeItem::resizeEvent(QResizeEvent* event)
{
   if (m_pPlayer) {
      m_pPlayer->setMinimumSize(width(),height());
      m_pPlayer->setMaximumSize(width(),height());
   }
}

/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Set the call to be handled by this item
void HistoryTreeItem::setCall(Call *call)
{
   m_pItemCall = call;

   if (m_pItemCall->isConference()) {
      m_pIconL->setVisible(true);
      return;
   }

   m_pCallNumberL->setText(m_pItemCall->getPeerPhoneNumber());

   m_pTimeL->setText(QDateTime::fromTime_t(m_pItemCall->getStartTimeStamp().toUInt()).toString());

   int dur = m_pItemCall->getStopTimeStamp().toInt() - m_pItemCall->getStartTimeStamp().toInt();
   m_pDurationL->setText(QString("%1").arg(dur/3600,2).trimmed()+":"+QString("%1").arg((dur%3600)/60,2).trimmed()+":"+QString("%1").arg((dur%3600)%60,2).trimmed()+" ");

   connect(m_pItemCall , SIGNAL(changed())                          , this , SLOT(updated()           ));
   updated();

   m_TimeStamp   = m_pItemCall->getStartTimeStamp().toUInt();
   m_Duration    = dur;
   m_Name        = m_pItemCall->getPeerName();
   m_PhoneNumber = m_pItemCall->getPeerPhoneNumber();
   
   m_pPlay->  setVisible(!m_pItemCall->getRecordingPath().isEmpty() && QFile::exists(m_pItemCall->getRecordingPath()));
   m_pRemove->setVisible(!m_pItemCall->getRecordingPath().isEmpty() && QFile::exists(m_pItemCall->getRecordingPath()));
}

///Set the index associed with this widget
void HistoryTreeItem::setItem(QTreeWidgetItem* item)
{
   m_pItem = item;
}

void HistoryTreeItem::setDurWidth(uint width)
{
   m_pDurationL->setMaximumSize(width, 9999 );
   m_pDurationL->setMinimumSize(width, 0    );
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///Can a contact be associed with this call?
bool HistoryTreeItem::getContactInfo(QString phoneNumber)
{
   Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(phoneNumber,true);
   if (contact) {
      if (contact->getPhoto() != NULL)
         m_pIconL->setPixmap(*contact->getPhoto());
      m_pPeerNameL->setText("<b>"+contact->getFormattedName()+"</b>");
   }
   else {
      m_pIconL->setPixmap(QPixmap(KIcon("user-identity").pixmap(QSize(48,48))));
      m_pPeerNameL->setText(i18n("<b>Unknow</b>"));
      return false;
   }
   return true;
}

///Return the time stamp
uint HistoryTreeItem::getTimeStamp()
{
   return m_TimeStamp;
}

///Return the duration
uint HistoryTreeItem::getDuration()
{
   return m_Duration;
}

///Return the caller name
QString HistoryTreeItem::getName()
{
   return m_Name;
}

///Return the caller peer number
QString HistoryTreeItem::getPhoneNumber()
{
   return m_PhoneNumber;
}

///Get the index item assiciated with this widget
QTreeWidgetItem* HistoryTreeItem::getItem()
{
   return m_pItem;
}

///Get the width of the durationWidget
uint HistoryTreeItem::getDurWidth()
{
   QFontMetrics fm(m_pDurationL->font());
   return fm.width(m_pDurationL->text());
}