/***************************************************************************
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
 **************************************************************************/

#include "SFLPhoneView.h"

#include <QtGui/QLabel>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QBrush>
#include <QtGui/QPalette>
#include <QtGui/QInputDialog>

#include <klocale.h>
#include <kstandardaction.h>
#include <kactioncollection.h>
#include <kaction.h>
#include <kmenu.h>

#include <kabc/addressbook.h>
#include <kabc/stdaddressbook.h>
#include <kabc/addresseelist.h>

#include "lib/sflphone_const.h"
#include "conf/ConfigurationSkeleton.h"
#include "lib/configurationmanager_interface_singleton.h"
#include "lib/callmanager_interface_singleton.h"
#include "lib/instance_interface_singleton.h"
#include "ActionSetAccountFirst.h"
#include "widgets/ContactItemWidget.h"
#include "SFLPhone.h"
#include "lib/typedefs.h"
#include "widgets/Dialpad.h"
#include "widgets/CallTreeItem.h"


using namespace KABC;

//ConfigurationDialog* SFLPhoneView::configDialog;

SFLPhoneView::SFLPhoneView(QWidget *parent)
   : QWidget(parent),
     wizard(0)
{
   setupUi(this);
   
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   
   errorWindow = new QErrorMessage(this);
   callTreeModel->setTitle("Calls");
   
   QPalette pal = QPalette(palette());
   pal.setColor(QPalette::AlternateBase, Qt::lightGray);
   setPalette(pal);
   
   //BEGIN Port to CallModel
   connect(&callManager, SIGNAL(callStateChanged(const QString &, const QString &)),
           this,         SLOT(on1_callStateChanged(const QString &, const QString &)));
   connect(&callManager, SIGNAL(incomingCall(const QString &, const QString &, const QString &)),
           this,         SLOT(on1_incomingCall(const QString &, const QString &)));
   connect(&callManager, SIGNAL(conferenceCreated(const QString &)),
           this,         SLOT(on1_incomingConference(const QString &)));
   connect(&callManager, SIGNAL(conferenceChanged(const QString &, const QString &)),
           this,         SLOT(on1_changingConference(const QString &, const QString &)));
   connect(&callManager, SIGNAL(conferenceRemoved(const QString &)),
           this,         SLOT(on1_conferenceRemoved(const QString &)));
   connect(&callManager, SIGNAL(incomingMessage(const QString &, const QString &)),
           this,         SLOT(on1_incomingMessage(const QString &, const QString &)));
   connect(&callManager, SIGNAL(voiceMailNotify(const QString &, int)),
           this,         SLOT(on1_voiceMailNotify(const QString &, int)));

   connect(&callManager, SIGNAL(volumeChanged(const QString &, double)),
           this,         SLOT(on1_volumeChanged(const QString &, double)));
   
   connect(&configurationManager, SIGNAL(accountsChanged()),
           CallView::getAccountList(), SLOT(updateAccounts()));

   connect(&configurationManager, SIGNAL(audioManagerChanged()),
      this,         SLOT(on1_audioManagerChanged()));
   //END Port to Call Model
           
   //connect(configDialog, SIGNAL(changesApplied()),
           //this,         SLOT(loadWindow()));
           
   connect(CallView::getAccountList(), SIGNAL(accountListUpdated()),
           this,        SLOT(updateStatusMessage()));
   connect(CallView::getAccountList(), SIGNAL(accountListUpdated()),
           this,        SLOT(updateWindowCallState()));

   connect(callTreeModel->getWidget(),    SIGNAL(itemChanged()), //currentItemChanged
      this,        SLOT(on_callTree_currentItemChanged()));
   connect(callTreeModel->getWidget(),    SIGNAL(itemChanged()), //ITem changed
      this,        SLOT(on_callTree_itemChanged()));
   connect(callTreeModel->getWidget(),    SIGNAL(doubleClicked(const QModelIndex &)),
      this,        SLOT(on_callTree_itemDoubleClicked(const QModelIndex&)));
                
           
}



SFLPhoneView::~SFLPhoneView()
{
}

