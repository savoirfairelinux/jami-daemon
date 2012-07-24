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
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/

#ifndef SFLPHONE_H
#define SFLPHONE_H

#include <KXmlGuiWindow>
#include <lib/CallModel.h>

//Qt
class QString;
class QLabel;
class QTreeWidgetItem;
class QActionGroup;

//KDE
class KAction;

//SFLPhone
class Call;
class ContactDock;
class BookmarkDock;
class VideoDock;
class SFLPhoneTray;
class SFLPhoneView;
class HistoryDock;
class CallTreeItem;
class VideoRenderer;

typedef CallModel<CallTreeItem*,QTreeWidgetItem*> TreeWidgetCallModel;

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
 * @author Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
 * @version 1.1.0
**/
class SFLPhone : public KXmlGuiWindow
{
Q_OBJECT

public:
enum CallAction {
        Accept            ,
        Refuse            ,
        Hold              ,
        Transfer          ,
        Record            ,
        Mailbox           ,
        NumberOfCallActions
};

private:
   //Attributes
   bool   m_pInitialized;
   KAction* action_accept                ;
   KAction* action_refuse                ;
   KAction* action_hold                  ;
   KAction* action_transfer              ;
   KAction* action_record                ;
   KAction* action_mailBox               ;
   KAction* action_close                 ;
   KAction* action_quit                  ;
   KAction* action_displayVolumeControls ;
   KAction* action_displayDialpad        ;
   KAction* action_displayMessageBox     ;
   KAction* action_configureSflPhone     ;
   KAction* action_configureShortcut     ;
   KAction* action_accountCreationWizard ;
   KAction* action_pastenumber           ;
   KAction* action_showContactDock       ;
   KAction* action_showHistoryDock       ;
   KAction* action_showBookmarkDock      ;
   QActionGroup* action_screen           ;

   SFLPhoneView*  m_pView            ;
   bool           m_pIconChanged     ;
   SFLPhoneTray*  m_pTrayIcon        ;
   QLabel*        m_pStatusBarWidget ;
   ContactDock*   m_pContactCD       ;
   QDockWidget*   m_pCentralDW       ;
   HistoryDock*   m_pHistoryDW       ;
   BookmarkDock*  m_pBookmarkDW      ;
   #ifdef ENABLE_VIDEO
   VideoDock*     m_pVideoDW         ;
   #endif

   static SFLPhone* m_sApp;
   static TreeWidgetCallModel* m_pModel;

   //Setters
   void setObjectNames();

protected:
   virtual bool queryClose();
   virtual void changeEvent(QEvent * event);


public:
   SFLPhone(QWidget *parent = 0);
   ~SFLPhone                       ();
   bool             initialize     ();
   void             setupActions   ();
   void             trayIconSignal ();
   QList<QAction *> getCallActions ();

   friend class SFLPhoneView;

   static SFLPhone*            app   ();
   static TreeWidgetCallModel* model ();
   SFLPhoneView*               view  ();

   ContactDock*  contactDock ();
   HistoryDock*  historyDock ();
   BookmarkDock* bookmarkDock();

private slots:
   void on_m_pView_statusMessageChangeAsked      ( const QString& message        );
   void on_m_pView_windowTitleChangeAsked        ( const QString& message        );
   void on_m_pView_enabledActionsChangeAsked     ( const bool*    enabledActions );
   void on_m_pView_actionIconsChangeAsked        ( const QString* actionIcons    );
   void on_m_pView_actionTextsChangeAsked        ( const QString* actionTexts    );
   void on_m_pView_transferCheckStateChangeAsked ( bool  transferCheckState      );
   void on_m_pView_recordCheckStateChangeAsked   ( bool  recordCheckState        );
   void on_m_pView_incomingCall                  ( const Call*    call           );
   void showShortCutEditor                       (                               );
   void quitButton                               (                               );
   #ifdef ENABLE_VIDEO
   void displayVideoDock                         ( VideoRenderer* r              );
   #endif
};

#endif
