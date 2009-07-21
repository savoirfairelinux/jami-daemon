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

#ifndef CALL_LIST_H
#define CALL_LIST_H

#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtGui/QListWidgetItem>

#include "Call.h"

class CallList : public QObject
{
Q_OBJECT

private:

	QVector<Call *> * calls;

public:

	//Constructors & Destructors
	CallList(QObject * parent = 0);
	~CallList();

	//Getters
	Call * findCallByItem(const QListWidgetItem * item);
	Call * findCallByHistoryItem(const QListWidgetItem * item);
	Call * findCallByCallId(const QString & callId);
	Call * operator[](const QListWidgetItem * item);
	Call * operator[](const QString & callId);
	Call * operator[](int ind);
	int size();

	//Setters
	Call * addDialingCall(const QString & peerName = "", QString account = "");
	Call * addIncomingCall(const QString & callId/*, const QString & from, const QString & account*/);
	Call * addRingingCall(const QString & callId);

	//GSetter
	QString generateCallId();
	
public slots:
	void clearHistory();

};


#endif