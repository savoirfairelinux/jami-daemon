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
#include <qstringlist.h>

#include "../../configuration.h"
#include "../../global.h"
#include "../../manager.h"
#include "../../skin.h"
#include "../../user_cfg.h"
#include "../../audio/audiolayer.h"
#include "qjlistboxpixmap.h"
#include "qmessagebox.h"
#include "qtGUImainwindow.h"

void ConfigurationPanel::init()
{
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
    // List audio devices 
    const char* devicename ; 
    const char* hostapiname;
 
    int top = 0;
    AudioDevice devStruct;
   
	portaudio::AutoSystem autoSys;
	// For each device
    for (int i = 0; i < Manager::instance().deviceCount(); i++) {
		// Fill the device structure
    	devStruct = Manager::instance().deviceList(i);
   		hostapiname = devStruct.hostApiName;
        devicename = devStruct.deviceName;
 
     	QString hostNameApi(hostapiname) ;
        QString name = hostNameApi + " (device #" + QString::number(i) + ")";

		// New radio button with found device name
     	QRadioButton* device = new QRadioButton(DriverChoice);   
     	device->setGeometry( QRect( 10, 30 + top, 390, 21 ) );
    
		// Set label of radio button
     	device->setText(devicename);
		
		// Add tooltip for each one
 		QToolTip::add(device , name );
     
        top += 30;
   		if (Manager::instance().defaultDevice(i)) {
         	device->setChecked(true);   
        }
    }
	// Set position of the button group, with appropriate length
 	DriverChoice->setGeometry( QRect( 10, 10, 410, top + 30 ) );
    
  // For signalisations tab
   fullName->setText(QString(get_config_fields_str(SIGNALISATION, FULL_NAME)));
   userPart->setText(QString(get_config_fields_str(SIGNALISATION, USER_PART)));
   username->setText(QString(get_config_fields_str(SIGNALISATION, AUTH_USER_NAME)));
   password->setText(QString(get_config_fields_str(SIGNALISATION, PASSWORD)));
   hostPart->setText(QString(get_config_fields_str(SIGNALISATION, HOST_PART)));
   sipproxy->setText(QString(get_config_fields_str(SIGNALISATION, PROXY)));
   autoregister->setChecked(get_config_fields_int(SIGNALISATION, AUTO_REGISTER));
   playTones->setChecked(get_config_fields_int(SIGNALISATION, PLAY_TONES));
   pulseLength->setValue(get_config_fields_int(SIGNALISATION, PULSE_LENGTH));
   sendDTMFas->setCurrentItem(get_config_fields_int(SIGNALISATION, SEND_DTMF_AS));
   STUNserver->setText(QString(get_config_fields_str(SIGNALISATION, STUN_SERVER)));
((QRadioButton*)stunButtonGroup->find(get_config_fields_int(SIGNALISATION, USE_STUN)))->setChecked(true);
   // For audio tab
  
((QRadioButton*)DriverChoice->find(get_config_fields_int(AUDIO, OUTPUT_DRIVER_NAME)))->setChecked(true);

   codec1->setCurrentText(QString(get_config_fields_str(AUDIO, CODEC1)));
   codec2->setCurrentText(QString(get_config_fields_str(AUDIO, CODEC2)));
   codec3->setCurrentText(QString(get_config_fields_str(AUDIO, CODEC3)));

   ringsChoice->setCurrentText(QString(get_config_fields_str(AUDIO, RING_CHOICE)));
   
   // For preferences tab
   SkinChoice->setCurrentText(QString(get_config_fields_str(
               PREFERENCES, SKIN_CHOICE)));
   confirmationToQuit->setChecked(get_config_fields_int(
               PREFERENCES, CONFIRM_QUIT));
     zoneToneChoice->setCurrentText(QString(get_config_fields_str(
             PREFERENCES, ZONE_TONE)));
     checkedTray->setChecked(get_config_fields_int(
               PREFERENCES, CHECKED_TRAY));
  voicemailNumber->setText(QString(get_config_fields_str(
               PREFERENCES, VOICEMAIL_NUM)));
  
   //  Init tab view order
    Tab_Signalisations->show();
    Tab_Audio->hide();
    Tab_Preferences->hide();
    Tab_About->hide();

    // Set items for QListBox
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
 QPixmap(Skin::getPathPixmap(QString(PIXDIR),QString(PIXMAP_SIGNALISATIONS))),  "Signalisation", Menu );
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
 QPixmap(Skin::getPathPixmap(QString(PIXDIR), QString(PIXMAP_AUDIO))) ,
 "Audio", Menu );
    
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
 QPixmap(Skin::getPathPixmap(QString(PIXDIR), QString(PIXMAP_PREFERENCES))),
 "Preferences", Menu );
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
 QPixmap(Skin::getPathPixmap(QString(PIXDIR), QString(PIXMAP_ABOUT))),
 "About", Menu );
}

// For saving settings at application 'save'
void ConfigurationPanel::saveSlot()
{ 
   Config::set("VoIPLink", "SIP.fullName", string(fullName->text().ascii()));
   Config::set("VoIPLink", "SIP.userPart", string(userPart->text().ascii()));
   Config::set("VoIPLink", "SIP.username", string(username->text().ascii()));
   Config::set("VoIPLink", "SIP.password", string(password->text().ascii()));
   Config::set("VoIPLink", "SIP.hostPart", string(hostPart->text().ascii()));
   Config::set("VoIPLink", "SIP.proxy", string(sipproxy->text().ascii()));
   Config::set("VoIPLink", "SIP.autoregister",autoregister->isChecked());
   Config::set("VoIPLink", "DTMF.pulseLength",  pulseLength->value());
   Config::set("VoIPLink", "DTMF.playTones",  playTones->isChecked());
   Config::set("VoIPLink", "DTMF.sendDTMFas" , sendDTMFas->currentItem());
   Config::set("VoIPLink", "STUN.STUNserver", string(STUNserver->text().ascii()));
 
   Config::set("Audio", "Codecs.codec1", string(codec1->currentText().ascii()));
   Config::set("Audio", "Codecs.codec2", string(codec2->currentText().ascii()));
   Config::set("Audio", "Codecs.codec3", string(codec3->currentText().ascii()));

   if (ringsChoice->currentText() != NULL)
     Config::set("Audio", "Rings.ringChoice", 
         string(ringsChoice->currentText().ascii()));
   
   Config::set("Preferences", "Themes.skinChoice", 
     string(SkinChoice->currentText().ascii()));
   Config::set("Preferences", "Options.zoneToneChoice", 
     string(zoneToneChoice->currentText().ascii()));
   Config::set("Preferences", "Options.confirmQuit", 
     confirmationToQuit->isChecked());
   Config::set("Preferences", "Options.checkedTray", checkedTray->isChecked());

   Config::set("Preferences", "Options.voicemailNumber", 
     string(voicemailNumber->text().ascii()));   
#if 0 
   QMessageBox::information(this, "Save settings",
   "You must restart SFLPhone",
    QMessageBox::Yes);
#endif
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


void ConfigurationPanel::useStunSlot(int id)
{
    Config::set("VoIPLink", "STUN.useStun", id);
}


void ConfigurationPanel::applySkinSlot()
{
 Config::set("Preferences", "Themes.skinChoice", string(SkinChoice->currentText().ascii()));
}


void ConfigurationPanel::driverSlot(int id)
{
Config::set("Audio", "Drivers.driverName", id);
}
