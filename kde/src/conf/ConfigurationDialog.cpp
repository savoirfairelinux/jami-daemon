/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/
#include "ConfigurationDialog.h"

//KDE
#include <KDebug>


#include "klib/ConfigurationSkeleton.h"

#include "dlggeneral.h"
#include "dlgdisplay.h"
#include "dlgaccounts.h"
#include "dlgaudio.h"
#include "dlgaddressbook.h"
#include "dlghooks.h"
#include "dlgaccessibility.h"
#include "dlgvideo.h"

#include "lib/sflphone_const.h"

ConfigurationDialog::ConfigurationDialog(SFLPhoneView *parent)
 :KConfigDialog(parent, SETTINGS_NAME, ConfigurationSkeleton::self())
{
   this->setWindowIcon(QIcon(ICON_SFLPHONE));

   dlgGeneral       = new DlgGeneral       (this);
   dlgDisplay       = new DlgDisplay       (this);
   dlgAudio         = new DlgAudio         (this);
   dlgAddressBook   = new DlgAddressBook   (this);
   dlgHooks         = new DlgHooks         (this);
   dlgAccessibility = new DlgAccessibility (this);
   dlgAccounts      = new DlgAccounts      (this);

   #ifdef ENABLE_VIDEO
   dlgVideo         = new DlgVideo         (this);
   #endif
   
   addPage( dlgGeneral       , i18nc("General settings","General")       , "sflphone-client-kde"               );
   addPage( dlgAccounts      , i18n("Accounts")      , "user-identity"                     );
   addPage( dlgAudio         , i18n("Audio")         , "audio-headset"                     );
   addPage( dlgAddressBook   , i18n("Address Book")  , "x-office-address-book"             );
   addPage( dlgHooks         , i18n("Hooks")         , "insert-link"                       );
   addPage( dlgAccessibility , i18n("Accessibility") , "preferences-desktop-accessibility" );
   #ifdef ENABLE_VIDEO
   addPage( dlgVideo         , i18nc("Video conversation","Video")         , "camera-web"                        );
   #endif
   addPage( dlgDisplay       , i18nc("User interterface settings","Display")       , "applications-graphics"             );

   connect(this, SIGNAL(applyClicked()),  this,     SLOT(applyCustomSettings()));
   connect(this, SIGNAL(okClicked()),     this,     SLOT(applyCustomSettings()));
   connect(this, SIGNAL(cancelClicked()), this,     SLOT(cancelSettings()     ));

   connect(dlgGeneral, SIGNAL(clearCallHistoryAsked()), this, SIGNAL(clearCallHistoryAsked()));
} //ConfigurationDialog


ConfigurationDialog::~ConfigurationDialog()
{
   delete dlgGeneral;
   delete dlgDisplay;
   delete dlgAccounts;
   delete dlgAudio;
   delete dlgAddressBook;
   delete dlgHooks;
   delete dlgAccessibility;
   #ifdef ENABLE_VIDEO
   delete dlgVideo;
   #endif
}

void ConfigurationDialog::updateWidgets()
{
   dlgAudio->updateWidgets        ();
   dlgAccounts->updateWidgets     ();
   dlgGeneral->updateWidgets      ();
   dlgAddressBook->updateWidgets  ();
   dlgAccessibility->updateWidgets();
}

void ConfigurationDialog::updateSettings()
{
   dlgAudio->updateSettings        ();
   dlgAccounts->updateSettings     ();
   dlgGeneral->updateSettings      ();
   dlgAddressBook->updateSettings  ();
   dlgAccessibility->updateSettings();
}

void ConfigurationDialog::cancelSettings()
{
   dlgAccounts->cancel();
}

bool ConfigurationDialog::hasChanged()
{
   bool res = dlgAudio->hasChanged() || dlgAccounts->hasChanged() || dlgGeneral->hasChanged();
   return res;
}

void ConfigurationDialog::updateButtons()
{
   bool changed = hasChanged();
   enableButtonApply( changed );
}

void ConfigurationDialog::applyCustomSettings()
{
   if(hasChanged()) {
          ConfigurationSkeleton::self()->writeConfig();
   }
   updateSettings();
   updateWidgets ();
   updateButtons ();
   emit changesApplied();
}

void ConfigurationDialog::reload()
{
   kDebug() << "Reloading config";
   ConfigurationSkeleton::self()->readConfig();
   updateWidgets();
   updateButtons();
}
