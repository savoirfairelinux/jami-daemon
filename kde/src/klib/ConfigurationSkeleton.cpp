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

#include "ConfigurationSkeleton.h"

#include "../lib/configurationmanager_interface_singleton.h"
#include "../lib/sflphone_const.h"

//KDE
#include <KDebug>

ConfigurationSkeleton * ConfigurationSkeleton::instance = NULL;

///Constructor
ConfigurationSkeleton::ConfigurationSkeleton()
 : ConfigurationSkeletonBase()
{
   kDebug() << "Building ConfigurationSkeleton";
   //codecListModel = new CodecListModel();
   readConfig();
}

///Destructor
ConfigurationSkeleton::~ConfigurationSkeleton()
{
}

///Signleton
ConfigurationSkeleton * ConfigurationSkeleton::self()
{
   if(instance == NULL)
   {   instance = new ConfigurationSkeleton();   }
   return instance;
}

///Read the config and override some variable using deamon defaults
void ConfigurationSkeleton::readConfig()
{
   //ConfigurationSkeleton::readConfig();
   kDebug() << "Reading config";

   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();

   //General settings

   //Call history settings
   //setEnableHistory(true);
   setHistoryMax(configurationManager.getHistoryLimit());

   //Display settings

   //Notification settings
   //setNotifOnCalls(true);
   setNotifOnMessages(configurationManager.getMailNotify());

   //Window display settings
   //setDisplayOnStart(true);
   //setDisplayOnCalls(true);

   //Accounts settings

   //Audio settings

   //Audio Interface settings
   QString audioManager = configurationManager.getAudioManager();
   kDebug() << "audioManager = " << audioManager;
   //setInterface(audioManager); //TODO

//    //ringtones settings
//    setEnableRingtones(true);
//    //QString ringtone = "";
//    if(ringtone().isEmpty()) {
//       setRingtone(QString(SHARE_INSTALL_PREFIX) + "/sflphone/ringtones/konga.ul");
//    }

   //codecs settings
   //setActiveCodecList(configurationManager.getActiveCodecList()); //Outdated

   kDebug() << "configurationManager.getCurrentAudioOutputPlugin() = " << configurationManager.getCurrentAudioOutputPlugin();
   setAlsaPlugin(configurationManager.getCurrentAudioOutputPlugin());
   bool ok;
   QStringList devices = configurationManager.getCurrentAudioDevicesIndex();
   int inputDevice =0;
   if (devices.size() > 1) {
      kDebug() << "inputDevice = " << devices[1];
      inputDevice = devices[1].toInt(& ok);
   }
   else
      kDebug() << "Fatal: Too few audio devices";

   if(!ok)
      kDebug() << "inputDevice is not a number";

   setAlsaInputDevice(inputDevice);

   //kDebug() << "outputDevice = " << devices[0];
   //int outputDevice = devices[0].toInt(& ok);
   //if(!ok) kDebug() << "outputDevice is not a number";
   //setAlsaOutputDevice(outputDevice);

   //Address book settings

   MapStringInt addressBookSettings = configurationManager.getAddressbookSettings().value();
   setEnableAddressBook(addressBookSettings[ADDRESSBOOK_ENABLE]);
   //setMaxResults(addressBookSettings[ADDRESSBOOK_MAX_RESULTS]);
   //setDisplayPhoto(addressBookSettings[ADDRESSBOOK_DISPLAY_CONTACT_PHOTO]);
   //setBusiness(addressBookSettings[ADDRESSBOOK_DISPLAY_BUSINESS]);
   //setMobile(addressBookSettings[ADDRESSBOOK_DISPLAY_MOBILE]);
   //setHome(addressBookSettings[ADDRESSBOOK_DISPLAY_HOME]);

   //Hooks settings

   MapStringString hooksSettings = configurationManager.getHookSettings().value();
   setAddPrefix(hooksSettings[HOOKS_ENABLED]=="1");
   setPrepend(hooksSettings[HOOKS_ADD_PREFIX]);
   setEnableHooksSIP(hooksSettings[HOOKS_SIP_ENABLED]=="1");
   setEnableHooksIAX(hooksSettings[HOOKS_IAX2_ENABLED]=="1");
   setHooksSIPHeader(hooksSettings[HOOKS_SIP_FIELD]);
   setHooksCommand(hooksSettings[HOOKS_COMMAND]);

   kDebug() << "Finished to read config";
}

