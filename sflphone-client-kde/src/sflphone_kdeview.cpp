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

#include "sflphone_kdeview.h"

#include <klocale.h>
#include <QtGui/QLabel>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QMenu>
#include <QtGui/QBrush>
#include <QtGui/QPalette>
#include <QtGui/QInputDialog>

#include <kstandardaction.h>
#include <kactioncollection.h>
#include <kaction.h>

#include <kabc/addressbook.h>
#include <kabc/stdaddressbook.h>
#include <kabc/addresseelist.h>

#include "sflphone_const.h"
#include "configurationmanager_interface_singleton.h"
#include "callmanager_interface_singleton.h"
#include "instance_interface_singleton.h"
#include "ActionSetAccountFirst.h"
#include "ContactItemWidget.h"
#include "SFLPhone.h"
#include "typedefs.h"


using namespace KABC;

ConfigurationDialog * sflphone_kdeView::configDialog;

sflphone_kdeView::sflphone_kdeView(QWidget *parent)
	: QWidget(parent)
{
	setupUi(this);
	
	errorWindow = new QErrorMessage(this);
	callList = new CallList();
	
	configDialog = new ConfigurationDialog(this);
	configDialog->setModal(true);
	
	wizard = new AccountWizard(this);
	wizard->setModal(false);
	
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	connect(&callManager, SIGNAL(callStateChanged(const QString &, const QString &)),
	        this,         SLOT(on1_callStateChanged(const QString &, const QString &)));
	connect(&callManager, SIGNAL(incomingCall(const QString &, const QString &, const QString &)),
	        this,         SLOT(on1_incomingCall(const QString &, const QString &)));
	connect(&callManager, SIGNAL(incomingMessage(const QString &, const QString &)),
	        this,         SLOT(on1_incomingMessage(const QString &, const QString &)));
	connect(&callManager, SIGNAL(voiceMailNotify(const QString &, int)),
	        this,         SLOT(on1_voiceMailNotify(const QString &, int)));
	connect(&callManager, SIGNAL(volumeChanged(const QString &, double)),
	        this,         SLOT(on1_volumeChanged(const QString &, double)));
	        
	QPalette pal = QPalette(palette());
	pal.setColor(QPalette::AlternateBase, Qt::lightGray);
	setPalette(pal);
	
	stackedWidget_screen->setCurrentWidget(page_callList);
	
	loadWindow();
	
} 

sflphone_kdeView::~sflphone_kdeView()
{
	delete configDialog;
	delete wizard;
	delete callList;
	delete errorWindow;
}


void sflphone_kdeView::buildDialPad()
{
	QHBoxLayout * layout;
	QLabel * number;
	QLabel * text;
	int spacing = 5;
	int numberSize = 14;
	int textSize = 8;
	
	QPushButton * buttons[12] = 
	    {pushButton_1,      pushButton_2,   pushButton_3, 
	     pushButton_4,      pushButton_5,   pushButton_6, 
	     pushButton_7,      pushButton_8,   pushButton_9, 
	     pushButton_etoile, pushButton_0,   pushButton_diese};
	     
	QString numbers[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
	
	QString texts[12] = {"", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz", "", "", ""};
	
	for(int i = 0 ; i < 12 ; i++)
	{
		layout = new QHBoxLayout();
		layout->setSpacing(spacing);
		number = new QLabel(numbers[i]);
		number->setFont(QFont("", numberSize));
		layout->addWidget(number);
		number->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		text = new QLabel(texts[i]);
		text->setFont(QFont("", textSize));
		layout->addWidget(text);
		buttons[i]->setLayout(layout);
		buttons[i]->setMinimumHeight(30);
		buttons[i]->setText("");
	}
}

void sflphone_kdeView::loadWindow()
{
	qDebug() << "loadWindow";
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	action_displayVolumeControls->setChecked(configurationManager.getVolumeControls());
	action_displayDialpad->setChecked(configurationManager.getDialpad());
	buildDialPad();
	updateWindowCallState();
	updateRecordButton();
	updateVolumeButton();
	updateRecordBar();
	updateVolumeBar();
	updateVolumeControls();
	updateDialpad();
	updateSearchHistory();
}

QString sflphone_kdeView::firstAccountId()
{
	Account * firstAccount = getAccountList()->firstRegisteredAccount();
	if(firstAccount == NULL)
	{
		return QString();
	}
	return firstAccount->getAccountId();
}

QVector<Account *> sflphone_kdeView::registeredAccounts()
{
	return getAccountList()->registeredAccounts();
}

Account * sflphone_kdeView::firstRegisteredAccount()
{
	return getAccountList()->firstRegisteredAccount();
}

AccountList * sflphone_kdeView::getAccountList()
{
	return configDialog->getAccountList();
}

QErrorMessage * sflphone_kdeView::getErrorWindow()
{
	return errorWindow;
}

void sflphone_kdeView::addCallToCallList(Call * call)
{
	QListWidgetItem * item = call->getItem();
	QWidget * widget = call->getItemWidget();
	if(item && widget)
	{
		listWidget_callList->addItem(item);
		listWidget_callList->setItemWidget(item, widget);
	}
}

void sflphone_kdeView::addCallToCallHistory(Call * call)
{
	QListWidgetItem * item = call->getHistoryItem();
	QWidget * widget = call->getHistoryItemWidget();
	if(item && widget)
	{
		listWidget_callHistory->addItem(item);
		listWidget_callHistory->setItemWidget(item, widget);
	}
}

void sflphone_kdeView::addContactToContactList(Contact * contact)
{
	QListWidgetItem * item = contact->getItem();
	QWidget * widget = contact->getItemWidget();
	if(item && widget)
	{
		listWidget_addressBook->addItem(item);
		listWidget_addressBook->setItemWidget(item, widget);
	}
}

void sflphone_kdeView::typeString(QString str)
{
	qDebug() << "typeString";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		callManager.playDTMF(str);
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Typing when no item is selected. Opening an item.";
			Call * call = callList->addDialingCall();
			addCallToCallList(call);
			listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		}
		callList->findCallByItem(listWidget_callList->currentItem())->appendItemText(str);
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		qDebug() << "In call history.";
		lineEdit_searchHistory->setText(lineEdit_searchHistory->text() + str);
		lineEdit_searchHistory->setFocus();
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		qDebug() << "In address book.";
		lineEdit_addressBook->setText(lineEdit_addressBook->text() + str);
		lineEdit_addressBook->setFocus();
	}
}

