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
#ifndef CONTACT_H
#define CONTACT_H

#include <QtGui/QListWidgetItem>
#include <QtGui/QWidget>

#include <kabc/addressee.h>

using namespace KABC;

/**
	@author Jérémy Quentin <jeremy.quentin@gmail.com>
*/
class Contact{
private:
	QListWidgetItem * item;
	QWidget * itemWidget;
	QString firstName;
	QString secondName;
	QString nickName;
	QString phoneNumber;
	QString photo;
	
private:
	void initItem();

public:
    Contact(Addressee addressee, QString number);

    ~Contact();
    
    QListWidgetItem * getItem();
    
    QWidget * getItemWidget();

};

#endif
