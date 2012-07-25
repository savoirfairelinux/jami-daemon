/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
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
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

//Parent
#include "SFLPhoneView.h"

//Qt
#include <QtCore/QString>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QPalette>
#include <QtGui/QInputDialog>
#include <QtGui/QWidget>
#include <QtGui/QClipboard>
#include <QErrorMessage>

//KDE
#include <KLocale>
#include <KAction>
#include <KMenu>
#include <kabc/addressbook.h>

//SFLPhone
#include "conf/ConfigurationDialog.h"
#include "klib/ConfigurationSkeleton.h"
#include "AccountWizard.h"
#include "ActionSetAccountFirst.h"
#include "SFLPhone.h"

//SFLPhone library
#include "lib/typedefs.h"
#include "lib/configurationmanager_interface_singleton.h"
#include "lib/callmanager_interface_singleton.h"
#include "lib/instance_interface_singleton.h"
#include "lib/sflphone_const.h"
#include "lib/Contact.h"
#include "klib/HelperFunctions.h"

#define IM_ACTIVE m_pMessageTabBox->isVisible()

//ConfigurationDialog* SFLPhoneView::configDialog;

class ColorVisitor : public AccountListColorVisitor {
public:
   ColorVisitor(QPalette pal) : m_Pal(pal) {
      m_Green = QColor(m_Pal.color(QPalette::Base));
      if (m_Green.green()+20 >= 255) {
         m_Green.setRed ( ((int)m_Green.red()  -20));
         m_Green.setBlue( ((int)m_Green.blue() -20));
      }
      else
         m_Green.setGreen(((int)m_Green.green()+20));
      
      m_Red = QColor(m_Pal.color(QPalette::Base));
      if (m_Red.red()+20 >= 255) {
         m_Red.setGreen(  ((int)m_Red.green()  -20));
         m_Red.setBlue(   ((int)m_Red.blue()   -20));
      }
      else
         m_Red.setRed(    ((int)m_Red.red()     +20));

      m_Yellow = QColor(m_Pal.color(QPalette::Base));
      if (m_Yellow.red()+20 >= 255 || m_Green.green()+20 >= 255) {
         m_Yellow.setBlue(((int)m_Yellow.blue() -20));
      }
      else {
         m_Yellow.setGreen(((int)m_Yellow.green()+20));
         m_Yellow.setRed( ((int)m_Yellow.red()   +20));
      }
   }
   
   virtual QVariant getColor(const Account* a) {
      if(a->getAccountRegistrationStatus() == ACCOUNT_STATE_UNREGISTERED || !a->isEnabled())
         return m_Pal.color(QPalette::Base);
      if(a->getAccountRegistrationStatus() == ACCOUNT_STATE_REGISTERED || a->getAccountRegistrationStatus() == ACCOUNT_STATE_READY) {
         return m_Green;
      }
      if(a->getAccountRegistrationStatus() == ACCOUNT_STATE_TRYING)
         return m_Yellow;
      return m_Red;
   }

   virtual QVariant getIcon(const Account* a) {
      if (a->currentState() == MODIFIED)
         return KIcon("document-save");
      else if (a->currentState() == OUTDATED) {
         return KIcon("view-refresh");
      }
      return QVariant();
   }
private:
   QPalette m_Pal;
   QColor   m_Green;
   QColor   m_Yellow;
   QColor   m_Red;
};

