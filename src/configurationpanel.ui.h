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
#include <qsettings.h>
#include <qstringlist.h>

#include "configuration.h"
#include "qjlistboxpixmap.h"
#include "qmessagebox.h"
#include "skin.h"
#include "qtGUImainwindow.h"

void ConfigurationPanel::init()
{
    // For reading settings at application startup
  QSettings settings;  

     // List skin choice from "skins" directory
   QDir dir(Skin::getPath(QString(SKINDIR)));
   if ( !dir.exists() ) {
        printf ("Cannot find skins directory\n");
    }
   dir.setFilter( QDir::Dirs | QDir::NoSymLinks);
   dir.setSorting( QDir::Name );
    
   QStringList list;
   list = dir.entryList();
   for (unsigned int i = 0; i < dir.count(); i++) {
       if (list[i] != "." && list[i] != ".." && list[i] != "CVS") {
      SkinChoice->insertItem(list[i]);
       }
   } 


   // For signalisations tab
   fullName->setText(
    Config::get(QString("Signalisations/SIP.fullName"), QString("")));
   userPart->setText(
    Config::get(QString("Signalisations/SIP.userPart"), QString("")));
   username->setText(
    Config::get(QString("Signalisations/SIP.username"), QString("")));
   password->setText(
    Config::get(QString("Signalisations/SIP.password"), QString("")));
   hostPart->setText(
    Config::get(QString("Signalisations/SIP.hostPart"), QString("")));
   sipproxy->setText(
    Config::get(QString("Signalisations/SIP.sipproxy"), QString("")));
   playTones->setChecked(
    Config::get(QString("Signalisations/DTMF.playTones"), true));
   pulseLength->setValue(
    Config::get(QString("Signalisations/DTMF.pulseLength"),  250));
   sendDTMFas->setCurrentItem(
    Config::get(QString("Signalisations/DTMF.sendDTMFas"), 1));
STUNserver->setText(
    Config::get(QString("Signalisations/STUN.STUNserver"), 
		QString("stun.fwdnet.net:3478")));
useStunYes->setChecked(
    Config::get(QString("Signalisations/STUN.useStunYes"), false));
useStunNo->setChecked(
    Config::get(QString("Signalisations/STUN.useStunNo"), true));


   // For audio tab
   ossButton->setChecked(
    Config::get(QString("Audio/Drivers.driverOSS"), true));
   alsaButton->setChecked(
    Config::get(QString("Audio/Drivers.driverALSA"), false));
   
   codec1->setCurrentText(
    Config::get(QString("Audio/Codecs.codec1"), QString("G711u")));
   codec2->setCurrentText(
    Config::get(QString("Audio/Codecs.codec2"), QString("G711a")));
   codec3->setCurrentText(
    Config::get(QString("Audio/Codecs.codec3"), QString("GSM")));
   codec4->setCurrentText(
    Config::get(QString("Audio/Codecs.codec4"), QString("iLBC")));
   codec5->setCurrentText(
    Config::get(QString("Audio/Codecs.codec5"), QString("SPEEX")));
   
   // For preferences tab
   SkinChoice->setCurrentText(
    Config::get(QString("Preferences/Themes.skinChoice"), QString("metal")));
   confirmationToQuit->setChecked(
    Config::get(QString("Preferences/Options.confirmQuit"), true));
     zoneToneChoice->setCurrentText(
      Config::get(QString("Preferences/Options.zoneToneChoice"),
    QString("France")));
     checkedTray->setChecked(
     Config::get(QString("Preferences/Options.checkedTray"), false));
     autoregister->setChecked(
     Config::get(QString("Preferences/Options.autoregister"), true));
voicemailNumber->setText(
    Config::get(QString("Preferences/Themes.voicemailNumber"), QString("888")));
   
   //  Init tab view order
    Tab_Signalisations->show();
    Tab_Audio->hide();
    Tab_Video->hide();    
    Tab_Network->hide();
    Tab_Preferences->hide();
    Tab_About->hide();

    // Set items for QListBox
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
 QPixmap(Skin::getPathPixmap(QString(PIXDIR),QString(PIXMAP_SIGNALISATIONS))),  "Signalisation", Menu );
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
 QPixmap(Skin::getPathPixmap(QString(PIXDIR), QString(PIXMAP_AUDIO))) ,
 "Audio", Menu );
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
 QPixmap(Skin::getPathPixmap(QString(PIXDIR), QString(PIXMAP_VIDEO))),
 "Video", Menu );
    new QjListBoxPixmap (QjListBoxPixmap::Above, 
 QPixmap(Skin::getPathPixmap(QString(PIXDIR), QString(PIXMAP_NETWORK))),
 "Network", Menu );
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
 QSettings settings;
 
   Config::set("Signalisations/SIP.fullName", fullName->text());
   Config::set("Signalisations/SIP.userPart", userPart->text());
   Config::set("Signalisations/SIP.username", username->text());
   Config::set("Signalisations/SIP.password", password->text());
   Config::set("Signalisations/SIP.hostPart", hostPart->text());
   Config::set("Signalisations/SIP.sipproxy", sipproxy->text());
   Config::set("Signalisations/DTMF.pulseLength",  pulseLength->value());
   Config::set("Signalisations/DTMF.playTones",  playTones->isChecked());
   Config::set("Signalisations/DTMF.sendDTMFas",  sendDTMFas->currentItem());
   Config::set("Signalisations/STUN.STUNserver", STUNserver->text());
   Config::set("Signalisations/STUN.useStunYes", useStunYes->isChecked());
   Config::set("Signalisations/STUN.useStunNo", useStunNo->isChecked());
 
   Config::set("Audio/Drivers.driverOSS", ossButton->isChecked());
   Config::set("Audio/Drivers.driverALSA", alsaButton->isChecked());

   Config::set("Audio/Codecs.codec1", codec1->currentText());
   Config::set("Audio/Codecs.codec2", codec2->currentText());
   Config::set("Audio/Codecs.codec3", codec3->currentText());
   Config::set("Audio/Codecs.codec4", codec4->currentText());
   Config::set("Audio/Codecs.codec5", codec5->currentText());
   
   Config::set("Preferences/Themes.skinChoice", SkinChoice->currentText());
   Config::set("Preferences/Options.zoneToneChoice", zoneToneChoice->currentText());
   Config::set("Preferences/Options.confirmQuit", confirmationToQuit->isChecked());
   Config::set("Preferences/Options.checkedTray", checkedTray->isChecked());
   Config::set("Preferences/Options.autoregister", autoregister->isChecked());
   Config::set("Preferences/Options.voicemailNumber", voicemailNumber->text());
   
   QMessageBox::information(this, "Save settings",
   "You must restart SFLPhone",
    QMessageBox::Yes);
   accept();
}

