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
#ifndef ITEM_H
#define ITEM_H

#include <QObject>
#include <QListWidgetItem>
#include <QWidget>

/**
	@author Jérémy Quentin <jeremy.quentin@gmail.com>
	Represents an item of a list, that is displayed
	by an QListWidgetItem with a QWidget inside.
	The two objects are contained in this class, but their
	initializations are pure virtual.
	The template class WIDGET_TYPE should be derived from
	QWidget.
	The implementation of initItem should call initItemWidget
*/
template<class WIDGET_TYPE>class Item
{
protected:
	QListWidgetItem * item;
	WIDGET_TYPE * itemWidget;
	

public:
	/**
	 *  Would be great to take the QListWidget as attribute
	 *  to be able to add the itemWidget to the item in the list.
	 *  For the moment, we have to do it from outside.
	 */
	Item(/*QListWidget *list=0*/)
	{
		item = NULL;
		itemWidget = NULL;
	}
	
	/**
	 *   Be careful that it is not already deleted by QObject
	 *   Commented for safety reasons...
	 */
	virtual ~Item()
	{
// 		delete item;
// 		delete itemWidget;
	}
	
	QListWidgetItem * getItem()
	{
		return item;
	}
	
	WIDGET_TYPE * getItemWidget()
	{
		return itemWidget;
	}
	
	const QListWidgetItem * getItem() const
	{
		return item;
	}
	const WIDGET_TYPE * getItemWidget() const
	{
		return itemWidget;
	}
	
	/**
	 *   Initializes the item and widget
	 *   Implementation should call initItemWidget!
	 */
	virtual void initItem() = 0;
	
protected:
	virtual void initItemWidget() = 0;
	
	
};

#endif