void SFLPhoneView::saveState()
{
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   configurationManager.setHistory(callTreeModel->getHistoryCallId());
}

void SFLPhoneView::loadWindow()
{
   updateWindowCallState();
   updateRecordButton();
   updateVolumeButton();
   updateRecordBar();
   updateVolumeBar();
   updateVolumeControls();
   updateDialpad();
   updateStatusMessage();
}

QErrorMessage * SFLPhoneView::getErrorWindow()
{
   return errorWindow;
}

CallView* SFLPhoneView::model()
{
   return callTreeModel;
}

void SFLPhoneView::typeString(QString str)
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   
   Call* call = callTreeModel->getCurrentItem();
   callManager.playDTMF(str);
   Call *currentCall = 0;
   Call *candidate = 0;

   if(call) {
      if(call->getState() == CALL_STATE_CURRENT) {
         currentCall = call;
      }
   }

   foreach (Call* call2, callTreeModel->getCallList()) {
      if(currentCall != call2 && call2->getState() == CALL_STATE_CURRENT) {
         action(call2, CALL_ACTION_HOLD);
      }
      else if(call2->getState() == CALL_STATE_DIALING) {
         candidate = call2;
      }
   }

   if(!currentCall && !candidate) {
      qDebug() << "Typing when no item is selected. Opening an item.";
      candidate = callTreeModel->addDialingCall();
   }

   if(!currentCall && candidate) {
      candidate->appendText(str);
   }
}

void SFLPhoneView::backspace()
{
   qDebug() << "backspace";
   qDebug() << "In call list.";
   Call* call = callTreeModel->getCurrentItem();
   if(!call) {
      qDebug() << "Error : Backspace on unexisting call.";
   }
   else {
      call->backspaceItemText();
      if(call->getState() == CALL_STATE_OVER) {
         if (callTreeModel->getCurrentItem())
            callTreeModel->removeItem(callTreeModel->getCurrentItem());

//             if(call->getHistoryState() != NONE) {
//                //historyTree->insert(call);
//                historyTreeModel->addCall(call);
//             }
      }
   }
}

void SFLPhoneView::escape()
{
   qDebug() << "escape";
   qDebug() << "In call list.";
   Call* call = callTreeModel->getCurrentItem();
   if(!call) {
      //qDebug() << "Escape when no item is selected. Doing nothing.";
   }
   else {
      if(call->getState() == CALL_STATE_TRANSFER || call->getState() == CALL_STATE_TRANSF_HOLD) {
         action(call, CALL_ACTION_TRANSFER);
      }
      else {
         action(call, CALL_ACTION_REFUSE);
      }
   }
}

void SFLPhoneView::enter()
{
   qDebug() << "enter";
   qDebug() << "In call list.";
   Call* call = callTreeModel->getCurrentItem();
   if(!call) {
      qDebug() << "Error : Enter on unexisting call.";
   }
   else {
      int state = call->getState();
      if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD) {
         action(call, CALL_ACTION_ACCEPT);
      }
      else {
         qDebug() << "Enter when call selected not in appropriate state. Doing nothing.";
      }
   }
}

void SFLPhoneView::action(Call* call, call_action action)
{
   if(! call) {
      qDebug() << "Error : action " << action << "applied on null object call. Should not happen.";
   }
   else {
      try {
         call->actionPerformed(action);
      }
      catch(const char * msg) {
         errorWindow->showMessage(QString(msg));
      }
      updateWindowCallState();
   }
}


/*******************************************
******** Update Display Functions **********
*******************************************/

