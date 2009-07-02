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

#include "sflphone_const.h"

DlgAudio::DlgAudio(KConfigDialog *parent)
 : QWidget(parent)
{
	setupUi(this);
	
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	QStyle * style = QApplication::style();
	
	KUrl url = KUrl(SHARE_INSTALL_PREFIX);
	url.cd("sflphone/ringtones");
	KUrlRequester_ringtone->setUrl(url);
	KUrlRequester_ringtone->lineEdit()->setObjectName("kcfg_ringtone"); 
	
	codecTableHasChanged = false;
	toolButton_codecUp->setIcon(KIcon("go-up"));
	toolButton_codecDown->setIcon(KIcon("go-down"));
	tableWidget_codecs->verticalHeader()->hide();
	tableWidget_codecs->setSelectionBehavior(QAbstractItemView::SelectRows);
	
	updateAlsaSettings();
	connect(box_alsaPlugin,        SIGNAL(currentIndexChanged(int)),        parent, SLOT(updateButtons()));
	connect(tableWidget_codecs,    SIGNAL(itemChanged(QTableWidgetItem *)), this,   SLOT(codecTableChanged()));
	connect(toolButton_codecUp,    SIGNAL(clicked()),                       this,   SLOT(codecTableChanged()));
	connect(toolButton_codecDown,  SIGNAL(clicked()),                       this,   SLOT(codecTableChanged()));
	
	connect(this,                  SIGNAL(updateButtons()),                 parent, SLOT(updateButtons()));
}


DlgAudio::~DlgAudio()
{
}

void DlgAudio::updateWidgets()
{
// 	qDebug() << "DlgAudio::updateWidgets";
	//alsa Plugin
	ConfigurationSkeleton * skeleton = ConfigurationSkeleton::self();
	box_alsaPlugin->setCurrentIndex(box_alsaPlugin->findText(skeleton->alsaPlugin()));
	
	//codecList
	qDebug() << "loadCodecs";
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	QStringList codecList = configurationManager.getCodecList();
	QStringList activeCodecList = skeleton->activeCodecList();
	#if QT_VERSION >= 0x040500
		activeCodecList.removeDuplicates();
	#else
   	for (int i = 0 ; i < activeCodecList.size() ; i++)
		{
			if(activeCodecList.lastIndexOf(activeCodecList[i]) != i)
			{
				activeCodecList.removeAt(i);
				i--;
			}
		}
	#endif

	for (int i=0 ; i<activeCodecList.size() ; i++)
	{
		if(! codecList.contains(activeCodecList[i]))
		{
			activeCodecList.removeAt(i);
			i--;
		}
	}
	QStringList codecListToDisplay = activeCodecList;
	for (int i=0 ; i<codecList.size() ; i++)
	{
		if(! activeCodecList.contains(codecList[i]))
		{
			codecListToDisplay << codecList[i];
		}
	}
	qDebug() << "codecList = " << codecList;
	qDebug() << "activeCodecList" << activeCodecList;
	qDebug() << "codecListToDisplay" << codecListToDisplay;
	tableWidget_codecs->setRowCount(0);
	for(int i=0 ; i<codecListToDisplay.size() ; i++)
	{
		bool ok;
		qDebug() << codecListToDisplay[i];
		QString payloadStr = QString(codecListToDisplay[i]);
		int payload = payloadStr.toInt(&ok);
		if(!ok)	
			qDebug() << "The codec's payload sent by the configurationManager is not a number : " << codecListToDisplay[i];
		else
		{
			QStringList details = configurationManager.getCodecDetails(payload);
			tableWidget_codecs->insertRow(i);
			tableWidget_codecs->setVerticalHeaderItem (i, new QTableWidgetItem());
			tableWidget_codecs->verticalHeaderItem (i)->setText(payloadStr);
			tableWidget_codecs->setItem(i,0,new QTableWidgetItem(""));
			tableWidget_codecs->setItem(i,1,new QTableWidgetItem(details[CODEC_NAME]));
			tableWidget_codecs->setItem(i,2,new QTableWidgetItem(details[CODEC_SAMPLE_RATE]));
			tableWidget_codecs->setItem(i,3,new QTableWidgetItem(details[CODEC_BIT_RATE]));
			tableWidget_codecs->setItem(i,4,new QTableWidgetItem(details[CODEC_BANDWIDTH]));
			tableWidget_codecs->item(i,0)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsUserCheckable|Qt::ItemIsEnabled);
			tableWidget_codecs->item(i,0)->setCheckState(activeCodecList.contains(codecListToDisplay[i]) ? Qt::Checked : Qt::Unchecked);
			tableWidget_codecs->item(i,1)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			tableWidget_codecs->item(i,2)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			tableWidget_codecs->item(i,3)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			tableWidget_codecs->item(i,4)->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);

			qDebug() << "Added to codecs : " << payloadStr << " , " << details[CODEC_NAME];
		}
	}
	tableWidget_codecs->resizeColumnsToContents();
	tableWidget_codecs->resizeRowsToContents();
	codecTableHasChanged = false;
}


