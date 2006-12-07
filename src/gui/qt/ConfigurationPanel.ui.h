/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *                                                                              
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                              
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/
#include <qdir.h>
#include <qmessagebox.h>
#include <qstringlist.h>
#include <qcolor.h>
#include <qvbuttongroup.h>

#include "globals.h"
#include "ConfigurationManager.hpp"
#include "DebugOutput.hpp"
#include "QjListBoxPixmap.hpp"
#include "SkinManager.hpp"
#include "TransparentWidget.hpp"

#include <map>

#define SIGNALISATIONS_IMAGE "signalisations.png"
#define AUDIO_IMAGE "audio.png"
#define PREFERENCES_IMAGE "preferences.png"
#define ABOUT_IMAGE "about.png"


void ConfigurationPanel::init()
{
  _cutStringCombo = 30;
  //DebugOutput::instance() << "ConfigurationPanel::init()\n"; 
  lblError->hide();
  lblIAXError->hide();
  Tab_Signalisations->show();
  Tab_Audio->hide();
  Tab_Preferences->hide();
  Tab_About->hide();

  /*
    // For reading settings at application startup
     // List skin choice from "skins" directory
   QDir dir(Skin::getPath(QString(SKINDIR)));
   if ( !dir.exists() ) {
        _debug("\nCannot find 'skins' directory\n");
		return;
    } else {
    dir.setFilter( QDir::Dirs | QDir::NoSymLinks);
    dir.setSorting( QDir::Name );
  
    QStringList list;
    list = dir.entryList();
    for (unsigned int i = 0; i < dir.count(); i++) {
     if (list[i] != "." && list[i] != ".." && list[i] != "CVS") {
    SkinChoice->insertItem(list[i]);
     }
    } 
 }
  */

   // For preferences tab

    /*
      SkinChoice->setCurrentText(QString(manager.getConfigString(PREFERENCES, SKIN_CHOICE)));
    confirmationToQuit->setChecked(manager.getConfigInt(PREFERENCES, CONFIRM_QUIT));
    zoneToneChoice->setCurrentText(QString(manager.getConfigString(
								   PREFERENCES, ZONE_TONE)));
    checkedTray->setChecked(manager.getConfigInt(
               PREFERENCES, CHECKED_TRAY));
  voicemailNumber->setText(QString(manager.getConfigString(
               PREFERENCES, VOICEMAIL_NUM)));
    */
   //  Init tab view order

    // Set items for QListBox

    new QjListBoxPixmap (QjListBoxPixmap::Above, 
			 TransparentWidget::retreive(SIGNALISATIONS_IMAGE),  
			 "Signalisation", 
			 Menu);
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
			 TransparentWidget::retreive(AUDIO_IMAGE),
			 "Audio", Menu );
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
    			 TransparentWidget::retreive(PREFERENCES_IMAGE),
    			 "Preferences", 
    			 Menu);
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
			 TransparentWidget::retreive(ABOUT_IMAGE),
			 "About", 
			 Menu);
}