void SFLPhoneView::updateWindowCallState()
{
   qDebug() << "updateWindowCallState";
   
   bool enabledActions[6]= {true,true,true,true,true,true};
   QString buttonIconFiles[6] = {ICON_CALL, ICON_HANGUP, ICON_HOLD, ICON_TRANSFER, ICON_REC_DEL_OFF, ICON_MAILBOX};
   QString actionTexts[6] = {ACTION_LABEL_CALL, ACTION_LABEL_HANG_UP, ACTION_LABEL_HOLD, ACTION_LABEL_TRANSFER, ACTION_LABEL_RECORD, ACTION_LABEL_MAILBOX};
   
   Call* call;
   
   bool transfer = false;
   bool recordActivated = false;    //tells whether the call is in recording position

   enabledActions[SFLPhone::Mailbox] = CallView::getCurrentAccount() && ! CallView::getCurrentAccount()->getAccountDetail(ACCOUNT_MAILBOX).isEmpty();

   call = callTreeModel->getCurrentItem();
   if (!call) {
      qDebug() << "No item selected.";
      enabledActions[SFLPhone::Refuse] = false;
      enabledActions[SFLPhone::Hold] = false;
      enabledActions[SFLPhone::Transfer] = false;
      enabledActions[SFLPhone::Record] = false;
   }
   else {
      call_state state = call->getState();
      recordActivated = call->getRecording();

      switch (state) {
         case CALL_STATE_INCOMING:
            qDebug() << "Reached CALL_STATE_INCOMING with call " << call->getCallId();
            buttonIconFiles[SFLPhone::Accept] = ICON_ACCEPT;
            buttonIconFiles[SFLPhone::Refuse] = ICON_REFUSE;
            actionTexts[SFLPhone::Accept] = ACTION_LABEL_ACCEPT;
            actionTexts[SFLPhone::Refuse] = ACTION_LABEL_REFUSE;
            break;
         case CALL_STATE_RINGING:
            qDebug() << "Reached CALL_STATE_RINGING with call " << call->getCallId();
            enabledActions[SFLPhone::Hold] = false;
            enabledActions[SFLPhone::Transfer] = false;
            break;
         case CALL_STATE_CURRENT:
            qDebug() << "details = " << CallManagerInterfaceSingleton::getInstance().getCallDetails(call->getCallId()).value();
            qDebug() << "Reached CALL_STATE_CURRENT with call " << call->getCallId();
            buttonIconFiles[SFLPhone::Record] = ICON_REC_DEL_ON;
            break;
         case CALL_STATE_DIALING:
            qDebug() << "Reached CALL_STATE_DIALING with call " << call->getCallId();
            enabledActions[SFLPhone::Hold] = false;
            enabledActions[SFLPhone::Transfer] = false;
            enabledActions[SFLPhone::Record] = false;
            actionTexts[SFLPhone::Accept] = ACTION_LABEL_ACCEPT;
            buttonIconFiles[SFLPhone::Accept] = ICON_ACCEPT;
            break;
         case CALL_STATE_HOLD:
            qDebug() << "Reached CALL_STATE_HOLD with call " << call->getCallId();
            buttonIconFiles[SFLPhone::Hold] = ICON_UNHOLD;
            actionTexts[SFLPhone::Hold] = ACTION_LABEL_UNHOLD;
            break;
         case CALL_STATE_FAILURE:
            qDebug() << "Reached CALL_STATE_FAILURE with call " << call->getCallId();
            enabledActions[SFLPhone::Accept] = false;
            enabledActions[SFLPhone::Hold] = false;
            enabledActions[SFLPhone::Transfer] = false;
            enabledActions[SFLPhone::Record] = false;
            break;
         case CALL_STATE_BUSY:
            qDebug() << "Reached CALL_STATE_BUSY with call " << call->getCallId();
            enabledActions[SFLPhone::Accept] = false;
            enabledActions[SFLPhone::Hold] = false;
            enabledActions[SFLPhone::Transfer] = false;
            enabledActions[SFLPhone::Record] = false;
            break;
         case CALL_STATE_TRANSFER:
            qDebug() << "Reached CALL_STATE_TRANSFER with call " << call->getCallId();
            buttonIconFiles[SFLPhone::Accept] = ICON_EXEC_TRANSF;
            actionTexts[SFLPhone::Transfer] = ACTION_LABEL_GIVE_UP_TRANSF;
            transfer = true;
            buttonIconFiles[SFLPhone::Record] = ICON_REC_DEL_ON;
            break;
         case CALL_STATE_TRANSF_HOLD:
            qDebug() << "Reached CALL_STATE_TRANSF_HOLD with call " << call->getCallId();
            buttonIconFiles[SFLPhone::Accept] = ICON_EXEC_TRANSF;
            buttonIconFiles[SFLPhone::Hold] = ICON_UNHOLD;
            actionTexts[SFLPhone::Transfer] = ACTION_LABEL_GIVE_UP_TRANSF;
            actionTexts[SFLPhone::Hold] = ACTION_LABEL_UNHOLD;
            transfer = true;
            break;
         case CALL_STATE_OVER:
            qDebug() << "Error : Reached CALL_STATE_OVER with call " << call->getCallId() << "!";
            break;
         case CALL_STATE_ERROR:
            qDebug() << "Error : Reached CALL_STATE_ERROR with call " << call->getCallId() << "!";
            break;
         default:
            qDebug() << "Error : Reached unexisting state for call " << call->getCallId() << "!";
            break;
      }
   }
   
   qDebug() << "Updating Window.";
   
   emit enabledActionsChangeAsked(enabledActions);
   emit actionIconsChangeAsked(buttonIconFiles);
   emit actionTextsChangeAsked(actionTexts);
   emit transferCheckStateChangeAsked(transfer);
   emit recordCheckStateChangeAsked(recordActivated);

   qDebug() << "Window updated.";
}