void DlgAudio::updateSettings()
{
// 	qDebug() << "DlgAudio::updateSettings";
	//alsaPlugin
	ConfigurationSkeleton * skeleton = ConfigurationSkeleton::self();
	skeleton->setAlsaPlugin(box_alsaPlugin->currentText());
	
	//codecList
	QStringList activeCodecs;
	for(int i = 0 ; i < tableWidget_codecs->rowCount() ; i++)
	{
		if(tableWidget_codecs->item(i,0)->checkState() == Qt::Checked)
		{
			activeCodecs << tableWidget_codecs->verticalHeaderItem(i)->text();
		}
	}
	qDebug() << "Calling setActiveCodecList with list : " << activeCodecs ;
	skeleton->setActiveCodecList(activeCodecs);
	codecTableHasChanged = false;
}

bool DlgAudio::hasChanged()
{
// 	qDebug() << "DlgAudio::hasChanged";
	ConfigurationSkeleton * skeleton = ConfigurationSkeleton::self();
	qDebug() << "skeleton->alsaPlugin() = " << skeleton->alsaPlugin();
	qDebug() << "box_alsaPlugin->currentText() = " << box_alsaPlugin->currentText();
	bool alsaPluginHasChanged = 
	           skeleton->interface() == ConfigurationSkeleton::EnumInterface::ALSA 
	       &&  skeleton->alsaPlugin() != box_alsaPlugin->currentText();
	return alsaPluginHasChanged || codecTableHasChanged;
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

void DlgAudio::updateCodecListCommands()
{
	bool buttonsEnabled[2] = {true,true};
	if(! tableWidget_codecs->currentItem())
	{
		buttonsEnabled[0] = false;
		buttonsEnabled[1] = false;
	}
	else if(tableWidget_codecs->currentRow() == 0)
	{
		buttonsEnabled[0] = false;
	}
	else if(tableWidget_codecs->currentRow() == tableWidget_codecs->rowCount() - 1)
	{
		buttonsEnabled[1] = false;
	}
	toolButton_codecUp->setEnabled(buttonsEnabled[0]);
	toolButton_codecDown->setEnabled(buttonsEnabled[1]);
}

void DlgAudio::on_tableWidget_codecs_currentCellChanged(int currentRow)
{
	qDebug() << "on_tableWidget_codecs_currentCellChanged";
	int nbCol = tableWidget_codecs->columnCount();
	for(int i = 0 ; i < nbCol ; i++)
	{
		tableWidget_codecs->setRangeSelected(QTableWidgetSelectionRange(currentRow, 0, currentRow, nbCol - 1), true);
	}
	updateCodecListCommands();
}

void DlgAudio::on_toolButton_codecUp_clicked()
{
	qDebug() << "on_toolButton_codecUp_clicked";
	int currentCol = tableWidget_codecs->currentColumn();
	int currentRow = tableWidget_codecs->currentRow();
	int nbCol = tableWidget_codecs->columnCount();
	for(int i = 0 ; i < nbCol ; i++)
	{
		QTableWidgetItem * item1 = tableWidget_codecs->takeItem(currentRow, i);
		QTableWidgetItem * item2 = tableWidget_codecs->takeItem(currentRow - 1, i);
		tableWidget_codecs->setItem(currentRow - 1, i , item1);
		tableWidget_codecs->setItem(currentRow, i , item2);
	}
	QTableWidgetItem * item1 = tableWidget_codecs->takeVerticalHeaderItem(currentRow);
	QTableWidgetItem * item2 = tableWidget_codecs->takeVerticalHeaderItem(currentRow - 1);
	tableWidget_codecs->setVerticalHeaderItem(currentRow - 1, item1);
	tableWidget_codecs->setVerticalHeaderItem(currentRow, item2);
	tableWidget_codecs->setCurrentCell(currentRow - 1, currentCol);
}

void DlgAudio::on_toolButton_codecDown_clicked()
{
	qDebug() << "on_toolButton_codecDown_clicked";
	int currentCol = tableWidget_codecs->currentColumn();
	int currentRow = tableWidget_codecs->currentRow();
	int nbCol = tableWidget_codecs->columnCount();
	for(int i = 0 ; i < nbCol ; i++)
	{
		QTableWidgetItem * item1 = tableWidget_codecs->takeItem(currentRow, i);
		QTableWidgetItem * item2 = tableWidget_codecs->takeItem(currentRow + 1, i);
		tableWidget_codecs->setItem(currentRow + 1, i , item1);
		tableWidget_codecs->setItem(currentRow, i , item2);
	}
	QTableWidgetItem * item1 = tableWidget_codecs->takeVerticalHeaderItem(currentRow);
	QTableWidgetItem * item2 = tableWidget_codecs->takeVerticalHeaderItem(currentRow + 1);
	tableWidget_codecs->setVerticalHeaderItem(currentRow + 1, item1);
	tableWidget_codecs->setVerticalHeaderItem(currentRow, item2);
	tableWidget_codecs->setCurrentCell(currentRow + 1, currentCol);
}


void DlgAudio::codecTableChanged()
{
	codecTableHasChanged = true;
	emit updateButtons();
}