void 
ConfigurationPanel::generate()
{
   // For audio tab
  codec1->setCurrentText(ConfigurationManager::instance().get(AUDIO_SECTION, AUDIO_CODEC1));
  codec2->setCurrentText(ConfigurationManager::instance().get(AUDIO_SECTION, AUDIO_CODEC2));
  codec3->setCurrentText(ConfigurationManager::instance().get(AUDIO_SECTION, AUDIO_CODEC3));

  ringsChoice->setCurrentText(ConfigurationManager::instance().get(AUDIO_SECTION, AUDIO_RINGTONE));

  // For signalisations tab
  
  // Account
  QComboBox* cbo = 0;
  int nbItem = 0;

  // Load account
  cbo = cboSIPAccount;
  cbo->clear();
  nbItem = 4;
  for (int iItem = 0; iItem < nbItem; iItem++) {
    QString accountId = "SIP" + QString::number(iItem);	  
    QString aliasName = ConfigurationManager::instance().get(accountId, ACCOUNT_ALIAS);
    QString accountName;
    if (aliasName.isEmpty()) {
      accountName = QObject::tr("SIP Account #%1").arg(iItem+1);
    } else {
      if (aliasName.length() > 30) {
        aliasName = aliasName.left(30) + "...";
      }
      accountName = aliasName + " (" + QObject::tr("SIP Account #%1").arg(iItem+1) + ")";
    }
    cbo->insertItem(accountName,iItem);
  }
  loadSIPAccount(0);

  // Load IAX Account
  cbo = cboIAXAccount;
  cbo->clear();
  nbItem = 1;
  for (int iItem = 0; iItem < nbItem; iItem++) {
    QString accountId = "IAX" + QString::number(iItem);	  
    QString aliasName = ConfigurationManager::instance().get(accountId, ACCOUNT_ALIAS);
	if (aliasName != "") {
      QString accountName;
      if (aliasName.isEmpty()) {
        accountName = QObject::tr("IAX Account #%1").arg(iItem+1);
      } else {
        if (aliasName.length() > 30) {
          aliasName = aliasName.left(30) + "...";
        }
        accountName = aliasName + " (" + QObject::tr("IAX Account #%1").arg(iItem+1) + ")";
      }
      cbo->insertItem(accountName,iItem);
	}
  }
  loadIAXAccount(0);

  sendDTMFas->setCurrentItem(ConfigurationManager::instance().get(SIGNALISATION_SECTION,
	 SIGNALISATION_SEND_DTMF_AS).toUInt());
  playTones->setChecked(ConfigurationManager::instance().get(SIGNALISATION_SECTION, 
	SIGNALISATION_PLAY_TONES).toUInt());
  pulseLength->setValue(ConfigurationManager::instance().get(SIGNALISATION_SECTION, 
	SIGNALISATION_PULSE_LENGTH).toUInt());


  // select the position index (combobox of the device index)
  // deviceIndexOut can be 8, but be at position 0 in the combo box
  int deviceIndexOut = ConfigurationManager::instance().get(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEOUT).toInt();
  int deviceIndexIn  = ConfigurationManager::instance().get(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEIN).toInt();
  int positionIndexIn  = 0;
  int positionIndexOut = 0;

  // search for deviceIndexIn, and get the positionIndex (key)
  std::map< int, int >::iterator it = _deviceInMap.begin();
  while (it != _deviceInMap.end()) {
    if ( it->second == deviceIndexIn) {
	  // we found the deviceIndex
	  positionIndexIn = it->first;
      break;
	}
	it++;
  }
  it = _deviceOutMap.begin();
  while (it != _deviceOutMap.end()) {
    if ( it->second == deviceIndexOut) {
	  // we found the deviceIndex
	  positionIndexOut = it->first;
      break;
	}
	it++;
  }
  cboDriverChoiceIn->setCurrentItem(positionIndexIn);
  cboDriverChoiceOut->setCurrentItem(positionIndexOut);

  // fill cboDriverRate here
  int nbRate = 5;
  int allowedRate[5] = {8000,16000,32000,44100,48000};
  cboDriverRate->clear();
  for(int iRate = 0; iRate < nbRate; iRate++) {
    cboDriverRate->insertItem(QString::number(allowedRate[iRate]));
  }
  cboDriverRate->setCurrentText(ConfigurationManager::instance().get(AUDIO_SECTION, AUDIO_SAMPLERATE));

  //preference tab
  updateSkins();
}

