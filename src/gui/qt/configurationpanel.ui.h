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

#include "../../global.h"
#include "../../manager.h"
#include "skin.h"
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
    
  ManagerImpl& manager = Manager::instance();
  // For signalisations tab
  
fullName->setText(QString(manager.getConfigString(
SIGNALISATION , FULL_NAME)));
   userPart->setText(QString(manager.getConfigString(SIGNALISATION,
USER_PART)));
   username->setText(QString(manager.getConfigString(SIGNALISATION,
AUTH_USER_NAME)));
   password->setText(QString(manager.getConfigString(SIGNALISATION, PASSWORD)));
   hostPart->setText(QString(manager.getConfigString(SIGNALISATION,
HOST_PART)));
   sipproxy->setText(QString(manager.getConfigString(SIGNALISATION, PROXY)));
   autoregister->setChecked(manager.getConfigInt(SIGNALISATION,
AUTO_REGISTER));
   playTones->setChecked(manager.getConfigInt(SIGNALISATION, PLAY_TONES));
   pulseLength->setValue(manager.getConfigInt(SIGNALISATION, PULSE_LENGTH));
   sendDTMFas->setCurrentItem(manager.getConfigInt(SIGNALISATION,
SEND_DTMF_AS));
   STUNserver->setText(QString(manager.getConfigString(SIGNALISATION,
STUN_SERVER)));
((QRadioButton*)stunButtonGroup->find(manager.getConfigInt(SIGNALISATION,
USE_STUN)))->setChecked(true);
   // For audio tab
  
((QRadioButton*)DriverChoice->find(manager.getConfigInt(AUDIO,
DRIVER_NAME)))->setChecked(true);

   codec1->setCurrentText(QString(manager.getConfigString(AUDIO, CODEC1)));
   codec2->setCurrentText(QString(manager.getConfigString(AUDIO, CODEC2)));
   codec3->setCurrentText(QString(manager.getConfigString(AUDIO, CODEC3)));

   ringsChoice->setCurrentText(QString(manager.getConfigString(AUDIO,
RING_CHOICE)));
   
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
  ManagerImpl& manager = Manager::instance();
  manager.setConfig("VoIPLink", "SIP.fullName",
string(fullName->text().ascii()));
  manager.setConfig("VoIPLink", "SIP.userPart",
string(userPart->text().ascii()));
  manager.setConfig("VoIPLink", "SIP.username",
string(username->text().ascii()));
  manager.setConfig("VoIPLink", "SIP.password",
string(password->text().ascii()));
  manager.setConfig("VoIPLink", "SIP.hostPart",
string(hostPart->text().ascii()));
  manager.setConfig("VoIPLink", "SIP.proxy", string(sipproxy->text().ascii()));
  manager.setConfig("VoIPLink", "SIP.autoregister", autoregister->isChecked());
  manager.setConfig("VoIPLink", "DTMF.pulseLength",  pulseLength->value());
  manager.setConfig("VoIPLink", "DTMF.playTones",  playTones->isChecked());
  manager.setConfig("VoIPLink", "DTMF.sendDTMFas" , sendDTMFas->currentItem());
  manager.setConfig("VoIPLink", "STUN.STUNserver",
string(STUNserver->text().ascii()));

  manager.setConfig("Audio", "Codecs.codec1",
string(codec1->currentText().ascii()));
  manager.setConfig("Audio", "Codecs.codec2",
string(codec2->currentText().ascii()));
  manager.setConfig("Audio", "Codecs.codec3",
string(codec3->currentText().ascii()));

  if (ringsChoice->currentText() != NULL)
    manager.setConfig("Audio", "Rings.ringChoice", 
         string(ringsChoice->currentText().ascii()));

  manager.setConfig("Preferences", "Themes.skinChoice", 
    string(SkinChoice->currentText().ascii()));
  manager.setConfig("Preferences", "Options.zoneToneChoice", 
    string(zoneToneChoice->currentText().ascii()));
  manager.setConfig("Preferences", "Options.confirmQuit", 
    confirmationToQuit->isChecked());
  manager.setConfig("Preferences", "Options.checkedTray",
checkedTray->isChecked());

  manager.setConfig("Preferences", "Options.voicemailNumber", 
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
  Manager::instance().setConfig("VoIPLink", "STUN.useStun", id);
}


void ConfigurationPanel::applySkinSlot()
{
 Manager::instance().setConfig("Preferences", "Themes.skinChoice",
string(SkinChoice->currentText().ascii()));
}


void ConfigurationPanel::driverSlot(int id)
{
  Manager::instance().setConfig("Audio", "Drivers.driverName", id);
}
