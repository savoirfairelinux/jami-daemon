/************************************** *************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
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
#include <unistd.h>
#include "SFLPhone.h"

#include <unistd.h>
#include <KApplication>
#include <KStandardAction>
#include <KMenu>
#include <KAction>
#include <KToolBar>
#include <KStatusBar>
#include <QtGui/QStatusBar>
#include <QtGui/QCursor>
#include <KActionCollection>

#include "lib/sflphone_const.h"
#include "lib/instance_interface_singleton.h"
#include "lib/configurationmanager_interface_singleton.h"
#include "lib/Contact.h"
#include "AkonadiBackend.h"
#include "conf/ConfigurationSkeleton.h"

SFLPhone* SFLPhone::m_sApp = NULL;
TreeWidgetCallModel* SFLPhone::m_pModel = NULL;

SFLPhone::SFLPhone(QWidget *parent)
    : KXmlGuiWindow(parent),
      initialized_(false),
      m_pView(new SFLPhoneView(this))
{
    setupActions();
    m_sApp = this;
}

SFLPhone* SFLPhone::app()
{
   return m_sApp;
}

SFLPhoneView* SFLPhone::view()
{
   return m_pView;
}
TreeWidgetCallModel* SFLPhone::model()
{
   if (!m_pModel) {
      m_pModel = new TreeWidgetCallModel(TreeWidgetCallModel::ActiveCall);
      m_pModel->initCall();
    }
   return m_pModel;
}

ContactDock*  SFLPhone::contactDock()
{
   return m_pContactCD;
}

HistoryDock*  SFLPhone::historyDock()
{
   return m_pHistoryDW;
}

BookmarkDock* SFLPhone::bookmarkDock()
{
   return m_pBookmarkDW;
}

SFLPhone::~SFLPhone()
{
   saveState();
}

bool SFLPhone::initialize()
{
  if ( initialized_ ) {
    qDebug() << "Already initialized.";
    return false;
  }

   ConfigurationSkeleton::self();

   //Keep these template paramater or the static attribute wont be share between this and the call view, they need to be
   CallModel<CallTreeItem*,QTreeWidgetItem*>* histoModel = new CallModel<CallTreeItem*,QTreeWidgetItem*>(CallModel<CallTreeItem*,QTreeWidgetItem*>::History);
   histoModel->initHistory();
   
  ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
  // accept dnd
  setAcceptDrops(true);

   m_pContactCD = new ContactDock(this);
   addDockWidget(Qt::TopDockWidgetArea,m_pContactCD);
   
  // tell the KXmlGuiWindow that this is indeed the main widget
  //setCentralWidget(m_pView);
  m_pCentralDW = new QDockWidget(this);
  m_pCentralDW->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
  m_pCentralDW->setWidget(m_pView);
  m_pCentralDW->setWindowTitle("Call");
  m_pCentralDW->setFeatures(QDockWidget::NoDockWidgetFeatures);
  m_pView->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
  m_pCentralDW->setStyleSheet("\
      QDockWidget::title {\
         margin:0px;\
         padding:0px;\
         spacing:0px;\
         max-height:0px;\
      }\
      \
  ");
  
  m_pCentralDW->setTitleBarWidget(new QWidget());
  m_pCentralDW->setContentsMargins(0,0,0,0);
  m_pView->setContentsMargins(0,0,0,0);
  
  addDockWidget(Qt::TopDockWidgetArea,m_pCentralDW);

   
   m_pHistoryDW  = new HistoryDock(this);
   addDockWidget(Qt::TopDockWidgetArea,m_pHistoryDW);
   m_pBookmarkDW = new BookmarkDock(this);
   addDockWidget(Qt::TopDockWidgetArea,m_pBookmarkDW);
   tabifyDockWidget(m_pBookmarkDW,m_pHistoryDW);

  setWindowIcon(QIcon(ICON_SFLPHONE));
  setWindowTitle(i18n("SFLphone"));

  setupActions();

  statusBarWidget = new QLabel();
  statusBar()->addWidget(statusBarWidget);


  trayIcon = new SFLPhoneTray(this->windowIcon(), this);
  trayIcon->show();

  iconChanged = false;

  setObjectNames();
  QMetaObject::connectSlotsByName(this);
  m_pView->loadWindow();

  move(QCursor::pos().x() - geometry().width()/2, QCursor::pos().y() - geometry().height()/2);
  //if( ! configurationManager.isStartHidden()) {
  show();
  //}

  if(configurationManager.getAccountList().value().isEmpty()) {
      (new AccountWizard())->show();
  }

  initialized_ = true;

  return true;
}

void SFLPhone::setObjectNames()
{
   m_pView->setObjectName("m_pView");
   statusBar()->setObjectName("statusBar");
   trayIcon->setObjectName("trayIcon");
}

void SFLPhone::setupActions()
{
   qDebug() << "setupActions";
   //ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   
   action_accept   = new KAction(this);
   action_refuse   = new KAction(this);
   action_hold     = new KAction(this);
   action_transfer = new KAction(this);
   action_record   = new KAction(this);
   action_mailBox  = new KAction(this);

   action_accept->setShortcut   ( Qt::CTRL + Qt::Key_A );
   action_refuse->setShortcut   ( Qt::CTRL + Qt::Key_D );
   action_hold->setShortcut     ( Qt::CTRL + Qt::Key_H );
   action_transfer->setShortcut ( Qt::CTRL + Qt::Key_T );
   action_record->setShortcut   ( Qt::CTRL + Qt::Key_R );
   action_mailBox->setShortcut  ( Qt::CTRL + Qt::Key_M );

   action_screen = new QActionGroup(this);
   action_screen->setExclusive(true);
   action_main = new KAction(KIcon(QIcon(ICON_SCREEN_MAIN)), i18n("Main screen"), action_screen);
   action_main->setCheckable(true);
   action_main->setChecked(true);
   action_screen->addAction(action_main);

   action_close = KStandardAction::close(this, SLOT(close()), this);
   action_quit = KStandardAction::quit(this, SLOT(quitButton()), this);
   
   action_configureSflPhone = KStandardAction::preferences(m_pView, SLOT(configureSflPhone()), this);
   action_configureSflPhone->setText(i18n("Configure SFLphone"));
   
   action_displayVolumeControls = new KAction(KIcon(QIcon(ICON_DISPLAY_VOLUME_CONSTROLS)), i18n("Display volume controls"), this);   
   action_displayDialpad = new KAction(KIcon(QIcon(ICON_DISPLAY_DIALPAD)), i18n("Display dialpad"), this);
   action_displayDialpad->setCheckable(true);

   action_displayVolumeControls->setCheckable(true);
   action_displayVolumeControls->setChecked(ConfigurationSkeleton::displayVolume());
   action_displayDialpad->setChecked(ConfigurationSkeleton::displayDialpad());
   action_accountCreationWizard = new KAction(i18n("Account creation wizard"), this);
   
   connect(action_accept,                SIGNAL(triggered()),           m_pView , SLOT(accept()                    ));
   connect(action_refuse,                SIGNAL(triggered()),           m_pView , SLOT(refuse()                    ));
   connect(action_hold,                  SIGNAL(triggered()),           m_pView , SLOT(hold()                      ));
   connect(action_transfer,              SIGNAL(triggered()),           m_pView , SLOT(transfer()                  ));
   connect(action_record,                SIGNAL(triggered()),           m_pView , SLOT(record()                    ));
   connect(action_screen,                SIGNAL(triggered(QAction *)),  this    , SLOT(updateScreen(QAction *)     ));
   connect(action_mailBox,               SIGNAL(triggered()),           m_pView , SLOT(mailBox()                   ));
   connect(action_displayVolumeControls, SIGNAL(toggled(bool)),         m_pView , SLOT(displayVolumeControls(bool) ));
   connect(action_displayDialpad,        SIGNAL(toggled(bool)),         m_pView , SLOT(displayDialpad(bool)        ));
   connect(action_accountCreationWizard, SIGNAL(triggered()),           m_pView , SLOT(accountCreationWizard()     ));

   action_screen->addAction(action_main);
   
   actionCollection()->addAction("action_accept"                , action_accept                );
   actionCollection()->addAction("action_refuse"                , action_refuse                );
   actionCollection()->addAction("action_hold"                  , action_hold                  );
   actionCollection()->addAction("action_transfer"              , action_transfer              );
   actionCollection()->addAction("action_record"                , action_record                );
   actionCollection()->addAction("action_main"                  , action_main                  );
   actionCollection()->addAction("action_mailBox"               , action_mailBox               );
   actionCollection()->addAction("action_close"                 , action_close                 );
   actionCollection()->addAction("action_quit"                  , action_quit                  );
   actionCollection()->addAction("action_displayVolumeControls" , action_displayVolumeControls );
   actionCollection()->addAction("action_displayDialpad"        , action_displayDialpad        );
   actionCollection()->addAction("action_configureSflPhone"     , action_configureSflPhone     );
   actionCollection()->addAction("action_accountCreationWizard" , action_accountCreationWizard );
   
   QString rcFilePath = QString(DATA_INSTALL_DIR) + "/sflphone-client-kde/sflphone-client-kdeui.rc";

   if(! QFile::exists(rcFilePath)) {
      QDir dir;
      dir.cdUp();
      dir.cd("data");
      rcFilePath = dir.filePath("sflphone-client-kdeui.rc");
      qDebug() << "rcFilePath = " << rcFilePath ;

      if(! QFile::exists(rcFilePath)) {
         QDir dir;
         dir.cdUp();
         dir.cdUp();
         dir.cd("data");
         rcFilePath = dir.filePath("sflphone-client-kdeui.rc");
      }
   }
   qDebug() << "rcFilePath = " << rcFilePath ;
   createGUI(rcFilePath);

}

SFLPhoneView * SFLPhone::getView()
{
   return m_pView;
}

bool SFLPhone::queryClose()
{
   qDebug() << "queryClose";
   hide();
   return false;
}

void SFLPhone::quitButton()
{
   
   //qDebug() << "quitButton : " << m_pView->callTree->count() << " calls open.";

   //if(m_pView->callTree->count() > 0 && instance.getRegistrationCount() <= 1) {
      //qDebug() << "Attempting to quit when still having some calls open.";
   //}
   m_pView->saveState();
   qApp->quit();
}

void SFLPhone::changeEvent(QEvent* event)
{
   if (event->type() == QEvent::ActivationChange && iconChanged && isActiveWindow()) {
     iconChanged = false;
   }
}

void SFLPhone::on_m_pView_statusMessageChangeAsked(const QString & message)
{
   qDebug() << "on_m_pView_statusMessageChangeAsked : " + message;
   statusBarWidget->setText(message);
}

void SFLPhone::on_m_pView_windowTitleChangeAsked(const QString & message)
{
   qDebug() << "on_m_pView_windowTitleChangeAsked : " + message;
   setWindowTitle(message);
}

void SFLPhone::on_m_pView_enabledActionsChangeAsked(const bool * enabledActions)
{
   qDebug() << "on_m_pView_enabledActionsChangeAsked";
   action_accept->setVisible   ( enabledActions[SFLPhone::Accept   ]);
   action_refuse->setVisible   ( enabledActions[SFLPhone::Refuse   ]);
   action_hold->setVisible     ( enabledActions[SFLPhone::Hold     ]);
   action_transfer->setVisible ( enabledActions[SFLPhone::Transfer ]);
   action_record->setVisible   ( enabledActions[SFLPhone::Record   ]);
   action_mailBox->setVisible  ( enabledActions[SFLPhone::Mailbox  ]);
}

void SFLPhone::on_m_pView_actionIconsChangeAsked(const QString * actionIcons)
{
   qDebug() << "on_m_pView_actionIconsChangeAsked";
   action_accept->setIcon   ( QIcon(actionIcons[SFLPhone::Accept   ]));
   action_refuse->setIcon   ( QIcon(actionIcons[SFLPhone::Refuse   ]));
   action_hold->setIcon     ( QIcon(actionIcons[SFLPhone::Hold     ]));
   action_transfer->setIcon ( QIcon(actionIcons[SFLPhone::Transfer ]));
   action_record->setIcon   ( QIcon(actionIcons[SFLPhone::Record   ]));
   action_mailBox->setIcon  ( QIcon(actionIcons[SFLPhone::Mailbox  ]));
}

void SFLPhone::on_m_pView_actionTextsChangeAsked(const QString * actionTexts)
{
   qDebug() << "on_m_pView_actionTextsChangeAsked";
   action_accept->setText   ( actionTexts[SFLPhone::Accept   ]);
   action_refuse->setText   ( actionTexts[SFLPhone::Refuse   ]);
   action_hold->setText     ( actionTexts[SFLPhone::Hold     ]);
   action_transfer->setText ( actionTexts[SFLPhone::Transfer ]);
   action_record->setText   ( actionTexts[SFLPhone::Record   ]);
   action_mailBox->setText  ( actionTexts[SFLPhone::Mailbox  ]);
}


void SFLPhone::on_m_pView_transferCheckStateChangeAsked(bool transferCheckState)
{
   qDebug() << "Changing transfer action checkState";
   action_transfer->setChecked(transferCheckState);
}

void SFLPhone::on_m_pView_recordCheckStateChangeAsked(bool recordCheckState)
{
   qDebug() << "Changing record action checkState";
   action_record->setChecked(recordCheckState);
}

void SFLPhone::updateScreen(QAction * action)
{
   if(action == action_main)   m_pView->changeScreen(SCREEN_MAIN);
}

void SFLPhone::on_m_pView_screenChanged(int screen)
{
   qDebug() << "on_m_pView_screenChanged";
   if(screen == SCREEN_MAIN)   action_main->setChecked(true);
}

QList<QAction*> SFLPhone::getCallActions()
{
   QList<QAction*> callActions = QList<QAction *>();
   callActions.insert((int) Accept   , action_accept   );
   callActions.insert((int) Refuse   , action_refuse   );
   callActions.insert((int) Hold     , action_hold     );
   callActions.insert((int) Transfer , action_transfer );
   callActions.insert((int) Record   , action_record   );
   callActions.insert((int) Mailbox  , action_mailBox  );
   return callActions;
}

void SFLPhone::on_m_pView_incomingCall(const Call * call)
{
   //ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
    //trayIconSignal();
    /*if(configurationManager.popupMode())
   {
      putForeground();
    }*/
   //if(configurationManager.getNotify()) {
   Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(call->getPeerPhoneNumber());
   if (contact) {
      KNotification::event(KNotification::Notification, "New incomming call", "New call from: \n" + call->getPeerName().isEmpty() ? call->getPeerPhoneNumber() : call->getPeerName(),*contact->getPhoto());
   }
   KNotification::event(KNotification::Notification, "New incomming call", "New call from: \n" + call->getPeerName().isEmpty() ? call->getPeerPhoneNumber() : call->getPeerName());
   //}
}
