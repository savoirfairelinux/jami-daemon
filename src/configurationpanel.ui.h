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
   fullName->setText(QString(Config::getchar("Signalisations", "SIP.fullName", "")));
   userPart->setText(QString(Config::getchar("Signalisations", "SIP.userPart", "")));
   username->setText(QString(Config::getchar("Signalisations", "SIP.username", "")));
   password->setText(QString(Config::getchar("Signalisations", "SIP.password", "")));
   hostPart->setText(QString(Config::getchar("Signalisations", "SIP.hostPart", "")));
   sipproxy->setText(QString(Config::getchar("Signalisations", "SIP.sipproxy", "")));
   playTones->setChecked(Config::get("Signalisations", "DTMF.playTones", (int)true));
   pulseLength->setValue(Config::get("Signalisations", "DTMF.pulseLength", 250));
   sendDTMFas->setCurrentItem(Config::get("Signalisations", "DTMF.sendDTMFas",1));

   // For audio tab
   ossButton->setChecked(Config::get("Audio", "Drivers.driverOSS", (int)true));
   alsaButton->setChecked(Config::get("Audio", "Drivers.driverALSA", (int)false));
   codec1->setCurrentText(QString(Config::getchar("Audio", "Codecs.codec1", "G711u")));
   codec2->setCurrentText(QString(Config::getchar("Audio", "Codecs.codec2", "G711a")));
   codec3->setCurrentText(QString(Config::getchar("Audio", "Codecs.codec3", "GSM")));
   codec4->setCurrentText(QString(Config::getchar("Audio", "Codecs.codec4", "iLBC")));
   codec5->setCurrentText(QString(Config::getchar("Audio", "Codecs.codec5", "SPEEX")));
   
   // For preferences tab
   SkinChoice->setCurrentText(QString(Config::getchar(
			   "Preferences", "Themes.skinChoice", "metal")));
   confirmationToQuit->setChecked(Config::get(
			   "Preferences", "Options.confirmQuit", (int)true));
     zoneToneChoice->setCurrentText(QString(Config::getchar(
				 "Preferences", "Options.zoneToneChoice", "North America")));
     checkedTray->setChecked(Config::get(
				 "Preferences", "Options.checkedTray", (int)false));
     autoregister->setChecked(Config::get(
				 "Preferences", "Options.autoregister", (int)true));
	 
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
   Config::set("Signalisations", "DTMF.pulseLength",  pulseLength->value());
   Config::set("Signalisations", "DTMF.playTones",  playTones->isChecked());
   Config::set("Signalisations", "DTMF.sendDTMFas" , sendDTMFas->currentItem());
 
   Config::set("Audio", "Drivers.driverOSS", ossButton->isChecked());
   Config::set("Audio", "Drivers.driverALSA", alsaButton->isChecked());

   Config::set("Audio", "Codecs.codec1", codec1->currentText());
   Config::set("Audio", "Codecs.codec2", codec2->currentText());
   Config::set("Audio", "Codecs.codec3", codec3->currentText());
   Config::set("Audio", "Codecs.codec4", codec4->currentText());
   Config::set("Audio", "Codecs.codec5", codec5->currentText());
   
   Config::set("Preferences", "Themes.skinChoice", SkinChoice->currentText());
   Config::set("Preferences", "Options.zoneToneChoice", 
		   zoneToneChoice->currentText());
   Config::set("Preferences", "Options.confirmQuit", 
		   confirmationToQuit->isChecked());
   Config::set("Preferences", "Options.checkedTray", checkedTray->isChecked());
   Config::set("Preferences", "Options.autoregister",autoregister->isChecked());
   
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


