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

#include "configuration.h"
#include "qjlistboxpixmap.h"
#include "qmessagebox.h"
#include "skin.h"
#include "qtGUImainwindow.h"


void ConfigurationPanel::init()
{
    // For reading settings at application startup

     // List skin choice from "skins" directory
   QDir dir(Skin::getPath(QString(SKINDIR)));
   if ( !dir.exists() ) {
        printf ("\nCannot find 'skins' directory\n");
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
        printf ("\nCannot find 'rings' directory\n");
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
  // For signalisations tab
   fullName->setText(QString(Config::getchar("Signalisations", "SIP.fullName", "")));
   userPart->setText(QString(Config::getchar("Signalisations", "SIP.userPart", "")));
   username->setText(QString(Config::getchar("Signalisations", "SIP.username", "")));
   password->setText(QString(Config::getchar("Signalisations", "SIP.password", "")));
   hostPart->setText(QString(Config::getchar("Signalisations", "SIP.hostPart", "")));
   sipproxy->setText(QString(Config::getchar("Signalisations", "SIP.sipproxy", "")));
       autoregister->setChecked(Config::get("Signalisations", "SIP.autoregister", (int)true));
   playTones->setChecked(Config::get("Signalisations", "DTMF.playTones", (int)true));
   pulseLength->setValue(Config::get("Signalisations", "DTMF.pulseLength", 250));
   sendDTMFas->setCurrentItem(Config::get("Signalisations", "DTMF.sendDTMFas",0));
   STUNserver->setText(Config::get("Signalisations", "STUN.STUNserver", 
      "stun.fwdnet.net:3478"));
((QRadioButton*)stunButtonGroup->find(Config::get("Signalisations", "STUN.useStun", 1)))->setChecked(true);
   // For audio tab
((QRadioButton*)DriverChoice->find(Config::get("Audio", "Drivers.driverName", 0)))->setChecked(true);

#ifdef ALSA
   alsaButton->setEnabled(true);
#else
   alsaButton->setEnabled(false);
#endif

   codec1->setCurrentText(QString(Config::getchar("Audio", "Codecs.codec1", "G711u")));
   codec2->setCurrentText(QString(Config::getchar("Audio", "Codecs.codec2", "G711u")));
   codec3->setCurrentText(QString(Config::getchar("Audio", "Codecs.codec3", "G711u")));
   codec4->setCurrentText(QString(Config::getchar("Audio", "Codecs.codec4", "G711u")));
   codec5->setCurrentText(QString(Config::getchar("Audio", "Codecs.codec5", "G711u")));
     ringsChoice->setCurrentText(QString(Config::getchar(
      "Audio", "Rings.ringChoice", "konga.ul")));
   
   // For preferences tab
   SkinChoice->setCurrentText(QString(Config::getchar(
      "Preferences", "Themes.skinChoice", "metal")));
   confirmationToQuit->setChecked(Config::get(
      "Preferences", "Options.confirmQuit", (int)true));
     zoneToneChoice->setCurrentText(QString(Config::getchar(
     "Preferences", "Options.zoneToneChoice", "North America")));
     checkedTray->setChecked(Config::get(
     "Preferences", "Options.checkedTray", (int)false));
 
  voicemailNumber->setText(Config::get("Preferences", "Themes.voicemailNumber", "888"));
  
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
   Config::set("Signalisations", "SIP.fullName", fullName->text());
   Config::set("Signalisations", "SIP.userPart", userPart->text());
   Config::set("Signalisations", "SIP.username", username->text());
   Config::set("Signalisations", "SIP.password", password->text());
   Config::set("Signalisations", "SIP.hostPart", hostPart->text());
   Config::set("Signalisations", "SIP.sipproxy", sipproxy->text());
      Config::set("Signalisations", "SIP.autoregister",autoregister->isChecked());
   Config::set("Signalisations", "DTMF.pulseLength",  pulseLength->value());
   Config::set("Signalisations", "DTMF.playTones",  playTones->isChecked());
   Config::set("Signalisations", "DTMF.sendDTMFas" , sendDTMFas->currentItem());
   Config::set("Signalisations", "STUN.STUNserver", STUNserver->text());
 
   Config::set("Audio", "Codecs.codec1", codec1->currentText());
   Config::set("Audio", "Codecs.codec2", codec2->currentText());
   Config::set("Audio", "Codecs.codec3", codec3->currentText());
   Config::set("Audio", "Codecs.codec4", codec4->currentText());
   Config::set("Audio", "Codecs.codec5", codec5->currentText());
   if (ringsChoice->currentText() != NULL)
     Config::set("Audio", "Rings.ringChoice", ringsChoice->currentText());
   
   Config::set("Preferences", "Themes.skinChoice", SkinChoice->currentText());
   Config::set("Preferences", "Options.zoneToneChoice", 
     zoneToneChoice->currentText());
   Config::set("Preferences", "Options.confirmQuit", 
     confirmationToQuit->isChecked());
   Config::set("Preferences", "Options.checkedTray", checkedTray->isChecked());

   Config::set("Preferences", "Options.voicemailNumber", voicemailNumber->text());   
#if 0 
   QMessageBox::information(this, "Save settings",
   "You must restart SFLPhone",
    QMessageBox::Yes);
#endif
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


void ConfigurationPanel::useStunSlot(int id)
{
    Config::set("Signalisations", "STUN.useStun", id);
}


void ConfigurationPanel::applySkinSlot()
{
 Config::set("Preferences", "Themes.skinChoice", SkinChoice->currentText());
}


void ConfigurationPanel::driverSlot(int id)
{
Config::set("Audio", "Drivers.driverName", id);
}
