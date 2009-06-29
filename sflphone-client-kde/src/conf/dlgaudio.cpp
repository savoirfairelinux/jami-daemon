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
#include "dlgaudio.h"

#include <KLineEdit>
#include "configurationmanager_interface_singleton.h"
#include "conf/ConfigurationSkeleton.h"
#include "conf/ConfigurationDialog.h"

DlgAudio::DlgAudio(KConfigDialog *parent)
 : QWidget(parent)
{
	setupUi(this);
	
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	
	KUrl url = KUrl(SHARE_INSTALL_PREFIX);
	url.cd("sflphone/ringtones");
	KUrlRequester_ringtone->setUrl(url);
	KUrlRequester_ringtone->lineEdit()->setObjectName("kcfg_ringtone"); 
	
	updateAlsaSettings();
	connect(box_alsaPlugin, SIGNAL(currentIndexChanged(int)), parent, SLOT(updateButtons()));
}


DlgAudio::~DlgAudio()
{
}

void DlgAudio::updateWidgets()
{
// 	qDebug() << "DlgAudio::updateWidgets";
	ConfigurationSkeleton * skeleton = ConfigurationSkeleton::self();
	box_alsaPlugin->setCurrentIndex(box_alsaPlugin->findText(skeleton->alsaPlugin()));
}


void DlgAudio::updateSettings()
{
// 	qDebug() << "DlgAudio::updateSettings";
	ConfigurationSkeleton * skeleton = ConfigurationSkeleton::self();
	skeleton->setAlsaPlugin(box_alsaPlugin->currentText());
}

bool DlgAudio::hasChanged()
{
// 	qDebug() << "DlgAudio::hasChanged";
	ConfigurationSkeleton * skeleton = ConfigurationSkeleton::self();
	qDebug() << "skeleton->alsaPlugin() = " << skeleton->alsaPlugin();
	qDebug() << "box_alsaPlugin->currentText() = " << box_alsaPlugin->currentText();
	return (skeleton->interface() == ConfigurationSkeleton::EnumInterface::ALSA && skeleton->alsaPlugin() != box_alsaPlugin->currentText());
}

void DlgAudio::updateAlsaSettings()
{
	qDebug() << "DlgAudio::updateAlsaSettings";
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	if(configurationManager.getAudioManager() == ConfigurationSkeleton::EnumInterface::ALSA)
	{
		ConfigurationSkeleton * skeleton = ConfigurationSkeleton::self();
		
		QStringList pluginList = configurationManager.getOutputAudioPluginList();
		box_alsaPlugin->clear();
		box_alsaPlugin->addItems(pluginList);
		int index = box_alsaPlugin->findText(skeleton->alsaPlugin());
		if(index < 0) index = 0;
		box_alsaPlugin->setCurrentIndex(index);
		
		QStringList inputDeviceList = configurationManager.getAudioInputDeviceList();
		kcfg_alsaInputDevice->clear();
		kcfg_alsaInputDevice->addItems(inputDeviceList);
		kcfg_alsaInputDevice->setCurrentIndex(skeleton->alsaInputDevice());
		
		QStringList outputDeviceList = configurationManager.getAudioOutputDeviceList();
		kcfg_alsaOutputDevice->clear();
		kcfg_alsaOutputDevice->addItems(outputDeviceList);
		kcfg_alsaOutputDevice->setCurrentIndex(skeleton->alsaOutputDevice());
		groupBox_alsa->setEnabled(true);
	}
	else
	{
// 		box_alsaPlugin->clear();
// 		kcfg_alsaInputDevice->clear();
// 		kcfg_alsaOutputDevice->clear();
		groupBox_alsa->setEnabled(false);
	}
}