///Constructor
SFLPhoneView::SFLPhoneView(QWidget *parent)
   : QWidget(parent), wizard(0), errorWindow(0)
{
   setupUi(this);

   callView->setTitle(i18n("Calls"));

   QPalette pal = QPalette(palette());
   pal.setColor(QPalette::AlternateBase, Qt::lightGray);
   setPalette(pal);

   AccountList::getInstance()->setColorVisitor(new ColorVisitor(pal));

   m_pMessageBoxW->setVisible(false);

   //                SENDER                             SIGNAL                             RECEIVER                                 SLOT                                  /
   /**/connect(SFLPhone::model()          , SIGNAL(incomingCall(Call*))                   , this                       , SLOT(on1_incomingCall(Call*))                    );
   /**/connect(SFLPhone::model()          , SIGNAL(voiceMailNotify(QString,int))          , this                       , SLOT(on1_voiceMailNotify(QString,int))           );
   /**/connect(callView                   , SIGNAL(itemChanged(Call*))                    , this                       , SLOT(updateWindowCallState())                    );
   /**///connect(SFLPhone::model()          , SIGNAL(volumeChanged(const QString &, double)), this                     , SLOT(on1_volumeChanged(const QString &, double) ));
   /**/connect(SFLPhone::model()          , SIGNAL(callStateChanged(Call*))               , this                       , SLOT(updateWindowCallState())                    );
   /**/connect(AccountList::getInstance() , SIGNAL(accountStateChanged(Account*,QString)) , this                       , SLOT(updateStatusMessage())                      );
   /**/connect(AccountList::getInstance() , SIGNAL(accountListUpdated())                  , this                       , SLOT(updateStatusMessage())                      );
   /**/connect(AccountList::getInstance() , SIGNAL(accountListUpdated())                  , this                       , SLOT(updateWindowCallState())                    );
   /**/connect(m_pSendMessageLE           , SIGNAL(returnPressed())                       , this                       , SLOT(sendMessage())                              );
   /**/connect(m_pSendMessagePB           , SIGNAL(clicked())                             , this                       , SLOT(sendMessage())                              );
   /*                                                                                                                                                                     */

   AccountList::getInstance()->updateAccounts();
}

///Destructor
SFLPhoneView::~SFLPhoneView()
{
}

///Init main window
void SFLPhoneView::loadWindow()
{
   updateWindowCallState ();
   updateRecordButton    ();
   updateVolumeButton    ();
   updateRecordBar       ();
   updateVolumeBar       ();
   updateVolumeControls  ();
   updateDialpad         ();
   updateStatusMessage   ();
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/


///Return the error window
QErrorMessage * SFLPhoneView::getErrorWindow()
{
   if (!errorWindow)
      errorWindow = new QErrorMessage(this);
   return errorWindow;
}


/*****************************************************************************
 *                                                                           *
 *                              Keyboard input                               *
 *                                                                           *
 ****************************************************************************/

///Input grabber
void SFLPhoneView::keyPressEvent(QKeyEvent *event)
{
   int key = event->key();
   if(key == Qt::Key_Escape)
      escape();
   else if(key == Qt::Key_Return || key == Qt::Key_Enter)
      enter();
   else if(key == Qt::Key_Backspace)
      backspace();
   else if (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down)
      callView->moveSelectedItem((Qt::Key)key);
   else
   {
      QString text = event->text();
      if(! text.isEmpty())
      {
         typeString(text);
      }
   }
} //keyPressEvent

///Called on keyboard
void SFLPhoneView::typeString(QString str)
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();

   Call* call = callView->getCurrentItem();
   callManager.playDTMF(str);
   Call* currentCall = nullptr;
   Call* candidate   = nullptr;

   if(call && call->getState() == CALL_STATE_CURRENT) {
      currentCall = call;
   }

   foreach (Call* call2, SFLPhone::model()->getCallList()) {
      if(dynamic_cast<Call*>(call2) && currentCall != call2 && call2->getState() == CALL_STATE_CURRENT) {
         action(call2, CALL_ACTION_HOLD);
      }
      else if(dynamic_cast<Call*>(call2) && call2->getState() == CALL_STATE_DIALING) {
         candidate = call2;
      }
   }

   if(!currentCall && !candidate) {
      kDebug() << "Typing when no item is selected. Opening an item.";
      candidate = SFLPhone::model()->addDialingCall();
   }

   if(!currentCall && candidate) {
      candidate->appendText(str);
   }
   if (!candidate)
      HelperFunctions::displayNoAccountMessageBox(this);
} //typeString

///Called when a backspace is detected
void SFLPhoneView::backspace()
{
   kDebug() << "backspace";
   Call* call = callView->getCurrentItem();
   if(!call) {
      kDebug() << "Error : Backspace on unexisting call.";
   }
   else {
      call->backspaceItemText();
      if(call->getState() == CALL_STATE_OVER && callView->getCurrentItem()) {
         callView->removeItem(callView->getCurrentItem());
      }
   }
}

