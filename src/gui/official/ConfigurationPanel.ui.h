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

#include "globals.h"
#include "ConfigurationManager.hpp"
#include "DebugOutput.hpp"
#include "QjListBoxPixmap.hpp"
#include "TransparentWidget.hpp"

#define SIGNALISATIONS_IMAGE "signalisations.png"
#define AUDIO_IMAGE "audio.png"
#define PREFERENCES_IMAGE "preferences.png"
#define ABOUT_IMAGE "about.png"


void ConfigurationPanel::init()
{
  DebugOutput::instance() << "ConfigurationPanel::init()\n";
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

  /*
   // List ring choice from "rings" directory
   QDir ringdir(Skin::getPath(QString(RINGDIR)));
   if ( !ringdir.exists() ) {
        _debug ("\nCannot find 'rings' directory\n");
		return;
    } else {
    ringdir.setFilter( QDir::Files | QDir::NoSymLinks);
    ringdir.setSorting( QDir::Name );
  
    QStringList ringlist;
    ringlist = ringdir.entryList();
    for (unsigned int i = 0; i < ringdir.count(); i++) {
     if (ringlist[i] != "." && ringlist[i] != ".." && ringlist[i] != "CVS") {
    ringsChoice->insertItem(ringlist[i]);
     }
    } 
 }
  */
    
  /*
   ringsChoice->setCurrentText(QString(manager.getConfigString(AUDIO,
RING_CHOICE)));
  */
   
  /*
   // For preferences tab
   SkinChoice->setCurrentText(QString(manager.getConfigString(
               PREFERENCES, SKIN_CHOICE)));
   confirmationToQuit->setChecked(manager.getConfigInt(
               PREFERENCES, CONFIRM_QUIT));
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

  int top = 0;
  std::list< AudioDevice > audio = ConfigurationManager::instance().getAudioDevices();
  std::list< AudioDevice >::iterator pos;
    
  for (pos = audio.begin(); pos != audio.end(); pos++) {
    QString name = pos->description;

    // New radio button with found device name
    QRadioButton* device = new QRadioButton(DriverChoice); 
    device->setGeometry( QRect( 10, 30 + top, 390, 21 ) );
    // Set label of radio button
    device->setText(name);
      
    top += 30;
    if (ConfigurationManager::instance().get(AUDIO_SECTION, 
					     AUDIO_DEFAULT_DEVICE) == pos->index) {
      device->setChecked(true);   
    }
  }
  // Set position of the button group, with appropriate length
  DriverChoice->setGeometry( QRect( 10, 10, 410, top + 30 ) );


  // For signalisations tab
  fullName->setText(ConfigurationManager::instance()
		    .get(SIGNALISATION_SECTION, 
			 SIGNALISATION_FULL_NAME));
  userPart->setText(ConfigurationManager::instance()
		    .get(SIGNALISATION_SECTION,
			 SIGNALISATION_USER_PART));
  username->setText(ConfigurationManager::instance()
		    .get(SIGNALISATION_SECTION,
			 SIGNALISATION_AUTH_USER_NAME));
  password->setText(ConfigurationManager::instance()
		    .get(SIGNALISATION_SECTION, 
			 SIGNALISATION_PASSWORD));
  hostPart->setText(ConfigurationManager::instance()
		    .get(SIGNALISATION_SECTION,
			 SIGNALISATION_HOST_PART));
  sipproxy->setText(ConfigurationManager::instance()
		    .get(SIGNALISATION_SECTION, 
			 SIGNALISATION_PROXY));
  autoregister->setChecked(ConfigurationManager::instance()
			   .get(SIGNALISATION_SECTION,
				SIGNALISATION_AUTO_REGISTER));
  playTones->setChecked(ConfigurationManager::instance()
			.get(SIGNALISATION_SECTION, 
			     SIGNALISATION_PLAY_TONES).toUInt());
  pulseLength->setValue(ConfigurationManager::instance()
			.get(SIGNALISATION_SECTION, 
			     SIGNALISATION_PULSE_LENGTH).toUInt());
  sendDTMFas->setCurrentItem(ConfigurationManager::instance()
			     .get(SIGNALISATION_SECTION,
				  SIGNALISATION_SEND_DTMF_AS).toUInt());
  STUNserver->setText(ConfigurationManager::instance()
		      .get(SIGNALISATION_SECTION,
			   SIGNALISATION_STUN_SERVER));
  ((QRadioButton*)stunButtonGroup->find(ConfigurationManager::instance()
					.get(SIGNALISATION_SECTION,
					     SIGNALISATION_USE_STUN).toUInt()))->setChecked(true);



  /*
   ringsChoice->setCurrentText(QString(manager.getConfigString(AUDIO,
RING_CHOICE)));
  */

}