// For saving settings at application 'save'
void ConfigurationPanel::saveSlot()
{
  saveSIPAccount(cboSIPAccount->currentItem());
  saveIAXAccount(cboIAXAccount->currentItem());

  ConfigurationManager::instance().set(SIGNALISATION_SECTION, 
				       SIGNALISATION_PULSE_LENGTH,
				       QString::number(pulseLength->value()));
  ConfigurationManager::instance().set(SIGNALISATION_SECTION, 
				       SIGNALISATION_PLAY_TONES,
				       QString::number(playTones->isChecked()));
  ConfigurationManager::instance().set(SIGNALISATION_SECTION, 
				       SIGNALISATION_SEND_DTMF_AS,
				       QString::number(sendDTMFas->currentItem()));

  if (codec1->currentText() != NULL) {
    ConfigurationManager::instance().set(AUDIO_SECTION, AUDIO_CODEC1, codec1->currentText());
  }
  if (codec2->currentText() != NULL) {
    ConfigurationManager::instance().set(AUDIO_SECTION, AUDIO_CODEC2, codec2->currentText());
  }
  if (codec3->currentText() != NULL) {
    ConfigurationManager::instance().set(AUDIO_SECTION, AUDIO_CODEC3, codec3->currentText());
  }
  if (ringsChoice->currentText() != NULL) {
    ConfigurationManager::instance().set(AUDIO_SECTION, AUDIO_RINGTONE, ringsChoice->currentText());
  }
  int cboPosition = 0;
  int deviceIndex = 0;
  if (cboDriverChoiceOut->currentText() != NULL) {
    cboPosition = cboDriverChoiceOut->currentItem();
    deviceIndex = _deviceOutMap[cboPosition]; // return 0 if not found and create it, by STL
    ConfigurationManager::instance().set(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEOUT, QString::number(deviceIndex));
  }
  if (cboDriverChoiceIn->currentText() != NULL) {
    cboPosition = cboDriverChoiceIn->currentItem();
    deviceIndex = _deviceInMap[cboPosition]; // return 0 if not found and create it, by STL
    ConfigurationManager::instance().set(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEIN, QString::number(deviceIndex));
  }

  SkinManager::instance().load(SkinChoice->currentText());
  SkinManager::instance().save();

#if 0 
  QMessageBox::information(this, tr("Save settings"),
			   tr("You must restart SFLPhone"),
			   QMessageBox::Yes);
#endif

  ConfigurationManager::instance().save();
}

// Handle tab view  according to current item of listbox
void ConfigurationPanel::changeTabSlot()
{
  switch (Menu->currentItem()) {
  case 0:
    TitleTab->setText(tr("Setup signalisation"));
    Tab_Signalisations->show();
    Tab_Audio->hide();
    Tab_Preferences->hide();
    Tab_About->hide();
    break;
  case 1:
    TitleTab->setText(tr("Setup audio"));
    Tab_Signalisations->hide();
    Tab_Audio->show();
    Tab_Preferences->hide();
    Tab_About->hide();
    break;
  case 2:
    updateSkins();
    TitleTab->setText(tr("Setup preferences"));
    Tab_Signalisations->hide();
    Tab_Audio->hide();
    Tab_Preferences->show();
    Tab_About->hide();
    break; 
  case 3:
    TitleTab->setText(tr("About"));
    Tab_Signalisations->hide();
    Tab_Audio->hide();
    Tab_Preferences->hide();
    Tab_About->show();
    break;
  }
}


void ConfigurationPanel::useStunSlot(int id)
{
  QString account = ACCOUNT_DEFAULT_NAME;
  ConfigurationManager::instance().set(account,
				       SIGNALISATION_USE_STUN, 
				       QString::number(id));
}


void ConfigurationPanel::applySkinSlot()
{
  SkinManager::instance().load(SkinChoice->currentText());
}


void ConfigurationPanel::updateSkins()
{
  SkinChoice->clear();
  SkinChoice->insertStringList(SkinManager::instance().getSkins());
  SkinChoice->setSelected(SkinChoice->findItem(SkinManager::instance().getCurrentSkin(), Qt::ExactMatch), true);
}

void ConfigurationPanel::updateRingtones()
{
  std::list< Ringtone > rings = ConfigurationManager::instance().getRingtones();
  std::list< Ringtone >::iterator pos;

  ringsChoice->clear();
  
  for (pos = rings.begin(); pos != rings.end(); pos++) {
    ringsChoice->insertItem(pos->filename);
  } 
}