void ConfigurationSkeleton::writeConfig()
{
   //ConfigurationSkeleton::writeConfig();
   kDebug() << "Writing config";
   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();


   //General settings

   kDebug() << "Writing General settings";

   //Call history settings
        //if(enableHistory() != QVariant(configurationManager.getHistoryEnabled()).toBool() ) {
            //configurationManager.setHistoryEnabled();
        //}
   configurationManager.setHistoryLimit(historyMax());

   //Display settings

   kDebug() << "Writing Display settings";

   //Notification settings
   //if(notifOnCalls() != configurationManager.getNotify()) configurationManager.setNotify();
   //    if(notifOnMessages() != configurationManager.getMailNotify()) configurationManager.setMailNotify();
   //configurationManager.setMailNotify(notifOnMessages());

   //Window display settings
   //WARNING états inversés
   //if(displayOnStart() == configurationManager.isStartHidden()) configurationManager.startHidden();
   //if(displayOnCalls() != configurationManager.popupMode()) configurationManager.switchPopupMode();

   //Accounts settings

   kDebug() << "Writing Accounts settings";

   //    saveAccountList();

   //Audio settings

   kDebug() << "Writing Audio settings";

   //Audio Interface settings
   //    int prevManager = configurationManager.getAudioManager();
   //    int newManager = interface();
   //    if(prevManager != newManager) {
   //       configurationManager.setAudioManager(newManager);
   //    }

   //ringtones settings
   //    if(enableRingtones() != configurationManager.isRingtoneEnabled()) configurationManager.ringtoneEnabled();
   //    configurationManager.setRingtoneChoice(ringtone());

   //codecs settings
   //kDebug() << "activeCodecList = " << activeCodecList();
   //configurationManager.setActiveCodecList(activeCodecList());


   //alsa settings
   //    if(prevManager == CONST_ALSA && newManager == EnumInterface::ALSA) {
   //       kDebug() << "setting alsa settings";
   //       configurationManager.setOutputAudioPlugin(alsaPlugin());
   //       configurationManager.setAudioInputDevice(alsaInputDevice());
   //       configurationManager.setAudioOutputDevice(alsaOutputDevice());
   //    }

   //Record settings

   kDebug() << "Writing Record settings";

   //    QString destination = destinationFolder();
   //    configurationManager.setRecordPath(destination);


   //Address Book settings

   kDebug() << "Writing Address Book settings";

   MapStringInt addressBookSettings = MapStringInt();
   addressBookSettings[ADDRESSBOOK_ENABLE] = enableAddressBook();
   //    addressBookSettings[ADDRESSBOOK_MAX_RESULTS] = maxResults();
   //    addressBookSettings[ADDRESSBOOK_DISPLAY_CONTACT_PHOTO] = displayPhoto();
   //    addressBookSettings[ADDRESSBOOK_DISPLAY_BUSINESS] = business();
   //    addressBookSettings[ADDRESSBOOK_DISPLAY_MOBILE] = mobile();
   //    addressBookSettings[ADDRESSBOOK_DISPLAY_HOME] = home();
   configurationManager.setAddressbookSettings(addressBookSettings);

   //Hooks settings

   kDebug() << "Writing Hooks settings";

   MapStringString hooksSettings = MapStringString();
   hooksSettings[HOOKS_ENABLED] = addPrefix() ? "1" : "0";
   hooksSettings[HOOKS_ADD_PREFIX] = prepend();
   hooksSettings[HOOKS_SIP_ENABLED] = enableHooksSIP() ? "1" : "0";
   hooksSettings[HOOKS_IAX2_ENABLED] = enableHooksIAX() ? "1" : "0";
   hooksSettings[HOOKS_SIP_FIELD] = hooksSIPHeader();
   hooksSettings[HOOKS_COMMAND] = hooksCommand();
   configurationManager.setHookSettings(hooksSettings);

   kDebug() << "Finished to write config\n";
   ConfigurationSkeletonBase::writeConfig();
}

// QStringList ConfigurationSkeleton::activeCodecList() const
// {
//    return codecListModel->getActiveCodecList();
// }
//
// void ConfigurationSkeleton::setActiveCodecList(const QStringList & v)
// {
//    codecListModel->setActiveCodecList(v);
// }

// void ConfigurationSkeleton::writeConfig()
// {
//
//    ConfigurationSkeletonBase::writeConfig();
// }
