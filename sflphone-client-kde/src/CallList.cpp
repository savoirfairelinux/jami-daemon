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

#include "CallList.h"


#include "callmanager_interface_singleton.h"
#include "configurationmanager_interface_singleton.h"

CallList::CallList(QObject * parent)
 : QObject(parent)
{
	CallManagerInterface & callManager = CallManagerInterfaceSingleton::getInstance();
	ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
	QStringList callList = callManager.getCallList();
	qDebug() << "Call List = " << callList;
	calls = new QVector<Call *>();
	for(int i = 0 ; i < callList.size() ; i++)
	{
		calls->append(Call::buildExistingCall(callList[i]));
	}
	MapStringString historyMap = configurationManager.getHistory().value();
	qDebug() << "Call History = " << historyMap;
	QMapIterator<QString, QString> i(historyMap);
	while (i.hasNext()) {
		i.next();
		uint startTimeStamp = i.key().toUInt();
		QStringList param = i.value().split("|");
		QString type = param[0];
		QString number = param[1];
		QString name = param[2];
		uint stopTimeStamp = param[3].toUInt();
		QString account = param[4];
		calls->append(Call::buildHistoryCall(generateCallId(), startTimeStamp, stopTimeStamp, account, name, number, type));
	}
}

CallList::~CallList()
{
	for(int i=0 ; i<size() ; i++)
	{
		delete (*calls)[i];
	}
	delete calls;
}

Call * CallList::operator[](const QListWidgetItem * item)
{
	for(int i = 0 ; i < size() ; i++)
	{
		if ((*calls)[i]->getItem() == item)
		{
			return (*calls)[i];
		}
	}
	return NULL;
}

Call * CallList::findCallByItem(const QListWidgetItem * item)
{
	for(int i = 0 ; i < size() ; i++)
	{
		if ((*calls)[i]->getItem() == item)
		{
			return (*calls)[i];
		}
	}
	return NULL;
}

Call * CallList::findCallByHistoryItem(const QListWidgetItem * item)
{
	for(int i = 0 ; i < size() ; i++)
	{
		if ((*calls)[i]->getHistoryItem() == item)
		{
			return (*calls)[i];
		}
	}
	return NULL;
}

Call * CallList::findCallByCallId(const QString & callId)
{
	for(int i = 0 ; i < size() ; i++)
	{
		if ((*calls)[i]->getCallId() == callId)
		{
			return (*calls)[i];
		}
	}
	return NULL;
}

Call * CallList::operator[](const QString & callId)
{
	for(int i = 0 ; i < size() ; i++)
	{
		if ((*calls)[i]->getCallId() == callId)
		{
			return (*calls)[i];
		}
	}
	return NULL;
}

Call * CallList::operator[](int ind)
{
	return (*calls)[ind];
}


QString CallList::generateCallId()
{
	int id = qrand();
	QString res = QString::number(id);
	return res;
}

int CallList::size()
{
	return calls->size();
}

Call * CallList::addDialingCall(const QString & peerName, QString account)
{
	Call * call = Call::buildDialingCall(generateCallId(), peerName, account);
	calls->append(call);
	return call;
}

Call * CallList::addIncomingCall(const QString & callId/*, const QString & from, const QString & account*/)
{
	Call * call = Call::buildIncomingCall(callId/*, from, account*/);
	calls->append(call);
	return call;
}

Call * CallList::addRingingCall(const QString & callId)
{
	Call * call = Call::buildRingingCall(callId);
	calls->append(call);
	return call;
}

void CallList::clearHistory()
{
	qDebug() << "clearHistory";
	Call * call;
	QMutableVectorIterator<Call *> i(*calls);
	while (i.hasNext()) 
	{
		call = i.next();
		if (call->isHistory()) 
		{	i.remove();	}
	}
}