void ConfigurationPanel::updateCodecs()
{
  std::list< Codec > codecs = ConfigurationManager::instance().getCodecs();
  std::list< Codec >::iterator pos;

  codec1->clear();
  codec2->clear();
  codec3->clear();
  
  for (pos = codecs.begin(); pos != codecs.end(); pos++) {
    codec1->insertItem(pos->codecName);
    codec2->insertItem(pos->codecName);
    codec3->insertItem(pos->codecName);
  } 
}

void ConfigurationPanel::updateAudioDevicesIn() 
{
  QComboBox* cbo = cboDriverChoiceIn;
  std::list< AudioDevice > audio = ConfigurationManager::instance().getAudioDevicesIn();
  std::list< AudioDevice >::iterator pos;
  cbo->clear();
  _deviceInMap.clear();

  int iPos = 0;
  for (pos = audio.begin(); pos != audio.end(); pos++) {
    QString hostApiName = pos->hostApiName;
    QString deviceName = pos->deviceName;
    
    if (hostApiName.length() > _cutStringCombo) {
	    hostApiName = hostApiName.left(_cutStringCombo) + "...";
    }
    QString name = hostApiName + QObject::tr(" (device #%1-%2Hz)").arg(pos->index).arg(pos->defaultRate);
    cbo->insertItem(name);
    _deviceInMap[iPos] = pos->index.toInt();
	iPos++;
  }
}

void ConfigurationPanel::updateAudioDevicesOut() 
{
  updateAudioDevices();
}

void ConfigurationPanel::updateAudioDevices()
{
  QComboBox* cbo = cboDriverChoiceOut;
  std::list< AudioDevice > audio = ConfigurationManager::instance().getAudioDevicesOut();
  std::list< AudioDevice >::iterator pos;
  cbo->clear();
  _deviceOutMap.clear();
 
  int iPos = 0;
  for (pos = audio.begin(); pos != audio.end(); pos++) {
    QString hostApiName = pos->hostApiName;
    QString deviceName = pos->deviceName;
    
    if (hostApiName.length() > _cutStringCombo) {
	    hostApiName = hostApiName.left(_cutStringCombo) + "...";
    }
    //DebugOutput::instance() << hostApiName << pos->defaultRate;
    QString name = hostApiName + QObject::tr(" (device #%1-%2Hz)").arg(pos->index).arg(pos->defaultRate);
    cbo->insertItem(name);
    _deviceOutMap[iPos] = pos->index.toInt();
	iPos++;
  }
}


void 
ConfigurationPanel::SkinChoice_selected( const QString & )
{

}

void
ConfigurationPanel::slotSIPRegister()
{
  saveSIPAccount(cboSIPAccount->currentItem());
  emit needRegister("SIP" + QString::number(cboSIPAccount->currentItem()));
}

void
ConfigurationPanel::slotIAXRegister()
{
  saveIAXAccount(cboIAXAccount->currentItem());
  emit needRegister("IAX" + QString::number(cboIAXAccount->currentItem()));
}

void 
ConfigurationPanel::slotRegisterReturn( bool hasError, QString ) 
{
  // here we check the current page...
  if (hasError) {
    lblError->setPaletteForegroundColor(red); // red
    lblError->setText(QObject::tr("Register failed"));
    lblIAXError->setPaletteForegroundColor(red); // red
    lblIAXError->setText(QObject::tr("Register failed"));
  } else {
    lblError->setPaletteForegroundColor(black); // black
    lblError->setText(QObject::tr("Register Succeed"));
    lblIAXError->setPaletteForegroundColor(black); // black
    lblIAXError->setText(QObject::tr("Register Succeed"));
  }
  lblError->show();
  lblIAXError->show();
}

/**
 * Test sound driver (save them before)
 */