///Called when escape is detected
void SFLPhoneView::escape()
{
   kDebug() << "escape";
   Call* call = callView->getCurrentItem();
   if (callView->haveOverlay()) {
      callView->hideOverlay();
   }
   else if(!call) {
      kDebug() << "Escape when no item is selected. Doing nothing.";
   }
   else {
      if(call->getState() == CALL_STATE_TRANSFERRED || call->getState() == CALL_STATE_TRANSF_HOLD) {
         action(call, CALL_ACTION_TRANSFER);
      }
      else {
         action(call, CALL_ACTION_REFUSE);
      }
   }
} //escape

///Called when enter is detected
void SFLPhoneView::enter()
{
   kDebug() << "enter";
   Call* call = callView->getCurrentItem();
   if(!call) {
      kDebug() << "Error : Enter on unexisting call.";
   }
   else {
      int state = call->getState();
      if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFERRED || state == CALL_STATE_TRANSF_HOLD) {
         action(call, CALL_ACTION_ACCEPT);
      }
      else {
         kDebug() << "Enter when call selected not in appropriate state. Doing nothing.";
      }
   }
}

///Create a call from the clipboard content
void SFLPhoneView::paste()
{
   QClipboard* cb = QApplication::clipboard();
   const QMimeData* data = cb->mimeData();
   if (data->hasFormat(MIME_PHONENUMBER)) {
      typeString(data->data(MIME_PHONENUMBER));
   }
   else {
      typeString(cb->text());
   }
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Change call state
void SFLPhoneView::action(Call* call, call_action action)
{
   if(! call) {
      kDebug() << "Error : action " << action << "applied on null object call. Should not happen.";
   }
   else {
      try {
         call->actionPerformed(action);
      }
      catch(const char * msg) {
         getErrorWindow()->showMessage(QString(msg));
      }
      updateWindowCallState();
   }
} //action

///Select a phone number when calling using a contact
bool SFLPhoneView::selectCallPhoneNumber(Call** call2,Contact* contact)
{
   if (contact->getPhoneNumbers().count() == 1) {
      *call2 = SFLPhone::model()->addDialingCall(contact->getFormattedName(),AccountList::getCurrentAccount());
      if (*call2)
         (*call2)->appendText(contact->getPhoneNumbers()[0]->getNumber());
   }
   else if (contact->getPhoneNumbers().count() > 1) {
      bool                   ok = false;
      QHash<QString,QString> map       ;
      QStringList            list      ;
      foreach (Contact::PhoneNumber* number, contact->getPhoneNumbers()) {
         map[number->getType()+" ("+number->getNumber()+')'] = number->getNumber();
         list << number->getType()+" ("+number->getNumber()+')';
      }
      QString result = QInputDialog::getItem (this, i18n("Select phone number"), i18n("This contact have many phone number, please select the one you wish to call"), list, 0, false, &ok);
      if (ok) {
         (*call2) = SFLPhone::model()->addDialingCall(contact->getFormattedName(), AccountList::getCurrentAccount());
         if (*call2)
            (*call2)->appendText(map[result]);
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
   return true;
} //selectCallPhoneNumber


/*****************************************************************************
 *                                                                           *
 *                       Update display related code                         *
 *                                                                           *
 ****************************************************************************/

///Change GUI icons
void SFLPhoneView::updateWindowCallState()
{
   kDebug() << "Call state changed";
   bool    enabledActions [6] = { true             ,true                 , true             , true                 , true               , true                 };
   QString buttonIconFiles[6] = { ICON_CALL        , ICON_HANGUP         , ICON_HOLD        , ICON_TRANSFER        , ICON_REC_DEL_OFF   , ICON_MAILBOX         };
   QString actionTexts    [6] = { ACTION_LABEL_CALL, ACTION_LABEL_HANG_UP, ACTION_LABEL_HOLD, ACTION_LABEL_TRANSFER, ACTION_LABEL_RECORD, ACTION_LABEL_MAILBOX };

   Call* call = 0;

   bool transfer(false),recordActivated(false);

   enabledActions[SFLPhone::Mailbox] = AccountList::getCurrentAccount() && ! AccountList::getCurrentAccount()->getAccountMailbox().isEmpty();

   call = callView->getCurrentItem();
   if (!call) {
      kDebug() << "No item selected.";
      enabledActions[ SFLPhone::Refuse   ] = false;
      enabledActions[ SFLPhone::Hold     ] = false;
      enabledActions[ SFLPhone::Transfer ] = false;
      enabledActions[ SFLPhone::Record   ] = false;
      m_pMessageBoxW->setVisible(false);
   }
   else if (call->isConference()) {
      //TODO Something to do?
   }
   else {
      call_state state = call->getState();
      recordActivated = call->getRecording();

      kDebug() << "Reached  State" << state << "(" << call->toHumanStateName() << ") with call" << call->getCallId();

      switch (state) {
         case CALL_STATE_INCOMING:
            buttonIconFiles [ SFLPhone::Accept   ] = ICON_ACCEPT                 ;
            buttonIconFiles [ SFLPhone::Refuse   ] = ICON_REFUSE                 ;
            actionTexts     [ SFLPhone::Accept   ] = ACTION_LABEL_ACCEPT         ;
            actionTexts     [ SFLPhone::Refuse   ] = ACTION_LABEL_REFUSE         ;
            m_pMessageBoxW->setVisible(false || IM_ACTIVE)   ;
            break;

         case CALL_STATE_RINGING:
            enabledActions  [ SFLPhone::Hold     ] = false                       ;
            enabledActions  [ SFLPhone::Transfer ] = false                       ;
            m_pMessageBoxW->setVisible(false)                                    ;
            break;

         case CALL_STATE_CURRENT:
            buttonIconFiles [ SFLPhone::Record   ] = ICON_REC_DEL_ON             ;
            m_pMessageBoxW->setVisible((true && ConfigurationSkeleton::displayMessageBox()) || IM_ACTIVE);
            break;

         case CALL_STATE_DIALING:
            enabledActions  [ SFLPhone::Hold     ] = false                       ;
            enabledActions  [ SFLPhone::Transfer ] = false                       ;
            enabledActions  [ SFLPhone::Record   ] = false                       ;
            actionTexts     [ SFLPhone::Accept   ] = ACTION_LABEL_ACCEPT         ;
            buttonIconFiles [ SFLPhone::Accept   ] = ICON_ACCEPT                 ;
            m_pMessageBoxW->setVisible(false)                                    ;
            break;

         case CALL_STATE_HOLD:
            buttonIconFiles [ SFLPhone::Hold     ] = ICON_UNHOLD                 ;
            actionTexts     [ SFLPhone::Hold     ] = ACTION_LABEL_UNHOLD         ;
            m_pMessageBoxW->setVisible(false)                                    ;
            break;

         case CALL_STATE_FAILURE:
            enabledActions  [ SFLPhone::Accept   ] = false                       ;
            enabledActions  [ SFLPhone::Hold     ] = false                       ;
            enabledActions  [ SFLPhone::Transfer ] = false                       ;
            enabledActions  [ SFLPhone::Record   ] = false                       ;
            m_pMessageBoxW->setVisible(false)                                    ;
            break;

         case CALL_STATE_BUSY:
            enabledActions  [ SFLPhone::Accept   ] = false                       ;
            enabledActions  [ SFLPhone::Hold     ] = false                       ;
            enabledActions  [ SFLPhone::Transfer ] = false                       ;
            enabledActions  [ SFLPhone::Record   ] = false                       ;
            m_pMessageBoxW->setVisible(false)                                    ;
            break;

         case CALL_STATE_TRANSFERRED:
            buttonIconFiles [ SFLPhone::Accept   ] = ICON_EXEC_TRANSF            ;
            actionTexts     [ SFLPhone::Transfer ] = ACTION_LABEL_GIVE_UP_TRANSF ;
            buttonIconFiles [ SFLPhone::Record   ] = ICON_REC_DEL_ON             ;
            m_pMessageBoxW->setVisible(false || IM_ACTIVE)                       ;
            transfer = true;
            break;

         case CALL_STATE_TRANSF_HOLD:
            buttonIconFiles [ SFLPhone::Accept   ] = ICON_EXEC_TRANSF            ;
            buttonIconFiles [ SFLPhone::Hold     ] = ICON_UNHOLD                 ;
            actionTexts     [ SFLPhone::Transfer ] = ACTION_LABEL_GIVE_UP_TRANSF ;
            actionTexts     [ SFLPhone::Hold     ] = ACTION_LABEL_UNHOLD         ;
            m_pMessageBoxW->setVisible(false)                                    ;
            transfer = true;
            break;

         case CALL_STATE_OVER:
            kDebug() << "Error : Reached CALL_STATE_OVER with call "  << call->getCallId() << "!";
            m_pMessageBoxW->setVisible(false)                                    ;
            break;

         case CALL_STATE_ERROR:
            kDebug() << "Error : Reached CALL_STATE_ERROR with call " << call->getCallId() << "!";
            m_pMessageBoxW->setVisible(false)                                    ;
            break;

         case CALL_STATE_CONFERENCE:
            enabledActions  [ SFLPhone::Transfer ] = false                       ;
            m_pMessageBoxW->setVisible(false || IM_ACTIVE)                       ;
            break;

         case CALL_STATE_CONFERENCE_HOLD:
            enabledActions  [ SFLPhone::Transfer ] = false                       ;
            m_pMessageBoxW->setVisible(false)                                    ;
            break;

         default: 
            kDebug() << "Error : Reached unexisting state for call "  << call->getCallId() << "(" << call->toHumanStateName() << "!";
            break;

      }
   }

   kDebug() << "Updating Window.";

   emit enabledActionsChangeAsked     ( enabledActions  );
   emit actionIconsChangeAsked        ( buttonIconFiles );
   emit actionTextsChangeAsked        ( actionTexts     );
   emit transferCheckStateChangeAsked ( transfer        );
   emit recordCheckStateChangeAsked   ( recordActivated );

   kDebug() << "Window updated.";
} //updateWindowCallState

///Change icon of the record button
void SFLPhoneView::updateRecordButton()
{
   kDebug() << "updateRecordButton";
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   double recVol = callManager.getVolume(RECORD_DEVICE);
   if(recVol     == 0.00) {
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

///Update the colume button icon
void SFLPhoneView::updateVolumeButton()
{
   kDebug() << "updateVolumeButton";
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   double sndVol = callManager.getVolume(SOUND_DEVICE);

   if(sndVol     == 0.00) {
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

///Update the record bar
void SFLPhoneView::updateRecordBar(double _value)
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   double recVol = callManager.getVolume(RECORD_DEVICE);
   kDebug() << "updateRecordBar" << recVol;
   int value = (_value > 0)?_value:(int)(recVol * 100);
   slider_recVol->setValue(value);
}

///Update the volume bar
void SFLPhoneView::updateVolumeBar(double _value)
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   double sndVol = callManager.getVolume(SOUND_DEVICE);
   kDebug() << "updateVolumeBar" << sndVol;
   int value = (_value > 0)?_value:(int)(sndVol * 100);
   slider_sndVol->setValue(value);
}

///Hide or show the volume control
void SFLPhoneView::updateVolumeControls()
{
   toolButton_recVol->setVisible ( SFLPhone::app()->action_displayVolumeControls->isChecked() && ConfigurationSkeleton::displayVolume() );
   toolButton_sndVol->setVisible ( SFLPhone::app()->action_displayVolumeControls->isChecked() && ConfigurationSkeleton::displayVolume() );
   slider_recVol->setVisible     ( SFLPhone::app()->action_displayVolumeControls->isChecked() && ConfigurationSkeleton::displayVolume() );
   slider_sndVol->setVisible     ( SFLPhone::app()->action_displayVolumeControls->isChecked() && ConfigurationSkeleton::displayVolume() );
}

///Hide or show the dialpad
void SFLPhoneView::updateDialpad()
{
   widget_dialpad->setVisible(ConfigurationSkeleton::displayDialpad());//TODO use display variable
}

///Change the statusbar message
void SFLPhoneView::updateStatusMessage()
{
   Account * account = AccountList::getCurrentAccount();

   if(!account) {
      emit statusMessageChangeAsked(i18n("No registered accounts"));
   }
   else {
      emit statusMessageChangeAsked(i18n("Using account")
         + " \'" + account->getAlias()
         + "\' (" + account->getAccountRegistrationStatus() + ')');
   }
}


/*****************************************************************************
 *                                                                           *
 *                                    Slots                                  *
 *                                                                           *
 ****************************************************************************/

///Proxy to hide or show the volume control
void SFLPhoneView::displayVolumeControls(bool checked)
{
   //ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   ConfigurationSkeleton::setDisplayVolume(checked);
   updateVolumeControls();
}

///Proxy to hide or show the dialpad
void SFLPhoneView::displayDialpad(bool checked)
{
   ConfigurationSkeleton::setDisplayDialpad(checked);
   updateDialpad();
}

///Display a notification popup (freedesktop notification)
void SFLPhoneView::displayMessageBox(bool checked)
{
   ConfigurationSkeleton::setDisplayMessageBox(checked);
   Call* call = callView->getCurrentItem();
   m_pMessageBoxW->setVisible(checked
      && call
      && (call->getState()   == CALL_STATE_CURRENT
         || call->getState() == CALL_STATE_HOLD
      )
   );
}

///Input grabber
void SFLPhoneView::on_widget_dialpad_typed(QString text)
{
   typeString(text);
}

///The value on the slider changed
void SFLPhoneView::on_slider_recVol_valueChanged(int value)
{
   kDebug() << "on_slider_recVol_valueChanged(" << value << ")";
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.setVolume(RECORD_DEVICE, (double)value / 100.0);
   updateRecordButton();
}

///The value on the slider changed
void SFLPhoneView::on_slider_sndVol_valueChanged(int value)
{
   kDebug() << "on_slider_sndVol_valueChanged(" << value << ")";
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.setVolume(SOUND_DEVICE, (double)value / 100.0);
   updateVolumeButton();
}

///The mute button have been clicked
void SFLPhoneView::on_toolButton_recVol_clicked(bool checked)
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   kDebug() << "on_toolButton_recVol_clicked().";
   if(!checked) {
      toolButton_recVol->setChecked(false);
      slider_recVol->setEnabled(true);
      callManager.setVolume(RECORD_DEVICE, (double)slider_recVol->value() / 100.0);
   }
   else {
      toolButton_recVol->setChecked(true);
      slider_recVol->setEnabled(false);
      callManager.setVolume(RECORD_DEVICE, 0.0);
   }
   updateRecordButton();
}

///The mute button have been clicked
void SFLPhoneView::on_toolButton_sndVol_clicked(bool checked)
{
   CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
   kDebug() << "on_toolButton_sndVol_clicked().";
   if(!checked) {
      toolButton_sndVol->setChecked(false);
      slider_sndVol->setEnabled(true);
      callManager.setVolume(SOUND_DEVICE, (double)slider_sndVol->value() / 100.0);
   }
   else {
      toolButton_sndVol->setChecked(true);
      slider_sndVol->setEnabled(false);
      callManager.setVolume(SOUND_DEVICE, 0.0);
   }

   updateVolumeButton();
}

///There is a right click menu request
void SFLPhoneView::contextMenuEvent(QContextMenuEvent *event)
{
   KMenu menu(this);

   SFLPhone * window = SFLPhone::app();
   QList<QAction *> callActions = window->getCallActions();

   menu.addAction ( callActions.at((int) SFLPhone::Accept) );
   menu.addAction ( callActions[ SFLPhone::Refuse   ]      );
   menu.addAction ( callActions[ SFLPhone::Hold     ]      );
   menu.addAction ( callActions[ SFLPhone::Transfer ]      );
   menu.addAction ( callActions[ SFLPhone::Record   ]      );
   menu.addSeparator();

   QAction* action = new ActionSetAccountFirst(nullptr, &menu);
   action->setChecked(AccountList::getPriorAccoundId().isEmpty());
   connect(action,  SIGNAL(setFirst(Account*)), this  ,  SLOT(setAccountFirst(Account*)));
   menu.addAction(action);

   QVector<Account *> accounts = AccountList::getInstance()->registeredAccounts();
   for (int i = 0 ; i < accounts.size() ; i++) {
      Account* account = accounts.at(i);
      QAction* action = new ActionSetAccountFirst(account, &menu);
      action->setChecked(account->getAccountId() == AccountList::getPriorAccoundId());
      connect(action, SIGNAL(setFirst(Account*)), this  , SLOT(setAccountFirst(Account*)));
      menu.addAction(action);
   }
   menu.exec(event->globalPos());
}

///Pick the default account and load it
void SFLPhoneView::setAccountFirst(Account * account)
{
   kDebug() << "setAccountFirst : " << (account ? account->getAlias() : QString()) << (account ? account->getAccountId() : QString());
   if(account) {
      AccountList::setPriorAccountId(account->getAccountId());
   }
   else {
      AccountList::setPriorAccountId(QString());
   }
   kDebug() << "Current account id" << (AccountList::getCurrentAccount()?AccountList::getCurrentAccount()->getAccountId():"<no account>");
   updateStatusMessage();
}

///Show the configuration dialog
void SFLPhoneView::configureSflPhone()
{
   ConfigurationDialog* configDialog = new ConfigurationDialog(this);
   configDialog->setModal(true);

   connect(configDialog, SIGNAL(changesApplied()),
           this,         SLOT(loadWindow()));

   //configDialog->reload();
   configDialog->show();
}

///Show the account creation wizard
void SFLPhoneView::accountCreationWizard()
{
   if (!wizard) {
      wizard = new AccountWizard(this);
      wizard->setModal(false);
   }
   wizard->show();
}

///Call
void SFLPhoneView::accept()
{
   Call* call = callView->getCurrentItem();
   if(!call) {
      kDebug() << "Calling when no item is selected. Opening an item.";
      SFLPhone::model()->addDialingCall();
   }
   else {
      int state = call->getState();
      if (state == CALL_STATE_RINGING || state == CALL_STATE_CURRENT || state == CALL_STATE_HOLD || state == CALL_STATE_BUSY)
      {
         kDebug() << "Calling when item currently ringing, current, hold or busy. Opening an item.";
         SFLPhone::model()->addDialingCall();
      }
      else {
         action(call, CALL_ACTION_ACCEPT);
      }
   }
} //accept

///Refuse call
void SFLPhoneView::refuse()
{
   Call* call = callView->getCurrentItem();
   if(!call) {
      kDebug() << "Error : Hanging up when no item selected. Should not happen.";
   }
   else {
      action(call, CALL_ACTION_REFUSE);
   }
}

///Put call on hold
void SFLPhoneView::hold()
{
   Call* call = callView->getCurrentItem();
   if(!call) {
      kDebug() << "Error : Holding when no item selected. Should not happen.";
   }
   else {
      action(call, CALL_ACTION_HOLD);
   }
}

///Transfer a call
void SFLPhoneView::transfer()
{
   Call* call = callView->getCurrentItem();
   if(!call) {
      kDebug() << "Error : Transferring when no item selected. Should not happen.";
   }
   else {
      action(call, CALL_ACTION_TRANSFER);
   }
}

///Record a call
void SFLPhoneView::record()
{
   Call* call = callView->getCurrentItem();
   if(!call) {
      kDebug() << "Error : Recording when no item selected. Should not happen.";
   }
   else {
      action(call, CALL_ACTION_RECORD);
   }
}

///Access the voice mail list
void SFLPhoneView::mailBox()
{
   Account* account = AccountList::getCurrentAccount();
   QString mailBoxNumber = account->getAccountMailbox();
   Call* call = SFLPhone::model()->addDialingCall();
   if (call) {
      call->appendText(mailBoxNumber);
      action(call, CALL_ACTION_ACCEPT);
   }
   else {
      HelperFunctions::displayNoAccountMessageBox(this);
   }
}

///Called the there is an error (dbus)
void SFLPhoneView::on1_error(MapStringString details)
{
   kDebug() << "Signal : Daemon error : " << details;
}

///When a call is coming (dbus)
void SFLPhoneView::on1_incomingCall(Call* call)
{
   kDebug() << "Signal : Incoming Call ! ID = " << call->getCallId();

   updateWindowCallState();

   SFLPhone::app()->activateWindow  (      );
   SFLPhone::app()->raise           (      );
   SFLPhone::app()->setVisible      ( true );

   emit incomingCall(call);
}

///When a new voice mail is coming
void SFLPhoneView::on1_voiceMailNotify(const QString &accountID, int count)
{
   kDebug() << "Signal : VoiceMail Notify ! " << count << " new voice mails for account " << accountID;
}

///When the volume change
void SFLPhoneView::on1_volumeChanged(const QString & /*device*/, double value)
{
   kDebug() << "Signal : Volume Changed !" << value;
   if(! (toolButton_recVol->isChecked() && value == 0.0))
      updateRecordBar(value);
   if(! (toolButton_sndVol->isChecked() && value == 0.0))
      updateVolumeBar(value);
}

///Send a text message
void SFLPhoneView::sendMessage()
{
   Call* call = callView->getCurrentItem();
   if (dynamic_cast<Call*>(call) && !m_pSendMessageLE->text().isEmpty()) {
      call->sendTextMessage(m_pSendMessageLE->text());
   }
   m_pSendMessageLE->clear();
}

#include "SFLPhoneView.moc"