void sflphone_kdeView::backspace()
{
	qDebug() << "backspace";
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		qDebug() << "In call list.";
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Backspace when no item is selected. Doing nothing.";
		}
		else
		{
			Call * call = callList->findCallByItem(listWidget_callList->currentItem());
			if(!call)
			{
				qDebug() << "Error : Backspace on unexisting call.";
			}
			else
			{
				call->backspaceItemText();
				updateCallItem(call);
			}
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		qDebug() << "In call history.";
		int textSize = lineEdit_searchHistory->text().size();
		if(textSize > 0)
		{
			lineEdit_searchHistory->setText(lineEdit_searchHistory->text().remove(textSize-1, 1));
		}
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		qDebug() << "In address book.";
		int textSize = lineEdit_addressBook->text().size();
		if(textSize > 0)
		{
			lineEdit_addressBook->setText(lineEdit_addressBook->text().remove(textSize-1, 1));
		}
	}
}

void sflphone_kdeView::escape()
{
	qDebug() << "escape";
	if(stackedWidget_screen->currentWidget() == page_callList )
	{
		qDebug() << "In call list.";
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Escape when no item is selected. Doing nothing.";
		}
		else
		{
			Call * call = callList->findCallByItem(listWidget_callList->currentItem());
			if(!call)
			{
				qDebug() << "Error : Escape on unexisting call.";
			}
			else
			{
				int state = call->getState();
				if(state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD)
				{
					actionb(call, CALL_ACTION_TRANSFER);
				}
				else
				{
					actionb(call, CALL_ACTION_REFUSE);
				}
			}
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		qDebug() << "In call history.";
		lineEdit_searchHistory->clear();
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		qDebug() << "In address book.";
		lineEdit_addressBook->clear();
	}
}

void sflphone_kdeView::enter()
{
	qDebug() << "enter";
	if(stackedWidget_screen->currentWidget() == page_callList )
	{
		qDebug() << "In call list.";
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Enter when no item is selected. Doing nothing.";
		}
		else
		{
			Call * call = callList->findCallByItem(listWidget_callList->currentItem());
			if(!call)
			{
				qDebug() << "Error : Enter on unexisting call.";
			}
			else
			{
				int state = call->getState();
				if(state == CALL_STATE_INCOMING || state == CALL_STATE_DIALING || state == CALL_STATE_TRANSFER || state == CALL_STATE_TRANSF_HOLD)
				{
					actionb(call, CALL_ACTION_ACCEPT);
				}
				else
				{
					qDebug() << "Enter when call selected not in appropriate state. Doing nothing.";
				}
			}
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		qDebug() << "In call history.";
		QListWidgetItem * item = listWidget_callHistory->currentItem();
		if(!item)
		{
			qDebug() << "Enter when no item is selected. Doing nothing.";
		}
		else
		{
			action_history->setChecked(false);
			stackedWidget_screen->setCurrentWidget(page_callList);
			
			Call * pastCall = callList->findCallByHistoryItem(item);
			if (!pastCall)
			{
				qDebug() << "pastCall null";
			}
			Call * call = callList->addDialingCall(pastCall->getPeerName(), pastCall->getAccountId());
			call->appendItemText(pastCall->getPeerPhoneNumber());
			addCallToCallList(call);
			listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
			actionb(call, CALL_ACTION_ACCEPT);
		}
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		qDebug() << "In address book.";
		QListWidgetItem * item = listWidget_addressBook->currentItem();
		if(!item)
		{
			qDebug() << "Enter when no item is selected. Doing nothing.";
		}
		else
		{
			action_addressBook->setChecked(false);
			stackedWidget_screen->setCurrentWidget(page_callList);
			ContactItemWidget * w = (ContactItemWidget *) (listWidget_addressBook->itemWidget(item));
			Call * call = callList->addDialingCall(w->getContactName());
			call->appendItemText(w->getContactNumber());
			addCallToCallList(call);
			listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
			actionb(call, CALL_ACTION_ACCEPT);
		}
	}
}

void sflphone_kdeView::actionb(Call * call, call_action action)
{
	try
	{
		call->actionPerformed(action);
	}
	catch(const char * msg)
	{
		errorWindow->showMessage(QString(msg));
	}
	updateCallItem(call);
	updateWindowCallState();
}

void sflphone_kdeView::action(QListWidgetItem * item, call_action action)
{
	actionb(callList->findCallByItem(item), action);
}

/*******************************************
******** Update Display Functions **********
*******************************************/