void 
ConfigurationPanel::slotTestSoundDriver()
{
  // save driver in configuration manager
  int cboPosition = 0;
  int deviceIndex = 0;
  if (cboDriverChoiceOut->currentText() != NULL) {
    cboPosition = cboDriverChoiceOut->currentItem();
    deviceIndex = _deviceOutMap[cboPosition]; // return 0 if not found and create it, by STL
    ConfigurationManager::instance().set(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEOUT, QString::number(deviceIndex));
  }
  if (cboDriverChoiceIn->currentText() != NULL) {
    cboPosition = cboDriverChoiceIn->currentItem();
    deviceIndex = _deviceInMap[cboPosition]; // return 0 if not found and create it, by STL
    ConfigurationManager::instance().set(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEIN, QString::number(deviceIndex));
  }
  if (cboDriverRate->currentText() != NULL) {
    ConfigurationManager::instance().set(AUDIO_SECTION, AUDIO_SAMPLERATE, cboDriverRate->currentText());
  }

  // save driver on portaudio
  ConfigurationManager::instance().save(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEOUT);
  ConfigurationManager::instance().save(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEIN);
  ConfigurationManager::instance().save(AUDIO_SECTION, AUDIO_SAMPLERATE);
  emit soundDriverChanged();
}

void 
ConfigurationPanel::slotReloadSoundDriver()
{
  ConfigurationManager::instance().reloadSoundDriver();
}


void 
ConfigurationPanel::slotSoundDriverReturn( bool hasError, QString message ) 
{
  if (hasError) { // no error
    lblSoundDriver->setPaletteForegroundColor(red); // error in red
  } else {
    lblSoundDriver->setPaletteForegroundColor(black);
  }
  lblSoundDriver->setText(message);
  lblSoundDriver->show();
}

void 
ConfigurationPanel::slotSIPAccountChange(int index) 
{
  if (lastSIPAccount!=index) {
    
    QString account = "SIP" + QString::number(index);
    //DebugOutput::instance() << "Selecting SIP account " << account << "\n";

    saveSIPAccount(lastSIPAccount);
    loadSIPAccount(index);
    lblError->setText("");
  }
}

void
ConfigurationPanel::loadSIPAccount(int number) 
{
  QString account = "SIP" + QString::number(number);
  QString type    = ConfigurationManager::instance().get(account, ACCOUNT_TYPE);

  chkAutoregister->setChecked(ConfigurationManager::instance()
			   .get(account,ACCOUNT_AUTO_REGISTER).toUInt());

  chkEnable->setChecked(ConfigurationManager::instance()
			   .get(account,ACCOUNT_ENABLE).toUInt());

  QString aliasName = ConfigurationManager::instance().get(account, ACCOUNT_ALIAS);
  alias->setText(aliasName);
  // void QComboBox::changeItem ( const QString & t, int index);
  // 
	  
  fullName->setText(ConfigurationManager::instance()
		    .get(account,SIGNALISATION_FULL_NAME));
  userPart->setText(ConfigurationManager::instance()
		    .get(account,SIGNALISATION_USER_PART));
  username->setText(ConfigurationManager::instance()
		    .get(account,SIGNALISATION_AUTH_USER_NAME));
  password->setText(ConfigurationManager::instance()
		    .get(account,SIGNALISATION_PASSWORD));
  hostPart->setText(ConfigurationManager::instance()
		    .get(account,SIGNALISATION_HOST_PART));
  sipproxy->setText(ConfigurationManager::instance()
		    .get(account,SIGNALISATION_PROXY));
  STUNserver->setText(ConfigurationManager::instance()
		      .get(account,SIGNALISATION_STUN_SERVER));
  ((QRadioButton*)stunButtonGroup->find(ConfigurationManager::instance()
			.get(account,SIGNALISATION_USE_STUN).toUInt()))->setChecked(true); 
  lastSIPAccount = number;
}