void SFLPhoneView::alternateColors(QListWidget * listWidget)
{
   for(int i = 0 ; i < listWidget->count(); i++) {
      QListWidgetItem* item = listWidget->item(i);
      QBrush c = (i % 2 == 1) ? palette().base() : palette().alternateBase();
      item->setBackground( c );
   }
   listWidget->setUpdatesEnabled( true );

}

int SFLPhoneView::phoneNumberTypesDisplayed()
{
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   MapStringInt addressBookSettings = configurationManager.getAddressbookSettings().value();
   int typesDisplayed = 0;
   if(addressBookSettings[ADDRESSBOOK_DISPLAY_BUSINESS]) {
      typesDisplayed = typesDisplayed | PhoneNumber::Work;
   }
   
   if(addressBookSettings[ADDRESSBOOK_DISPLAY_MOBILE]) {
      typesDisplayed = typesDisplayed | PhoneNumber::Cell;
   }
   
   if(addressBookSettings[ADDRESSBOOK_DISPLAY_HOME]) {
      typesDisplayed = typesDisplayed | PhoneNumber::Home;
   }
   
   return typesDisplayed;
}

void SFLPhoneView::updateRecordButton()
{
   qDebug() << "updateRecordButton";
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   double recVol = callManager.getVolume(RECORD_DEVICE);
   if(recVol == 0.00) {
      toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_0));
   }
   else if(recVol < 0.33) {
      toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_1));
   }
   else if(recVol < 0.67) {
      toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_2));
   }
   else {
      toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_3));
   }
   
   if(recVol > 0) {   
      toolButton_recVol->setChecked(false);
   }
}
void SFLPhoneView::updateVolumeButton()
{
   qDebug() << "updateVolumeButton";
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   double sndVol = callManager.getVolume(SOUND_DEVICE);
        
   if(sndVol == 0.00) {
      toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_0));
   }
   else if(sndVol < 0.33) {
      toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_1));
   }
   else if(sndVol < 0.67) {
      toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_2));
   }
   else {
      toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_3));
   }
   
   if(sndVol > 0) {
      toolButton_sndVol->setChecked(false);
   }
}


void SFLPhoneView::updateRecordBar()
{
   qDebug() << "updateRecordBar";
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   double recVol = callManager.getVolume(RECORD_DEVICE);
   int value = (int)(recVol * 100);
   slider_recVol->setValue(value);
}
void SFLPhoneView::updateVolumeBar()
{
   qDebug() << "updateVolumeBar";
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   double sndVol = callManager.getVolume(SOUND_DEVICE);
   int value = (int)(sndVol * 100);
   slider_sndVol->setValue(value);
}