void sflphone_kdeView::updateCallItem(Call * call)
{
	call_state state = call->getState();
	if(state == CALL_STATE_OVER)
	{
		QListWidgetItem * item = call->getItem();
		qDebug() << "Updating call with CALL_STATE_OVER. Deleting item " << (*callList)[item]->getCallId();
		listWidget_callList->takeItem(listWidget_callList->row(item));
	}
}


void sflphone_kdeView::updateWindowCallState()
{
	qDebug() << "updateWindowCallState";
	QListWidgetItem * item;
	
	bool enabledActions[6]= {true,true,true,true,true,true};
	QString buttonIconFiles[3] = {ICON_CALL, ICON_HANGUP, ICON_HOLD};
	QString actionTexts[5] = {ACTION_LABEL_CALL, ACTION_LABEL_HANG_UP, ACTION_LABEL_HOLD, ACTION_LABEL_TRANSFER, ACTION_LABEL_RECORD};
	bool transfer = false;
	//tells whether the call is in recording position
	bool recordActivated = false;
	//tells whether the call can be recorded in the state it is right now
	bool recordEnabled = false;
	enabledActions[5] = firstRegisteredAccount() && ! firstRegisteredAccount()->getAccountDetail(ACCOUNT_MAILBOX).isEmpty();
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		item = listWidget_callList->currentItem();
		if (!item)
		{
			qDebug() << "No item selected. Updating window.";
			enabledActions[1] = false;
			enabledActions[2] = false;
			enabledActions[3] = false;
			enabledActions[4] = false;
		}
		else
		{
			Call * call = (*callList)[item];
			call_state state = call->getState();
			//qDebug() << "calling getIsRecording on " << call->getCallId();
			//recordActivated = callManager.getIsRecording(call->getCallId());
			recordActivated = call->getRecording();
			switch (state)
			{
				case CALL_STATE_INCOMING:
					qDebug() << "Reached CALL_STATE_INCOMING with call " << (*callList)[item]->getCallId() << ". Updating window.";
					buttonIconFiles[0] = ICON_ACCEPT;
					buttonIconFiles[1] = ICON_REFUSE;
					actionTexts[0] = ACTION_LABEL_ACCEPT;
					actionTexts[0] = ACTION_LABEL_REFUSE;
					break;
				case CALL_STATE_RINGING:
					qDebug() << "Reached CALL_STATE_RINGING with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[2] = false;
					enabledActions[3] = false;
					break;
				case CALL_STATE_CURRENT:
					qDebug() << "details = " << CallManagerInterfaceSingleton::getInstance().getCallDetails(call->getCallId()).value();
					qDebug() << "Reached CALL_STATE_CURRENT with call " << (*callList)[item]->getCallId() << ". Updating window.";
					recordEnabled = true;
					break;
				case CALL_STATE_DIALING:
					qDebug() << "Reached CALL_STATE_DIALING with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[2] = false;
					enabledActions[3] = false;
					enabledActions[4] = false;
					buttonIconFiles[0] = ICON_ACCEPT;
					break;
				case CALL_STATE_HOLD:
					qDebug() << "Reached CALL_STATE_HOLD with call " << (*callList)[item]->getCallId() << ". Updating window.";
					buttonIconFiles[2] = ICON_UNHOLD;
					actionTexts[2] = ACTION_LABEL_UNHOLD;
					break;		
				case CALL_STATE_FAILURE:
					qDebug() << "Reached CALL_STATE_FAILURE with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[0] = false;
					enabledActions[2] = false;
					enabledActions[3] = false;
					enabledActions[4] = false;
					break;
				case CALL_STATE_BUSY:
					qDebug() << "Reached CALL_STATE_BUSY with call " << (*callList)[item]->getCallId() << ". Updating window.";
					enabledActions[0] = false;
					enabledActions[2] = false;
					enabledActions[3] = false;
					enabledActions[4] = false;
				break;
				case CALL_STATE_TRANSFER:
					qDebug() << "Reached CALL_STATE_TRANSFER with call " << (*callList)[item]->getCallId() << ". Updating window.";
					buttonIconFiles[0] = ICON_EXEC_TRANSF;
					actionTexts[3] = ACTION_LABEL_GIVE_UP_TRANSF;
					transfer = true;
					recordEnabled = true;
					break;
				case CALL_STATE_TRANSF_HOLD:
					qDebug() << "Reached CALL_STATE_TRANSF_HOLD with call " << (*callList)[item]->getCallId() << ". Updating window.";
					buttonIconFiles[0] = ICON_EXEC_TRANSF;
					buttonIconFiles[2] = ICON_UNHOLD;
					actionTexts[3] = ACTION_LABEL_GIVE_UP_TRANSF;
					actionTexts[2] = ACTION_LABEL_UNHOLD;
					transfer = true;
					break;
				case CALL_STATE_OVER:
					qDebug() << "Error : Reached CALL_STATE_OVER with call " << (*callList)[item]->getCallId() << "!";
					break;
				case CALL_STATE_ERROR:
					qDebug() << "Error : Reached CALL_STATE_ERROR with call " << (*callList)[item]->getCallId() << "!";
					break;
				default:
					qDebug() << "Error : Reached unexisting state for call " << (*callList)[item]->getCallId() << "!";
					break;
			}
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		item = listWidget_callHistory->currentItem();
		buttonIconFiles[0] = ICON_ACCEPT;
		actionTexts[0] = ACTION_LABEL_CALL_BACK;
		actionTexts[1] = ACTION_LABEL_GIVE_UP_SEARCH;
		if (!item)
		{
			qDebug() << "No item selected. Updating window.";
			enabledActions[0] = false;
			enabledActions[1] = false;
			enabledActions[2] = false;
			enabledActions[3] = false;
			enabledActions[4] = false;
		}
		else
		{
			enabledActions[1] = false;
			enabledActions[2] = false;
			enabledActions[3] = false;
			enabledActions[4] = false;
		}
		if(!lineEdit_searchHistory->text().isEmpty())
		{
			enabledActions[1] = true;
		}
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		item = listWidget_addressBook->currentItem();
		buttonIconFiles[0] = ICON_ACCEPT;
		actionTexts[1] = ACTION_LABEL_GIVE_UP_SEARCH;
		if (!item)
		{
			qDebug() << "No item selected. Updating window.";
			enabledActions[0] = false;
			enabledActions[1] = false;
			enabledActions[2] = false;
			enabledActions[3] = false;
			enabledActions[4] = false;
		}
		else
		{
			enabledActions[1] = false;
			enabledActions[2] = false;
			enabledActions[3] = false;
			enabledActions[4] = false;
		}
		if(!lineEdit_addressBook->text().isEmpty())
		{
			enabledActions[1] = true;
		}
	}
	
	action_accept->setEnabled(enabledActions[0]);
	action_refuse->setEnabled(enabledActions[1]);
	action_hold->setEnabled(enabledActions[2]);
	action_transfer->setEnabled(enabledActions[3]);
	action_record->setEnabled(enabledActions[4]);
	action_mailBox->setEnabled(enabledActions[5]);
	
	action_record->setIcon(QIcon(recordEnabled ? ICON_REC_DEL_ON : ICON_REC_DEL_OFF));
	action_accept->setIcon(QIcon(buttonIconFiles[0]));
	action_refuse->setIcon(QIcon(buttonIconFiles[1]));
	action_hold->setIcon(QIcon(buttonIconFiles[2]));
	
	action_accept->setText(actionTexts[0]);
	action_refuse->setText(actionTexts[1]);
	action_hold->setText(actionTexts[2]);
	action_transfer->setText(actionTexts[3]);
	action_record->setText(actionTexts[4]);
	
	action_transfer->setChecked(transfer);
	action_record->setChecked(recordActivated);
}

