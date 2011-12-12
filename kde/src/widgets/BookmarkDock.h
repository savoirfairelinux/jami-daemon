/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Valle <emmanuel.lepage@savoirfairelinux.com >*
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
 **************************************************************************/
#ifndef BOOKMARK_DOCK_H
#define BOOKMARK_DOCK_H

#include <QtGui/QDockWidget>

//Qt
class QTreeWidget;
class QSplitter;

//KDE
class KLineEdit;

//SFLPhone
class HistoryTreeItem;

typedef QList<HistoryTreeItem*> BookmarkList;
class BookmarkDock : public QDockWidget {
   Q_OBJECT
public:
   BookmarkDock(QWidget* parent);
   virtual ~BookmarkDock();

   //Mutators
   void addBookmark(QString phone);
private:
   //Attributes
   QTreeWidget*  m_pItemView;
   KLineEdit*    m_pFilterLE;
   QSplitter*    m_pSplitter;
   BookmarkList  m_pBookmark;

   //Mutators
   void addBookmark_internal(QString phone);
private slots:
   void filter(QString text);
};

#endif