void SFLPhoneView::updateVolumeControls()
{
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   int display = false;

   if(QString(configurationManager.getAudioManager()) == "alsa") {
      display = true;

      SFLPhone::app()->action_displayVolumeControls->setEnabled(true);
   }
   else {
      SFLPhone::app()->action_displayVolumeControls->setEnabled(false);
   }
      
   SFLPhone::app()->action_displayVolumeControls->setChecked(display);
   //widget_recVol->setVisible(display);
   //widget_sndVol->setVisible(display);
   toolButton_recVol->setVisible(display && ConfigurationSkeleton::displayVolume());
   toolButton_sndVol->setVisible(display && ConfigurationSkeleton::displayVolume());
   slider_recVol->setVisible(display && ConfigurationSkeleton::displayVolume());
   slider_sndVol->setVisible(display && ConfigurationSkeleton::displayVolume());
   
}

void SFLPhoneView::updateDialpad()
{
   widget_dialpad->setVisible(ConfigurationSkeleton::displayDialpad());//TODO use display variable
}


void SFLPhoneView::updateStatusMessage()
{
   qDebug() << "updateStatusMessage";
   Account * account = CallView::getCurrentAccount();

   if(account == NULL) {
      emit statusMessageChangeAsked(i18n("No registered accounts"));
   }
   else {
      emit statusMessageChangeAsked(i18n("Using account") 
                     + " \'" + account->getAlias() 
                     + "\' (" + account->getAccountDetail(ACCOUNT_TYPE) + ")") ;
   }
}



/************************************************************
************            Autoconnect             *************
************************************************************/

void SFLPhoneView::displayVolumeControls(bool checked)
{
   //ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   ConfigurationSkeleton::setDisplayVolume(checked);
   updateVolumeControls();
}

void SFLPhoneView::displayDialpad(bool checked)
{
   ConfigurationSkeleton::setDisplayDialpad(checked);
   updateDialpad();
}


void SFLPhoneView::on_widget_dialpad_typed(QString text)      
{ 
   typeString(text); 
}

void SFLPhoneView::on_slider_recVol_valueChanged(int value)
{
   qDebug() << "on_slider_recVol_valueChanged(" << value << ")";
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.setVolume(RECORD_DEVICE, (double)value / 100.0);
   updateRecordButton();
}

void SFLPhoneView::on_slider_sndVol_valueChanged(int value)
{
   qDebug() << "on_slider_sndVol_valueChanged(" << value << ")";
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.setVolume(SOUND_DEVICE, (double)value / 100.0);
   updateVolumeButton();
}

void SFLPhoneView::on_toolButton_recVol_clicked(bool checked)
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   qDebug() << "on_toolButton_recVol_clicked().";
   if(!checked) {
      qDebug() << "checked";
      toolButton_recVol->setChecked(false);
      slider_recVol->setEnabled(true);
      callManager.setVolume(RECORD_DEVICE, (double)slider_recVol->value() / 100.0);
   }
   else {
      qDebug() << "unchecked";
      toolButton_recVol->setChecked(true);
      slider_recVol->setEnabled(false);
      callManager.setVolume(RECORD_DEVICE, 0.0);
   }
   updateRecordButton();
}

void SFLPhoneView::on_toolButton_sndVol_clicked(bool checked)
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   qDebug() << "on_toolButton_sndVol_clicked().";
   if(!checked) {
      qDebug() << "checked";
      toolButton_sndVol->setChecked(false);
      slider_sndVol->setEnabled(true);
      callManager.setVolume(SOUND_DEVICE, (double)slider_sndVol->value() / 100.0);
   }
   else {
      qDebug() << "unchecked";
      toolButton_sndVol->setChecked(true);
      slider_sndVol->setEnabled(false);
      callManager.setVolume(SOUND_DEVICE, 0.0);
   }
   
   updateVolumeButton();
}

void SFLPhoneView::on_callTree_currentItemChanged()
{
   qDebug() << "on_callTree_currentItemChanged";
   updateWindowCallState();
}

void SFLPhoneView::on_callTree_itemChanged()
{
   qDebug() << "on_callTree_itemChanged";
}