void sflphone_kdeView::updateSearchHistory()
{
	qDebug() << "updateSearchHistory";
	lineEdit_searchHistory->setVisible(!lineEdit_searchHistory->text().isEmpty());
}


void sflphone_kdeView::updateCallHistory()
{
	qDebug() << "updateCallHistory";
	while(listWidget_callHistory->count() > 0)
	{
		QListWidgetItem * item = listWidget_callHistory->takeItem(0);
		qDebug() << "take item " << item->text() << " ; widget = " << callList->findCallByHistoryItem(item);
	}
	QString textSearched = lineEdit_searchHistory->text();
	for(int i = callList->size() - 1 ; i >= 0 ; i--)
	{
		Call * call = (*callList)[i];
		qDebug() << "" << call->getCallId();
		if(
		     call->getState() == CALL_STATE_OVER && 
		     call->getHistoryState() != NONE && 
		    (call->getPeerPhoneNumber().contains(textSearched) || call->getPeerName().contains(textSearched))
		  )
		{
			qDebug() << "call->getPeerPhoneNumber()=" << call->getPeerPhoneNumber() << " contains textSearched=" << textSearched;
			addCallToCallHistory(call);
		}
	}
	alternateColors(listWidget_callHistory);
}

void sflphone_kdeView::updateAddressBook()
{
	qDebug() << "updateAddressBook";
	while(listWidget_addressBook->count() > 0)
	{
		QListWidgetItem * item = listWidget_addressBook->takeItem(0);
		qDebug() << "take item " << item->text();
	}
	QString textSearched = lineEdit_addressBook->text();
	if(textSearched.isEmpty())
	{
		label_addressBookFull->setVisible(false);
		return;
	}
	bool full = false;
	QVector<Contact *> contactsFound = findContactsInKAddressBook(textSearched, full);
	qDebug() << "Full : " << full;
	label_addressBookFull->setVisible(full);
	for(int i = 0 ; i < contactsFound.size() ; i++)
	{
		Contact * contact = contactsFound[i];
		addContactToContactList(contact);
	}
	alternateColors(listWidget_addressBook);
}

void sflphone_kdeView::alternateColors(QListWidget * listWidget)
{
	qDebug() << "alternateColors";
	for(int i = 0 ; i < listWidget->count(); i++)
	{
		QListWidgetItem* item = listWidget->item(i);
		QBrush c = (i % 2 == 1) ? palette().base() : palette().alternateBase();
		item->setBackground( c );
	}
	listWidget->setUpdatesEnabled( true );

}

QVector<Contact *> sflphone_kdeView::findContactsInKAddressBook(QString textSearched, bool & full)
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	MapStringInt addressBookSettings = configurationManager.getAddressbookSettings().value();
	int maxResults = addressBookSettings[ADDRESSBOOK_MAX_RESULTS];
	int typesDisplayed = phoneNumberTypesDisplayed();
	bool displayPhoto = addressBookSettings[ADDRESSBOOK_DISPLAY_CONTACT_PHOTO];
	
	AddressBook * ab = KABC::StdAddressBook::self();
	QVector<Contact *> results = QVector<Contact *>();
	AddressBook::Iterator it;
	full = false;
	int k = 0;
	for ( it = ab->begin(); it != ab->end() && !full ; it++ ) {
		if(it->name().contains(textSearched, Qt::CaseInsensitive) || it->nickName().contains(textSearched, Qt::CaseInsensitive))
		{
			for(int i = 0 ; i < it->phoneNumbers().count() ; i++)
			{
				int typeFlag = it->phoneNumbers().at(i).type();
				if((typesDisplayed & typeFlag) != 0)
				{
					results.append(new Contact( *it, it->phoneNumbers().at(i), displayPhoto ));
					k++;
				}
			}
		}
		if(k >= maxResults)
		{
			full = true;
		}
	}
	return results;
}

