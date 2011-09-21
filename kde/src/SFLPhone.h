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
 ***************************************************************************/

#ifndef SFLPHONE_H
#define SFLPHONE_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QListWidgetItem>
#include <QtGui/QKeyEvent>
#include <QErrorMessage>
#include <KSystemTrayIcon>
#include <KNotification>

#include <KXmlGuiWindow>
#include <KAction>
#include "widgets/ContactDock.h"
#include "widgets/HistoryDock.h"
#include "widgets/BookmarkDock.h"
#include <QActionGroup>

// #include "ui_SFLPhoneView_base.h"
//#include "CallList.h"
#include "AccountWizard.h"
#include "lib/Contact.h"
#include "SFLPhoneView.h"
#include "widgets/SFLPhoneTray.h"

class SFLPhoneView;
class CallView;

/**
 * This class represents the SFLphone main window
 * It implements the methods relative to windowing
 * (status, menus, toolbars, notifications...).
 * It uses a view which implements the real functionning
 * and features of the phone.
 * The display of the window is according to the state of the view,
 * so the view sends some signals to ask for changes on the window
 * that the window has to take into account.
 *
 * @short Main window
 * @author Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>
 * @version 0.9.6
**/
class SFLPhone : public KXmlGuiWindow
{
Q_OBJECT

public:
enum CallAction {
        Accept,
        Refuse,
        Hold,
        Transfer,
        Record,
        Mailbox,
        NumberOfCallActions};

private:
   // Whether or not the object has been initialized
   bool   initialized_;
   KAction * action_accept;
   KAction * action_refuse;
   KAction * action_hold;
   KAction * action_transfer;
   KAction * action_record;
   QActionGroup * action_screen;
   KAction * action_main;
   KAction * action_history;
   KAction * action_addressBook;
   KAction * action_mailBox;
   KAction * action_close;
   KAction * action_quit;
   KAction * action_displayVolumeControls;
   KAction * action_displayDialpad;
   KAction * action_configureSflPhone;
   KAction * action_accountCreationWizard;

   SFLPhoneView * m_pView;
   QMenu *trayIconMenu;
   bool iconChanged;
   SFLPhoneTray *trayIcon;
   KNotification *notification;
   QLabel*       statusBarWidget;
   ContactDock*  m_pContactCD;
   QDockWidget*  m_pCentralDW;
   HistoryDock*  m_pHistoryDW;
   BookmarkDock* m_pBookmarkDW;
   
   static SFLPhone* m_sApp;
   
private:
   void setObjectNames();

protected:
   virtual bool queryClose();
   virtual void changeEvent(QEvent * event);
   

public:
   SFLPhone(QWidget *parent = 0);
   ~SFLPhone();
        bool initialize();
   void setupActions();
   void trayIconSignal();
   SFLPhoneView * getView();
   QList<QAction *> getCallActions();

   friend class SFLPhoneView;
   
   static SFLPhone* app();
   SFLPhoneView* view();
   CallView* model();
   
   
private slots:
   void on_m_pView_statusMessageChangeAsked(const QString & message);
   void on_m_pView_windowTitleChangeAsked(const QString & message);
   void on_m_pView_enabledActionsChangeAsked(const bool * enabledActions);
   void on_m_pView_actionIconsChangeAsked(const QString * actionIcons);
   void on_m_pView_actionTextsChangeAsked(const QString * actionTexts);
   void on_m_pView_transferCheckStateChangeAsked(bool transferCheckState);
   void on_m_pView_recordCheckStateChangeAsked(bool recordCheckState);
   void on_m_pView_addressBookEnableAsked(bool enabled);
   void on_m_pView_screenChanged(int screen);
   void on_m_pView_incomingCall(const Call * call);

   void updateScreen(QAction * action);

   void quitButton();

};

#endif
 
