/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin                                               *
 *   jeremy.quentin@savoirfairelinux.com                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "ConfigurationSkeleton.h"

#include "configurationmanager_interface_singleton.h"
#include "sflphone_const.h"

ConfigurationSkeleton::ConfigurationSkeleton()
 : ConfigurationSkeletonBase()
{
	qDebug() << "Building ConfigurationSkeleton";
	codecListModel = new CodecListModel();
	accountListModel = new AccountListModel();
	readConfig();
}

ConfigurationSkeleton * ConfigurationSkeleton::instance = NULL;

ConfigurationSkeleton * ConfigurationSkeleton::self()
{
	if(instance == NULL)
	{	instance = new ConfigurationSkeleton();	}
	return instance; 
}


ConfigurationSkeleton::~ConfigurationSkeleton()
{
}

CodecListModel * ConfigurationSkeleton::getCodecListModel()
{
	return codecListModel;
}

void ConfigurationSkeleton::readConfig()
{
	qDebug() << "\nReading config";
	
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	
// 	qDebug() << "configurationManager.getAudioManager4() = " << configurationManager.getAudioManager();
	////////////////////////
	////General settings////
	////////////////////////
	
	//Call history settings
	setEnableHistory(configurationManager.getHistoryEnabled());
	setHistoryMax(configurationManager.getHistoryLimit());

	//SIP port settings
	setSIPPort(configurationManager.getSipPort());
 	
	////////////////////////
	////Display settings////
	////////////////////////

	//Notification settings
	setNotifOnCalls(configurationManager.getNotify());
	setNotifOnMessages(configurationManager.getMailNotify());
 	
	//Window display settings
	setDisplayOnStart(! configurationManager.isStartHidden());
	setDisplayOnCalls(configurationManager.popupMode());
 	
	/////////////////////////
	////Accounts settings////
	/////////////////////////
	
// 	loadAccountList();


	//Stun settings
	setEnableStun(configurationManager.isStunEnabled());
	setStunServer(configurationManager.getStunServer());

	
	//////////////////////
	////Audio settings////
	//////////////////////
	
	//Audio Interface settings
	int audioManager = configurationManager.getAudioManager();
	qDebug() << "audioManager = " << audioManager;
	setInterface(audioManager);

	//ringtones settings
	setEnableRingtones(configurationManager.isRingtoneEnabled());
	QString ringtone = configurationManager.getRingtoneChoice();
	if(ringtone.isEmpty())
	{
		setRingtone(QString(SHARE_INSTALL_PREFIX) + "sflphone/ringtones/konga.ul");
	}
	else
	{
		setRingtone(ringtone);
	}

	//codecs settings
	setActiveCodecList(configurationManager.getActiveCodecList());

	qDebug() << "configurationManager.getCurrentAudioOutputPlugin() = " << configurationManager.getCurrentAudioOutputPlugin();
	setAlsaPlugin(configurationManager.getCurrentAudioOutputPlugin());
	bool ok;
	QStringList devices = configurationManager.getCurrentAudioDevicesIndex();
	qDebug() << "inputDevice = " << devices[1];
	int inputDevice = devices[1].toInt(& ok);
	if(!ok) qDebug() << "inputDevice is not a number";
	setAlsaInputDevice(inputDevice);
	
	qDebug() << "outputDevice = " << devices[0];
	int outputDevice = devices[0].toInt(& ok);
	if(!ok) qDebug() << "outputDevice is not a number";
	setAlsaOutputDevice(outputDevice);
	
	
	//pulseaudio settings
	setPulseAudioVolumeAlter(configurationManager.getPulseAppVolumeControl());
	
	///////////////////////
	////Record settings////
	///////////////////////
	
	QString recordPath = configurationManager.getRecordPath();
	if(! recordPath.isEmpty())
	{
		setDestinationFolder(recordPath);
	}
	else
	{
		setDestinationFolder(QDir::home().path());
	}
		
	
	
	/////////////////////////////
	////Address book settings////
	/////////////////////////////
	
	MapStringInt addressBookSettings = configurationManager.getAddressbookSettings().value();
	qDebug() << "getAddressbookSettings() : " << addressBookSettings;
	setEnableAddressBook(addressBookSettings[ADDRESSBOOK_ENABLE]);
	setMaxResults(addressBookSettings[ADDRESSBOOK_MAX_RESULTS]);
	setDisplayPhoto(addressBookSettings[ADDRESSBOOK_DISPLAY_CONTACT_PHOTO]);
	setBusiness(addressBookSettings[ADDRESSBOOK_DISPLAY_BUSINESS]);
	setMobile(addressBookSettings[ADDRESSBOOK_DISPLAY_MOBILE]);
	setHome(addressBookSettings[ADDRESSBOOK_DISPLAY_HOME]);
	
	/////////////////////////////
	///////Hooks settings////////
	/////////////////////////////
	
	MapStringString hooksSettings = configurationManager.getHookSettings().value();
	qDebug() << "getHooksSettings() : " << hooksSettings;
	setAddPrefix(hooksSettings[HOOKS_ENABLED]=="1");
	setPrepend(hooksSettings[HOOKS_ADD_PREFIX]);
	setEnableHooksSIP(hooksSettings[HOOKS_SIP_ENABLED]=="1");
	setEnableHooksIAX(hooksSettings[HOOKS_IAX2_ENABLED]=="1");
	setHooksSIPHeader(hooksSettings[HOOKS_SIP_FIELD]);
	setHooksCommand(hooksSettings[HOOKS_COMMAND]);
	
	qDebug() << "Finished to read config\n";
}