/**
 * 
 * @return the integer resulting to the flags of the types chosen to be displayed in SFLPhone configuration.
 * useful to sort contacts according to their types.
 */
int sflphone_kdeView::phoneNumberTypesDisplayed()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	MapStringInt addressBookSettings = configurationManager.getAddressbookSettings().value();
	int typesDisplayed = 0;
	if(addressBookSettings[ADDRESSBOOK_DISPLAY_BUSINESS])
	{
		typesDisplayed = typesDisplayed | PhoneNumber::Work;
	}
	if(addressBookSettings[ADDRESSBOOK_DISPLAY_MOBILE])
	{
		typesDisplayed = typesDisplayed | PhoneNumber::Cell;
	}
	if(addressBookSettings[ADDRESSBOOK_DISPLAY_HOME])
	{
		typesDisplayed = typesDisplayed | PhoneNumber::Home;
	}
	return typesDisplayed;
}

void sflphone_kdeView::updateRecordButton()
{
	qDebug() << "updateRecordButton";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	double recVol = callManager.getVolume(RECORD_DEVICE);
	if(recVol == 0.00)
	{
		toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_0));
		toolButton_recVolAlone->setIcon(QIcon(ICON_REC_VOL_0));
	}
	else if(recVol < 0.33)
	{
		toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_1));
		toolButton_recVolAlone->setIcon(QIcon(ICON_REC_VOL_1));
	}
	else if(recVol < 0.67)
	{
		toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_2));
		toolButton_recVolAlone->setIcon(QIcon(ICON_REC_VOL_2));
	}
	else
	{
		toolButton_recVol->setIcon(QIcon(ICON_REC_VOL_3));
		toolButton_recVolAlone->setIcon(QIcon(ICON_REC_VOL_3));
	}
	if(recVol > 0)
	{	
		toolButton_recVol->setChecked(false);
		toolButton_recVolAlone->setChecked(false);
	}
}
void sflphone_kdeView::updateVolumeButton()
{
	qDebug() << "updateVolumeButton";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	double sndVol = callManager.getVolume(SOUND_DEVICE);
	if(sndVol == 0.00)
	{
		toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_0));
		toolButton_sndVolAlone->setIcon(QIcon(ICON_SND_VOL_0));
	}
	else if(sndVol < 0.33)
	{
		toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_1));
		toolButton_sndVolAlone->setIcon(QIcon(ICON_SND_VOL_1));
	}
	else if(sndVol < 0.67)
	{
		toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_2));
		toolButton_sndVolAlone->setIcon(QIcon(ICON_SND_VOL_2));
	}
	else
	{
		toolButton_sndVol->setIcon(QIcon(ICON_SND_VOL_3));
		toolButton_sndVolAlone->setIcon(QIcon(ICON_SND_VOL_3));
	}
	if(sndVol > 0)
	{
		toolButton_sndVol->setChecked(false);
		toolButton_sndVolAlone->setChecked(false);
	}
}


void sflphone_kdeView::updateRecordBar()
{
	qDebug() << "updateRecordBar";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	double recVol = callManager.getVolume(RECORD_DEVICE);
	int value = (int)(recVol * 100);
	slider_recVol->setValue(value);
	slider_recVolAlone->setValue(value);
}
void sflphone_kdeView::updateVolumeBar()
{
	qDebug() << "updateVolumeBar";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	double sndVol = callManager.getVolume(SOUND_DEVICE);
	int value = (int)(sndVol * 100);
	slider_sndVol->setValue(value);
	slider_sndVolAlone->setValue(value);
}

void sflphone_kdeView::updateVolumeControls()
{
	qDebug() << "updateVolumeControls";
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	int display = configurationManager.getVolumeControls();
	int displayDialpad = configurationManager.getDialpad();
	widget_recVol->setVisible(display && displayDialpad);
	widget_sndVol->setVisible(display && displayDialpad);
	widget_recVolAlone->setVisible(display && ! displayDialpad);
	widget_sndVolAlone->setVisible(display && ! displayDialpad);
}

void sflphone_kdeView::updateDialpad()
{
	qDebug() << "updateDialpad";
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	int display = configurationManager.getDialpad();
	widget_dialpad->setVisible(display);
}



/************************************************************
************            Autoconnect             *************
************************************************************/

void sflphone_kdeView::on_action_displayVolumeControls_triggered()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	configurationManager.setVolumeControls();
	updateVolumeControls();
}

void sflphone_kdeView::on_action_displayDialpad_triggered()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	configurationManager.setDialpad();
	updateDialpad();
	updateVolumeControls();
}

void sflphone_kdeView::on_pushButton_1_clicked()      { typeString("1"); }
void sflphone_kdeView::on_pushButton_2_clicked()      { typeString("2"); }
void sflphone_kdeView::on_pushButton_3_clicked()      { typeString("3"); }
void sflphone_kdeView::on_pushButton_4_clicked()      { typeString("4"); }
void sflphone_kdeView::on_pushButton_5_clicked()      { typeString("5"); }
void sflphone_kdeView::on_pushButton_6_clicked()      { typeString("6"); }
void sflphone_kdeView::on_pushButton_7_clicked()      { typeString("7"); }
void sflphone_kdeView::on_pushButton_8_clicked()      { typeString("8"); }
void sflphone_kdeView::on_pushButton_9_clicked()      { typeString("9"); }
void sflphone_kdeView::on_pushButton_0_clicked()      { typeString("0"); }
void sflphone_kdeView::on_pushButton_diese_clicked()  { typeString("#"); }
void sflphone_kdeView::on_pushButton_etoile_clicked() { typeString("*"); }

