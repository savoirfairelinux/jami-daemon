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

#define SIGNALISATIONS_IMAGE "signalisations.png"
#define AUDIO_IMAGE "audio.png"
#define PREFERENCES_IMAGE "preferences.png"
#define ABOUT_IMAGE "about.png"


void ConfigurationPanel::init()
{
  DebugOutput::instance() << "ConfigurationPanel::init()\n"; 
    lblError->hide();
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
  codec1->setCurrentText(ConfigurationManager::instance()
			 .get(AUDIO_SECTION, AUDIO_CODEC1));
  codec2->setCurrentText(ConfigurationManager::instance()
			 .get(AUDIO_SECTION, AUDIO_CODEC2));
  codec3->setCurrentText(ConfigurationManager::instance()
			 .get(AUDIO_SECTION, AUDIO_CODEC3));


  ringsChoice->setCurrentText(ConfigurationManager::instance().get(AUDIO_SECTION,
				   AUDIO_RINGTONE));
  
  QString account = ACCOUNT_DEFAULT_NAME;
  QString type    = ConfigurationManager::instance().get(account, ACCOUNT_TYPE);
  QString active  = ConfigurationManager::instance().get(account, ACCOUNT_ENABLE);

  // For signalisations tab
  autoregister->setChecked(ConfigurationManager::instance()
			   .get(account,ACCOUNT_AUTO_REGISTER).toUInt());

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

  sendDTMFas->setCurrentItem(ConfigurationManager::instance()
			     .get(SIGNALISATION_SECTION,
				  SIGNALISATION_SEND_DTMF_AS).toUInt());
  playTones->setChecked(ConfigurationManager::instance()
			.get(SIGNALISATION_SECTION, 
			     SIGNALISATION_PLAY_TONES).toUInt());
  pulseLength->setValue(ConfigurationManager::instance()
			.get(SIGNALISATION_SECTION, 
			     SIGNALISATION_PULSE_LENGTH).toUInt());

  QRadioButton* device = 
    static_cast< QRadioButton * >(DriverChoice->find(ConfigurationManager::instance()
						     .get(AUDIO_SECTION, 
							  AUDIO_DEFAULT_DEVICEOUT).toUInt()));
  if(device) {
    device->setChecked(true);
  }

  device = static_cast< QRadioButton * >(DriverChoiceIn->find(ConfigurationManager::instance()
	.get(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEIN).toUInt()));
  if (device) {
    device->setChecked(true);
  }

  //preference tab
  updateSkins();
}

// For saving settings at application 'save'
void ConfigurationPanel::saveSlot()
{
  QString account = ACCOUNT_DEFAULT_NAME;
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
				       QString::number(autoregister->isChecked()));
  ConfigurationManager::instance().set(account, 
				       SIGNALISATION_STUN_SERVER,
				       STUNserver->text());

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


void ConfigurationPanel::driverSlot(int id)
{
  ConfigurationManager::instance().set(AUDIO_SECTION, 
				       AUDIO_DEFAULT_DEVICEOUT, 
				       QString::number(id));
  lblSoundDriver->setPaletteForegroundColor(black);
  lblSoundDriver->setText(tr("Not tested"));
}

void ConfigurationPanel::driverSlotIn(int id)
{
  ConfigurationManager::instance().set(AUDIO_SECTION, 
				       AUDIO_DEFAULT_DEVICEIN, 
				       QString::number(id));
  lblSoundDriver->setPaletteForegroundColor(black);
  lblSoundDriver->setText(tr("Not tested"));
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
  static std::list< QRadioButton * > buttonsIn;
  while(buttonsIn.begin() != buttonsIn.end()) {
    DriverChoiceIn->remove(*buttonsIn.begin());
    buttonsIn.pop_front();
  }
  int top = 0;

  //In
  std::list< AudioDevice > audio = ConfigurationManager::instance().getAudioDevicesIn();
  std::list< AudioDevice >::iterator pos;

  for (pos = audio.begin(); pos != audio.end(); pos++) {
    QString hostApiName = pos->hostApiName;
    QString deviceName = pos->deviceName;
    
    QString name = hostApiName + 
      QObject::tr(" (device #%1)").arg(pos->index);
    
    // New radio button with found device name
    QRadioButton* device = new QRadioButton(DriverChoiceIn);
    buttonsIn.push_back(device);
    DriverChoiceIn->insert(device, pos->index.toUInt());
    //device->setGeometry( QRect( 10, 30 + top, 360, 21 ) );
    // Set label of radio button
    //device->setText(deviceName);
    // Add tooltip for each one
    QString text = deviceName + " " + name;
    
    if(text.length() > 50) {
      device->setText(text.left(50) + "...");
    }
    else {
      device->setText(text);
    }
    
    QToolTip::add(device, text);
    top += 30;
  }
  // Set position of the button group, with appropriate length
  //DriverChoiceIn->setGeometry( QRect( 10, 200, 410, top + 30 ) );
}

void ConfigurationPanel::updateAudioDevicesOut() 
{
  updateAudioDevices();
}

void ConfigurationPanel::updateAudioDevices()
{
  static std::list< QRadioButton * > buttons;

  while(buttons.begin() != buttons.end()) {
    DriverChoice->remove(*buttons.begin());
    buttons.pop_front();
  }

  int top = 0;
  std::list< AudioDevice > audio = ConfigurationManager::instance().getAudioDevicesOut();
  std::list< AudioDevice >::iterator pos;
  
  for (pos = audio.begin(); pos != audio.end(); pos++) {
    QString hostApiName = pos->hostApiName;
    QString deviceName = pos->deviceName;
    
    QString name = hostApiName + 
      QObject::tr(" (device #%1)").arg(pos->index);
    
    // New radio button with found device name
    QRadioButton* device = new QRadioButton(DriverChoice); 
    buttons.push_back(device);
    DriverChoice->insert(device, pos->index.toUInt());
    //device->setGeometry( QRect( 10, 30 + top, 360, 21 ) );
    // Set label of radio button
    //device->setText(deviceName);
    // Add tooltip for each one
    QString text = deviceName + " " + name;
    
    if(text.length() > 50) {
      device->setText(text.left(50) + "...");
    }
    else {
      device->setText(text);
    }
    
    QToolTip::add(device, text);
    
    
    top += 30;
  }
  
  // Set position of the button group, with appropriate length
  //DriverChoice->setGeometry( QRect( 10, 10, 410, top + 30 ) );

}


void 
ConfigurationPanel::SkinChoice_selected( const QString & )
{

}

void
ConfigurationPanel::slotRegister()
{
  saveSlot();
  emit needRegister();
}

void 
ConfigurationPanel::slotRegisterReturn( bool hasError, QString ) 
{
  if (hasError) {
    lblError->setPaletteForegroundColor(red); // red
    lblError->setText("Register failed");
  } else {
    lblError->setPaletteForegroundColor(black); // black
    lblError->setText("Register Succeed");
  }
  lblError->show();
}

void ConfigurationPanel::slotTestSoundDriver()
{
  ConfigurationManager::instance().save(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEOUT);
  ConfigurationManager::instance().save(AUDIO_SECTION, AUDIO_DEFAULT_DEVICEIN);
  emit soundDriverChanged();
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