void
ConfigurationPanel::saveSIPAccount(int number)
{
  QString account = "SIP" + QString::number(number);
  QString aliasName = alias->text();
  ConfigurationManager::instance().set(account, ACCOUNT_ALIAS, aliasName);
  QString accountName;
  if (aliasName.isEmpty()) {
    accountName = QObject::tr("SIP Account #%1").arg(number+1);
  } else {
    if (aliasName.length() > 30) {
	    aliasName = aliasName.left(30) + "...";
    }
     accountName = aliasName + " (" + QObject::tr("SIP Account #%1").arg(number+1) + ")";
  }
  cboSIPAccount->changeItem(accountName, number); 
  ConfigurationManager::instance().set(account, 
				       SIGNALISATION_FULL_NAME,
				       fullName->text());
  ConfigurationManager::instance().set(account, 
				       SIGNALISATION_USER_PART,
				       userPart->text());
  ConfigurationManager::instance().set(account, 
				       SIGNALISATION_AUTH_USER_NAME,
				       username->text());
  ConfigurationManager::instance().set(account, 
				       SIGNALISATION_PASSWORD,
				       password->text());
  ConfigurationManager::instance().set(account, 
				       SIGNALISATION_HOST_PART,
				       hostPart->text());
  ConfigurationManager::instance().set(account, 
				       SIGNALISATION_PROXY,
				       sipproxy->text());
  ConfigurationManager::instance().set(account, 
				       ACCOUNT_AUTO_REGISTER,
				       QString::number(chkAutoregister->isChecked()));
  ConfigurationManager::instance().set(account, 
				       ACCOUNT_ENABLE,
				       QString::number(chkEnable->isChecked()));
  ConfigurationManager::instance().set(account, 
				       SIGNALISATION_STUN_SERVER,
				       STUNserver->text());
}

void 
ConfigurationPanel::slotIAXAccountChange(int index) 
{
  if (lastIAXAccount!=index) {
    
    QString account = "IAX" + QString::number(index);

    saveIAXAccount(lastIAXAccount);
    loadIAXAccount(index);
    lblIAXError->setText("");
  }
}

void
ConfigurationPanel::loadIAXAccount(int number) 
{
  QString account = "IAX" + QString::number(number);
  QString type    = ConfigurationManager::instance().get(account, ACCOUNT_TYPE);

  chkIAXAutoregister->setChecked(ConfigurationManager::instance()
			   .get(account,ACCOUNT_AUTO_REGISTER).toUInt());

  chkIAXEnable->setChecked(ConfigurationManager::instance()
			   .get(account,ACCOUNT_ENABLE).toUInt());

  QString aliasName = ConfigurationManager::instance().get(account, ACCOUNT_ALIAS);
  IAXalias->setText(aliasName);
	  
  IAXuser->setText(ConfigurationManager::instance().get(account,SIGNALISATION_IAXUSER));
  IAXpass->setText(ConfigurationManager::instance().get(account,SIGNALISATION_IAXPASS));
  IAXhost->setText(ConfigurationManager::instance().get(account,SIGNALISATION_IAXHOST));
  lastIAXAccount = number;
}

void
ConfigurationPanel::saveIAXAccount(int number)
{
  QString account = "IAX" + QString::number(number);
  QString aliasName = IAXalias->text();
  ConfigurationManager::instance().set(account, ACCOUNT_ALIAS, aliasName);
  QString accountName;
  if (aliasName.isEmpty()) {
    accountName = QObject::tr("IAX Account #%1").arg(number+1);
  } else {
    if (aliasName.length() > 30) {
	    aliasName = aliasName.left(30) + "...";
    }
     accountName = aliasName + " (" + QObject::tr("IAX Account #%1").arg(number+1) + ")";
  }
  cboIAXAccount->changeItem(accountName, number); 
  ConfigurationManager::instance().set(account, SIGNALISATION_IAXUSER, IAXuser->text());
  ConfigurationManager::instance().set(account, SIGNALISATION_IAXPASS, IAXpass->text());
  ConfigurationManager::instance().set(account, SIGNALISATION_IAXHOST, IAXhost->text());
  ConfigurationManager::instance().set(account, 
				       ACCOUNT_AUTO_REGISTER,
				       QString::number(chkAutoregister->isChecked()));
  ConfigurationManager::instance().set(account, 
				       ACCOUNT_ENABLE,
				       QString::number(chkEnable->isChecked()));
}