void sflphone_kdeView::on_lineEdit_searchHistory_textChanged()
{
	qDebug() << "on_lineEdit_searchHistory_textEdited";
	updateSearchHistory();
	updateCallHistory();
	updateWindowCallState();
}

void sflphone_kdeView::on_lineEdit_addressBook_textChanged()
{
	qDebug() << "on_lineEdit_addressBook_textEdited";
	updateAddressBook();
	updateWindowCallState();
}

void sflphone_kdeView::on_slider_recVol_valueChanged(int value)
{
	qDebug() << "on_slider_recVol_valueChanged(" << value << ")";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	callManager.setVolume(RECORD_DEVICE, (double)value / 100.0);
	updateRecordButton();
}

void sflphone_kdeView::on_slider_sndVol_valueChanged(int value)
{
	qDebug() << "on_slider_sndVol_valueChanged(" << value << ")";
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	callManager.setVolume(SOUND_DEVICE, (double)value / 100.0);
	updateVolumeButton();
}


void sflphone_kdeView::on_toolButton_recVol_clicked(bool checked)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "on_toolButton_recVol_clicked().";
	if(!checked)
	{
		qDebug() << "checked";
		toolButton_recVol->setChecked(false);
		toolButton_recVolAlone->setChecked(false);
		slider_recVol->setEnabled(true);
		slider_recVolAlone->setEnabled(true);
		callManager.setVolume(RECORD_DEVICE, (double)slider_recVol->value() / 100.0);
	}
	else
	{
		qDebug() << "unchecked";
		toolButton_recVol->setChecked(true);
		qDebug() << "toolButton_recVolAlone->setChecked(true);";
		toolButton_recVolAlone->setChecked(true);
		slider_recVol->setEnabled(false);
		slider_recVolAlone->setEnabled(false);
		callManager.setVolume(RECORD_DEVICE, 0.0);
	}
	updateRecordButton();
}


void sflphone_kdeView::on_toolButton_sndVol_clicked(bool checked)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	qDebug() << "on_toolButton_sndVol_clicked().";
	if(!checked)
	{
		qDebug() << "checked";
		toolButton_sndVol->setChecked(false);
		toolButton_sndVolAlone->setChecked(false);
		slider_sndVol->setEnabled(true);
		slider_sndVolAlone->setEnabled(true);
		callManager.setVolume(SOUND_DEVICE, (double)slider_sndVol->value() / 100.0);
	}
	else
	{
		qDebug() << "unchecked";
		toolButton_sndVol->setChecked(true);
		toolButton_sndVolAlone->setChecked(true);
		slider_sndVol->setEnabled(false);
		slider_sndVolAlone->setEnabled(false);
		callManager.setVolume(SOUND_DEVICE, 0.0);
	}
	updateVolumeButton();
}


void sflphone_kdeView::on_listWidget_callList_currentItemChanged()
{
	qDebug() << "on_listWidget_callList_currentItemChanged";
	updateWindowCallState();
}

void sflphone_kdeView::on_listWidget_callList_itemChanged()
{
	qDebug() << "on_listWidget_callList_itemChanged";
	stackedWidget_screen->setCurrentWidget(page_callList);
}

void sflphone_kdeView::on_listWidget_callList_itemDoubleClicked(QListWidgetItem * item)
{
	qDebug() << "on_listWidget_callList_itemDoubleClicked";
	Call * call = callList->findCallByItem(item);
	call_state state = call->getCurrentState();
	switch(state)
	{
		case CALL_STATE_HOLD:
			actionb(call, CALL_ACTION_HOLD);
			break;
		case CALL_STATE_DIALING:
			actionb(call, CALL_ACTION_ACCEPT);
			break;
		default:
			qDebug() << "Double clicked an item with no action on double click.";
	}
}

void sflphone_kdeView::on_listWidget_callHistory_itemDoubleClicked(QListWidgetItem * item)
{
	qDebug() << "on_listWidget_callHistory_itemDoubleClicked";
	action_history->setChecked(false);
	stackedWidget_screen->setCurrentWidget(page_callList);
	Call * pastCall = callList->findCallByHistoryItem(item);
	Call * call = callList->addDialingCall(pastCall->getPeerName(), pastCall->getAccountId());
	call->appendItemText(pastCall->getPeerPhoneNumber());
	addCallToCallList(call);
	listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
	actionb(call, CALL_ACTION_ACCEPT);
}


void sflphone_kdeView::on_listWidget_addressBook_itemDoubleClicked(QListWidgetItem * item)
{
	qDebug() << "on_listWidget_addressBook_itemDoubleClicked";
	action_addressBook->setChecked(false);
	stackedWidget_screen->setCurrentWidget(page_callList);
	ContactItemWidget * w = (ContactItemWidget *) (listWidget_addressBook->itemWidget(item));
	Call * call = callList->addDialingCall(w->getContactName());
	call->appendItemText(w->getContactNumber());
	addCallToCallList(call);
	listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
	actionb(call, CALL_ACTION_ACCEPT);
}