void SFLPhoneView::on_callTree_itemDoubleClicked(QTreeWidgetItem* call, int column)
{
Q_UNUSED(call)
Q_UNUSED(column)
   //TODO port
   //TODO remove once the last regression is sorted out.
//    qDebug() << "on_callTree_itemDoubleClicked";
//    call_state state = call->getCurrentState();
//    switch(state) {
//       case CALL_STATE_HOLD:
//          action(call, CALL_ACTION_HOLD);
//          break;
//       case CALL_STATE_DIALING:
//          action(call, CALL_ACTION_ACCEPT);
//          break;
//       default:
//          qDebug() << "Double clicked an item with no action on double click.";
//    }
}

void SFLPhoneView::contextMenuEvent(QContextMenuEvent *event)
{
   KMenu menu(this);
   
   SFLPhone * window = SFLPhone::app();
   QList<QAction *> callActions = window->getCallActions();
   menu.addAction(callActions.at((int) SFLPhone::Accept));
   menu.addAction(callActions[SFLPhone::Refuse]);
   menu.addAction(callActions[SFLPhone::Hold]);
   menu.addAction(callActions[SFLPhone::Transfer]);
   menu.addAction(callActions[SFLPhone::Record]);
   menu.addSeparator();
   
   QAction * action = new ActionSetAccountFirst(NULL, &menu);
   action->setChecked(CallView::getPriorAccoundId().isEmpty());
   connect(action,  SIGNAL(setFirst(Account *)),
           this  ,  SLOT(setAccountFirst(Account *)));
   menu.addAction(action);
   
   QVector<Account *> accounts = CallView::getAccountList()->registeredAccounts();
   for (int i = 0 ; i < accounts.size() ; i++) {
      Account * account = accounts.at(i);
      QAction * action = new ActionSetAccountFirst(account, &menu);
      action->setChecked(account->getAccountId() == CallView::getPriorAccoundId());
      connect(action, SIGNAL(setFirst(Account *)),
              this  , SLOT(setAccountFirst(Account *)));
      menu.addAction(action);
   }
   menu.exec(event->globalPos());
}

void SFLPhoneView::editBeforeCall()
{
   qDebug() << "editBeforeCall";
   QString name;
   QString number;
        
   bool ok;
   QString newNumber = QInputDialog::getText(this, i18n("Edit before call"), QString(), QLineEdit::Normal, number, &ok);
   if(ok) {
      changeScreen(SCREEN_MAIN);
      Call* call = callTreeModel->addDialingCall(name);
      call->appendText(newNumber);
      //callTreeModel->selectItem(addCallToCallList(call));
      action(call, CALL_ACTION_ACCEPT);
   }
}

void SFLPhoneView::setAccountFirst(Account * account)
{
   qDebug() << "setAccountFirst : " << (account ? account->getAlias() : QString()) << (account ? account->getAccountId() : QString());
   if(account) {
      CallView::setPriorAccountId(account->getAccountId());
   }
   else {
      CallView::setPriorAccountId(QString());
   }
   qDebug() << "Current account id" << CallView::getCurrentAccountId();
   updateStatusMessage();
}

void SFLPhoneView::configureSflPhone()
{
   ConfigurationDialog* configDialog = new ConfigurationDialog(this);
   configDialog->setModal(true);
   
   connect(configDialog, SIGNAL(changesApplied()),
           this,         SLOT(loadWindow()));
           
   //configDialog->reload();
   configDialog->show();
}

void SFLPhoneView::accountCreationWizard()
{
   if (!wizard) {
      wizard = new AccountWizard(this);
      wizard->setModal(false);
   }
   wizard->show();
}
   

void SFLPhoneView::accept()
{
   Call* call = callTreeModel->getCurrentItem();
   if(!call) {
      qDebug() << "Calling when no item is selected. Opening an item.";
      callTreeModel->addDialingCall();
   }
   else {
      int state = call->getState();
      if(state == CALL_STATE_RINGING || state == CALL_STATE_CURRENT || state == CALL_STATE_HOLD || state == CALL_STATE_BUSY)
      {
         qDebug() << "Calling when item currently ringing, current, hold or busy. Opening an item.";
         callTreeModel->addDialingCall();
      }
      else {
         action(call, CALL_ACTION_ACCEPT);
      }
   }
}