// Handle tab view  according to current item of listbox
void ConfigurationPanel::changeTabSlot()
{
    switch (Menu->currentItem()) {
    case 0:
 TitleTab->setText("Setup signalisation");
 Tab_Signalisations->show();
              Tab_Audio->hide();
 Tab_Video->hide();
              Tab_Network->hide();
              Tab_Preferences->hide();
 Tab_About->hide();
              break;
    case 1:
 TitleTab->setText("Setup audio");
 Tab_Signalisations->hide();
              Tab_Audio->show();
 Tab_Video->hide();
              Tab_Network->hide();
 Tab_Preferences->hide();
              Tab_About->hide();
              break;
   case 2:
       TitleTab->setText("Setup video");
              Tab_Signalisations->hide();
              Tab_Audio->hide();
 Tab_Video->show();
              Tab_Network->hide();
 Tab_Preferences->hide();
              Tab_About->hide();
              break;
   case 3:
       TitleTab->setText("Setup network");
              Tab_Signalisations->hide();
              Tab_Audio->hide();
 Tab_Video->hide();
              Tab_Network->show();
 Tab_Preferences->hide();
              Tab_About->hide();
              break;
   case 4:
       TitleTab->setText("Setup preferences");
              Tab_Signalisations->hide();
              Tab_Audio->hide();
 Tab_Video->hide();
              Tab_Network->hide();
 Tab_Preferences->show();
              Tab_About->hide();
              break;
   case 5:
       TitleTab->setText("About");
              Tab_Signalisations->hide();
              Tab_Audio->hide();
 Tab_Video->hide();
              Tab_Network->hide();
 Tab_Preferences->hide();
              Tab_About->show();
              break;
    }
}