void sflphone_kdeView::on_stackedWidget_screen_currentChanged(int index)
{
	qDebug() << "on_stackedWidget_screen_currentChanged";
	KXmlGuiWindow * window = (KXmlGuiWindow * ) this->parent();
	switch(index)
	{
		case 0:
			qDebug() << "Switched to call list screen.";
			window->setWindowTitle(tr2i18n("SFLPhone") + " - " + tr2i18n("Main screen"));
			break;
		case 1:
			qDebug() << "Switched to call history screen.";
			updateCallHistory();
			window->setWindowTitle(tr2i18n("SFLPhone") + " - " + tr2i18n("Call history"));
			break;
		case 2:
			qDebug() << "Switched to address book screen.";
			updateAddressBook();
			window->setWindowTitle(tr2i18n("SFLPhone") + " - " + tr2i18n("Address book"));
			break;
		default:
			qDebug() << "Error : reached an unknown index \"" << index << "\" with stackedWidget_screen.";
			break;
	}
}

void sflphone_kdeView::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);
	if(stackedWidget_screen->currentWidget() == page_callHistory || stackedWidget_screen->currentWidget() == page_addressBook)
	{
		QAction * action_edit = new QAction(&menu);
		action_edit->setText(tr2i18n("Edit before call"));
		connect(action_edit, SIGNAL(triggered()),
		        this  , SLOT(editBeforeCall()));
		menu.addAction(action_edit);
	}
	
	menu.addAction(action_accept);
	menu.addAction(action_refuse);
	menu.addAction(action_hold);
	menu.addAction(action_transfer);
	menu.addAction(action_record);
	menu.addSeparator();
	QVector<Account *> accounts = registeredAccounts();
	for (int i = 0 ; i < accounts.size() ; i++)
	{
		Account * account = accounts.at(i);
		QAction * action = new ActionSetAccountFirst(account, &menu);
		action->setCheckable(true);
		action->setChecked(false);
		if(account == firstRegisteredAccount())
		{
			action->setChecked(true);
		}
		connect(action, SIGNAL(setFirst(Account *)),
		        this  , SLOT(setAccountFirst(Account *)));
		menu.addAction(action);
	}
	menu.exec(event->globalPos());
	
}

void sflphone_kdeView::editBeforeCall()
{
	qDebug() << "editBeforeCall";
	QString name;
	QString number;
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		QListWidgetItem * item = listWidget_callHistory->currentItem();
		if(item)
		{
			Call * call = callList->findCallByHistoryItem(item);
			name = call->getPeerName();
			number = call->getPeerPhoneNumber();
		}
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		QListWidgetItem * item = listWidget_addressBook->currentItem();
		if(item)
		{
			ContactItemWidget * w = (ContactItemWidget *) (listWidget_addressBook->itemWidget(listWidget_addressBook->currentItem()));
			name = w->getContactName();
			number = w->getContactNumber();
		}
	}
	QString newNumber = QInputDialog::getText(this, tr2i18n("Edit before call"), QString(), QLineEdit::Normal, number);
	
	action_history->setChecked(false);
	action_addressBook->setChecked(false);
	stackedWidget_screen->setCurrentWidget(page_callList);
	Call * call = callList->addDialingCall(name);
	call->appendItemText(newNumber);
	addCallToCallList(call);
	listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
	actionb(call, CALL_ACTION_ACCEPT);
}

void sflphone_kdeView::setAccountFirst(Account * account)
{
	qDebug() << "setAccountFirst : " << account->getAlias();
	getAccountList()->setAccountFirst(account);
}

void sflphone_kdeView::on_listWidget_callHistory_currentItemChanged()
{
	qDebug() << "on_listWidget_callHistory_currentItemChanged";
	updateWindowCallState();
}

void sflphone_kdeView::on_listWidget_addressBook_currentItemChanged()
{
	qDebug() << "on_listWidget_addressBook_currentItemChanged";
	updateWindowCallState();
}

void sflphone_kdeView::on_action_configureAccounts_triggered()
{
	configDialog->loadOptions();
	configDialog->setPage(PAGE_ACCOUNTS);
	configDialog->show();
}

void sflphone_kdeView::on_action_configureAudio_triggered()
{
	configDialog->loadOptions();
	configDialog->setPage(PAGE_AUDIO);
	configDialog->show();
}

void sflphone_kdeView::on_action_configureSflPhone_triggered()
{
	sflphone_kdeView::configDialog->loadOptions();
	configDialog->setPage(PAGE_GENERAL);
	configDialog->show();
}

void sflphone_kdeView::on_action_accountCreationWizard_triggered()
{
	wizard->show();
}
	

void sflphone_kdeView::on_action_accept_triggered()
{
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Calling when no item is selected. Opening an item.";
			Call * call = callList->addDialingCall();
			addCallToCallList(call);
			listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		}
		else
		{
			Call * call = callList->findCallByItem(item);
			if(!call)
			{
				qDebug() << "Error : Accept triggered on unexisting call.";
			}
			else
			{
				int state = call->getState();
				if(state == CALL_STATE_RINGING || state == CALL_STATE_CURRENT || state == CALL_STATE_HOLD || state == CALL_STATE_BUSY)
				{
					qDebug() << "Calling when item currently ringing, current, hold or busy. Opening an item.";
					Call * call = callList->addDialingCall();
					addCallToCallList(call);
					listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
				}
				else
				{
					actionb(call, CALL_ACTION_ACCEPT);
				}
			}
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		action_history->setChecked(false);
		stackedWidget_screen->setCurrentWidget(page_callList);
		
		Call * pastCall = callList->findCallByHistoryItem(listWidget_callHistory->currentItem());
		Call * call = callList->addDialingCall(pastCall->getPeerName());
		call->appendItemText(pastCall->getPeerPhoneNumber());
		addCallToCallList(call);
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		actionb(call, CALL_ACTION_ACCEPT);
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		action_addressBook->setChecked(false);
		stackedWidget_screen->setCurrentWidget(page_callList);
		ContactItemWidget * w = (ContactItemWidget *) (listWidget_addressBook->itemWidget(listWidget_addressBook->currentItem()));
		Call * call = callList->addDialingCall(w->getContactName());
		call->appendItemText(w->getContactNumber());
		addCallToCallList(call);
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		actionb(call, CALL_ACTION_ACCEPT);
	}
}