void ConfigurationSkeleton::writeConfig()
{
	qDebug() << "\nWriting config";
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	
	
	////////////////////////
	////General settings////
	////////////////////////
	
	qDebug() << "Writing General settings";
	
	//Call history settings
	if(enableHistory() != configurationManager.getHistoryEnabled()) configurationManager.setHistoryEnabled();
	configurationManager.setHistoryLimit(historyMax());
	//SIP port settings
	configurationManager.setSipPort(sIPPort());


	////////////////////////
	////Display settings////
	////////////////////////
	
	qDebug() << "Writing Display settings";
	
	//Notification settings
	if(notifOnCalls() != configurationManager.getNotify()) configurationManager.setNotify();
	if(notifOnMessages() != configurationManager.getMailNotify()) configurationManager.setMailNotify();
	
	//Window display settings
	//WARNING états inversés
	if(displayOnStart() == configurationManager.isStartHidden()) configurationManager.startHidden();
	if(displayOnCalls() != configurationManager.popupMode()) configurationManager.switchPopupMode();
	
	/////////////////////////
	////Accounts settings////
	/////////////////////////
	
	qDebug() << "Writing Accounts settings";
	
// 	saveAccountList();


	//Stun settings
	if(enableStun() != configurationManager.isStunEnabled()) configurationManager.enableStun();
	configurationManager.setStunServer(stunServer());

	//////////////////////
	////Audio settings////
	//////////////////////
	
	qDebug() << "Writing Audio settings";
	
	//Audio Interface settings
	int prevManager = configurationManager.getAudioManager();
	int newManager = interface();
	if(prevManager != newManager)
	{
		configurationManager.setAudioManager(newManager);
	}
	
	//ringtones settings
	if(enableRingtones() != configurationManager.isRingtoneEnabled()) configurationManager.ringtoneEnabled();
	configurationManager.setRingtoneChoice(ringtone());

	//codecs settings
	qDebug() << "activeCodecList = " << activeCodecList();
	configurationManager.setActiveCodecList(activeCodecList());
	

	//alsa settings
	if(prevManager == CONST_ALSA && newManager == EnumInterface::ALSA)
	{
		qDebug() << "setting alsa settings";
		configurationManager.setOutputAudioPlugin(alsaPlugin());
		configurationManager.setAudioInputDevice(alsaInputDevice());
		configurationManager.setAudioOutputDevice(alsaOutputDevice());
	}
	//pulseaudio settings
	if(newManager == EnumInterface::PulseAudio)
	{
		qDebug() << "setting pulseaudio settings";
		if(pulseAudioVolumeAlter() != configurationManager.getPulseAppVolumeControl()) configurationManager.setPulseAppVolumeControl();
	}
	
	
	///////////////////////
	////Record settings////
	///////////////////////
	
	qDebug() << "Writing Record settings";
	
	QString destination = destinationFolder();
	configurationManager.setRecordPath(destination);
	
	
	/////////////////////////////
	////Address Book settings////
	/////////////////////////////
	
	qDebug() << "Writing Address Book settings";
	
	MapStringInt addressBookSettings = MapStringInt();
	addressBookSettings[ADDRESSBOOK_ENABLE] = enableAddressBook();
	addressBookSettings[ADDRESSBOOK_MAX_RESULTS] = maxResults();
	addressBookSettings[ADDRESSBOOK_DISPLAY_CONTACT_PHOTO] = displayPhoto();
	addressBookSettings[ADDRESSBOOK_DISPLAY_BUSINESS] = business();
	addressBookSettings[ADDRESSBOOK_DISPLAY_MOBILE] = mobile();
	addressBookSettings[ADDRESSBOOK_DISPLAY_HOME] = home();
	configurationManager.setAddressbookSettings(addressBookSettings);
	
	/////////////////////////////
	///////Hooks settings////////
	/////////////////////////////
	
	qDebug() << "Writing Hooks settings";
	
	MapStringString hooksSettings = MapStringString();
	hooksSettings[HOOKS_ENABLED] = addPrefix() ? "1" : "0";
	hooksSettings[HOOKS_ADD_PREFIX] = prepend();
	hooksSettings[HOOKS_SIP_ENABLED] = enableHooksSIP() ? "1" : "0";
	hooksSettings[HOOKS_IAX2_ENABLED] = enableHooksIAX() ? "1" : "0";
	hooksSettings[HOOKS_SIP_FIELD] = hooksSIPHeader();
	hooksSettings[HOOKS_COMMAND] = hooksCommand();
	configurationManager.setHookSettings(hooksSettings);
	
	qDebug() << "Finished to write config\n";
	
	readConfig();
}

QStringList ConfigurationSkeleton::activeCodecList() const
{
	return codecListModel->getActiveCodecList();
}

void ConfigurationSkeleton::setActiveCodecList(const QStringList & v)
{
	codecListModel->setActiveCodecList(v);
}