// For saving settings at application 'save'
void ConfigurationPanel::saveSlot()
{
  ConfigurationManager::instance().set("VoIPLink", "SIP.fullName",
				       fullName->text());
  ConfigurationManager::instance().set("VoIPLink", "SIP.userPart",
				       userPart->text());
  ConfigurationManager::instance().set("VoIPLink", "SIP.username",
				       username->text());
  ConfigurationManager::instance().set("VoIPLink", "SIP.password",
				       password->text());
  ConfigurationManager::instance().set("VoIPLink", "SIP.hostPart",
				       hostPart->text());
  ConfigurationManager::instance().set("VoIPLink", "SIP.proxy", 
				       sipproxy->text());
  ConfigurationManager::instance().set("VoIPLink", "SIP.autoregister", 
				       QString::number(autoregister->isChecked()));
  ConfigurationManager::instance().set("VoIPLink", "DTMF.pulseLength",  
				       QString::number(pulseLength->value()));
  ConfigurationManager::instance().set("VoIPLink", "DTMF.playTones",  
				       QString::number(playTones->isChecked()));
  ConfigurationManager::instance().set("VoIPLink", "DTMF.sendDTMFas" , 
				       QString::number(sendDTMFas->currentItem()));
  ConfigurationManager::instance().set("VoIPLink", "STUN.STUNserver",
				       STUNserver->text());

  ConfigurationManager::instance().set("Audio", "Codecs.codec1",
				       codec1->currentText());
  ConfigurationManager::instance().set("Audio", "Codecs.codec2",
				       codec2->currentText());
  ConfigurationManager::instance().set("Audio", "Codecs.codec3",
				       codec3->currentText());
  
  if (ringsChoice->currentText() != NULL)
    ConfigurationManager::instance().set("Audio", "Rings.ringChoice", 
		      ringsChoice->currentText());
  
  ConfigurationManager::instance().set("Preferences", "Themes.skinChoice", 
		    SkinChoice->currentText());
  ConfigurationManager::instance().set("Preferences", "Options.zoneToneChoice", 
		    zoneToneChoice->currentText());
  ConfigurationManager::instance().set("Preferences", "Options.confirmQuit", 
				       QString::number(confirmationToQuit->isChecked()));
  ConfigurationManager::instance().set("Preferences", "Options.checkedTray",
				       QString::number(checkedTray->isChecked()));
  
  ConfigurationManager::instance().set("Preferences", "Options.voicemailNumber", 
				       voicemailNumber->text());
#if 0 
  QMessageBox::information(this, "Save settings",
			   "You must restart SFLPhone",
			   QMessageBox::Yes);
#endif

  ConfigurationManager::instance().save();
}

// Handle tab view  according to current item of listbox
void ConfigurationPanel::changeTabSlot()
{
  switch (Menu->currentItem()) {
  case 0:
    TitleTab->setText("Setup signalisation");
    Tab_Signalisations->show();
    Tab_Audio->hide();
    Tab_Preferences->hide();
    Tab_About->hide();
    break;
  case 1:
    TitleTab->setText("Setup audio");
    Tab_Signalisations->hide();
    Tab_Audio->show();
    Tab_Preferences->hide();
    Tab_About->hide();
    break;
  case 2:
    TitleTab->setText("Setup preferences");
    Tab_Signalisations->hide();
    Tab_Audio->hide();
    Tab_Preferences->show();
    Tab_About->hide();
    break;
  case 3:
    TitleTab->setText("About");
    Tab_Signalisations->hide();
    Tab_Audio->hide();
    Tab_Preferences->hide();
    Tab_About->show();
    break;
  }
}


void ConfigurationPanel::useStunSlot(int)
{
  //Manager::instance().setConfig("VoIPLink", "STUN.useStun", id);
}


void ConfigurationPanel::applySkinSlot()
{
  //Manager::instance().setConfig("Preferences", "Themes.skinChoice",
  //string(SkinChoice->currentText().ascii()));
}


void ConfigurationPanel::driverSlot(int)
{
  //Manager::instance().setConfig("Audio", "Drivers.driverName", id);
}