void sflphone_kdeView::on_action_refuse_triggered()
{
	if(stackedWidget_screen->currentWidget() == page_callList)
	{
		QListWidgetItem * item = listWidget_callList->currentItem();
		if(!item)
		{
			qDebug() << "Error : Hanging up when no item selected. Should not happen.";
		}
		else
		{
			action(item, CALL_ACTION_REFUSE);
		}
	}
	if(stackedWidget_screen->currentWidget() == page_callHistory)
	{
		lineEdit_searchHistory->clear();
	}
	if(stackedWidget_screen->currentWidget() == page_addressBook)
	{
		lineEdit_addressBook->clear();
	}
}

void sflphone_kdeView::on_action_hold_triggered()
{
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		qDebug() << "Error : Holding when no item selected. Should not happen.";
	}
	else
	{
		action(item, CALL_ACTION_HOLD);
	}
}

void sflphone_kdeView::on_action_transfer_triggered()
{
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		qDebug() << "Error : Transfering when no item selected. Should not happen.";
	}
	else
	{
		action(item, CALL_ACTION_TRANSFER);
	}
}

void sflphone_kdeView::on_action_record_triggered()
{
	QListWidgetItem * item = listWidget_callList->currentItem();
	if(!item)
	{
		qDebug() << "Error : Recording when no item selected. Should not happen.";
	}
	else
	{
		action(item, CALL_ACTION_RECORD);
	}
}

void sflphone_kdeView::on_action_history_triggered(bool checked)
{
	if(checked == true)
	{
 		action_addressBook->setChecked(false);
		stackedWidget_screen->setCurrentWidget(page_callHistory);
	}
	else
	{
		stackedWidget_screen->setCurrentWidget(page_callList);
	}
	updateWindowCallState();
}

void sflphone_kdeView::on_action_addressBook_triggered(bool checked)
{
	if(checked == true)
	{
		action_history->setChecked(false);
		stackedWidget_screen->setCurrentWidget(page_addressBook);
		if(lineEdit_addressBook->text().isEmpty())
		{	lineEdit_addressBook->setFocus(Qt::OtherFocusReason);	}
	}
	else
	{
		stackedWidget_screen->setCurrentWidget(page_callList);
	}
	updateWindowCallState();
}

void sflphone_kdeView::on_action_mailBox_triggered()
{
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	QString account = firstAccountId();
		QString mailBoxNumber = configurationManager.getAccountDetails(account).value()[ACCOUNT_MAILBOX];
		Call * call = callList->addDialingCall();
		call->appendItemText(mailBoxNumber);
		addCallToCallList(call);
		listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
		actionb(call, CALL_ACTION_ACCEPT);
}

void sflphone_kdeView::on1_callStateChanged(const QString &callID, const QString &state)
{
	qDebug() << "Signal : Call State Changed for call  " << callID << " . New state : " << state;
	Call * call = callList->findCallByCallId(callID);
	if(!call)
	{
		if(state == CALL_STATE_CHANGE_RINGING)
		{
			call = callList->addRingingCall(callID);
			addCallToCallList(call);
		}
		else
		{
			qDebug() << "Call doesn't exist in this client. Might have been initialized by another client instance before this one started.";
			return;
		}
	}
	else
	{
		call->stateChanged(state);
	}
	updateCallItem(call);
	updateWindowCallState();
}

void sflphone_kdeView::on1_error(MapStringString details)
{
	qDebug() << "Signal : Daemon error : " << details;
}

void sflphone_kdeView::on1_incomingCall(const QString &accountID, const QString & callID)
{
	qDebug() << "Signal : Incoming Call ! ID = " << callID;
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	Call * call = callList->addIncomingCall(callID);
	addCallToCallList(call);
	listWidget_callList->setCurrentRow(listWidget_callList->count() - 1);
	SFLPhone * window = (SFLPhone * ) this->parent();
	window->trayIconSignal();
	if(configurationManager.popupMode())
	{
		window->putForeground();
	}
	if(configurationManager.getNotify())
	{
		window->sendNotif(call->getPeerName().isEmpty() ? call->getPeerPhoneNumber() : call->getPeerName());
	}
}

void sflphone_kdeView::on1_incomingMessage(const QString &accountID, const QString &message)
{
	qDebug() << "Signal : Incoming Message ! ";
}

void sflphone_kdeView::on1_voiceMailNotify(const QString &accountID, int count)
{
	qDebug() << "Signal : VoiceMail Notify ! " << count << " new voice mails for account " << accountID;
}

void sflphone_kdeView::on1_volumeChanged(const QString &device, double value)
{
	qDebug() << "Signal : Volume Changed !";
	if(! (toolButton_recVol->isChecked() && value == 0.0))
		updateRecordBar();
	if(! (toolButton_sndVol->isChecked() && value == 0.0))
		updateVolumeBar();
}




#include "sflphone_kdeview.moc"