void SFLPhoneView::refuse()
{
   Call* call = callTreeModel->getCurrentItem();
   if(!call) {
      qDebug() << "Error : Hanging up when no item selected. Should not happen.";
   }
   else {
      action(call, CALL_ACTION_REFUSE);
   }
}

void SFLPhoneView::hold()
{
   Call* call = callTreeModel->getCurrentItem();
   if(!call) {
      qDebug() << "Error : Holding when no item selected. Should not happen.";
   }
   else {
      action(call, CALL_ACTION_HOLD);
   }
}

void SFLPhoneView::transfer()
{
   Call* call = callTreeModel->getCurrentItem();
   if(!call) {
      qDebug() << "Error : Transfering when no item selected. Should not happen.";
   }
   else {
      action(call, CALL_ACTION_TRANSFER);
   }
}

void SFLPhoneView::record()
{
   Call* call = callTreeModel->getCurrentItem();
   if(!call) {
      qDebug() << "Error : Recording when no item selected. Should not happen.";
   }
   else {
      action(call, CALL_ACTION_RECORD);
   }
}

void SFLPhoneView::mailBox()
{
   Account * account = CallView::getCurrentAccount();
   QString mailBoxNumber = account->getAccountDetail(ACCOUNT_MAILBOX);
   Call * call = callTreeModel->addDialingCall();
   call->appendText(mailBoxNumber);
   action(call, CALL_ACTION_ACCEPT);
}

void SFLPhoneView::on1_callStateChanged(const QString &callID, const QString &state)
{
   //This code is part of the CallModel iterface too
   qDebug() << "Signal : Call State Changed for call  " << callID << " . New state : " << state;
   Call* call = callTreeModel->findCallByCallId(callID);
   if(!call) {
      if(state == CALL_STATE_CHANGE_RINGING) {
         call = callTreeModel->addRingingCall(callID);
      }
      else {
         qDebug() << "Call doesn't exist in this client. Might have been initialized by another client instance before this one started.";
         return;
      }
   }
   else {
      call->stateChanged(state);
   }
   updateWindowCallState(); //NEED_PORT
}

void SFLPhoneView::on1_error(MapStringString details)
{
   qDebug() << "Signal : Daemon error : " << details;
}

void SFLPhoneView::on1_incomingCall(const QString & /*accountID*/, const QString & callID)
{
   qDebug() << "Signal : Incoming Call ! ID = " << callID;
   Call* call = callTreeModel->addIncomingCall(callID);

   
   //NEED_PORT
   changeScreen(SCREEN_MAIN);

   SFLPhone::app()->activateWindow();
   SFLPhone::app()->raise();
   SFLPhone::app()->setVisible(true);

   emit incomingCall(call);
}

void SFLPhoneView::on1_incomingConference(const QString &confID) {
   callTreeModel->conferenceCreatedSignal(confID);
}

void SFLPhoneView::on1_changingConference(const QString &confID, const QString &state) {
   callTreeModel->conferenceChangedSignal(confID, state);
}

void SFLPhoneView::on1_conferenceRemoved(const QString &confId) {
   callTreeModel->conferenceRemovedSignal(confId);
}

void SFLPhoneView::on1_incomingMessage(const QString &accountID, const QString &message)
{
   qDebug() << "Signal : Incoming Message for account " << accountID << " ! \nMessage : " << message;
}

void SFLPhoneView::on1_voiceMailNotify(const QString &accountID, int count)
{
   qDebug() << "Signal : VoiceMail Notify ! " << count << " new voice mails for account " << accountID;
}

void SFLPhoneView::on1_volumeChanged(const QString & /*device*/, double value)
{
   qDebug() << "Signal : Volume Changed !";
   if(! (toolButton_recVol->isChecked() && value == 0.0))
      updateRecordBar();
   if(! (toolButton_sndVol->isChecked() && value == 0.0))
      updateVolumeBar();
}

void SFLPhoneView::on1_audioManagerChanged()
{
   qDebug() << "Signal : Audio Manager Changed !";

   updateVolumeControls();
}

void SFLPhoneView::changeScreen(int screen)
{
   qDebug() << "changeScreen";
   updateWindowCallState();
   emit screenChanged(screen);
}

#include "SFLPhoneView.moc"
