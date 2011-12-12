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
#ifndef HISTORY_DOCK_H
#define HISTORY_DOCK_H

#include <QtGui/QDockWidget>
#include <QtGui/QTreeWidget>
#include <QtCore/QDate>

//Qt
class QTreeWidgetItem;
class QString;
class QTreeWidget;
class QComboBox;
class QLabel;
class QCheckBox;
class QPushButton;
class QDate;

//KDE
class KLineEdit;
class KDateWidget;

//SFLPhone
class HistoryTreeItem;
class HistoryTree;

//Typedef
typedef QList<HistoryTreeItem*> HistoryList;

///@class HistoryDock Dock to see the previous SFLPhone calls
class HistoryDock : public QDockWidget {
   Q_OBJECT
   
public:
   //Friends
   friend class KeyPressEater;

   //Constructors
   HistoryDock(QWidget* parent);
   virtual ~HistoryDock();
   
private:
   //Enum
   enum SortBy {
      Date       = 0,
      Name       = 1,
      Popularity = 2,
      Duration   = 3
   };

   //Getters
   QString getIdentity(HistoryTreeItem* item);

   //Attributes
   HistoryTree*  m_pItemView        ;
   KLineEdit*    m_pFilterLE        ;
   QComboBox*    m_pSortByCBB       ;
   QLabel*       m_pSortByL         ;
   QLabel*       m_pFromL           ;
   QLabel*       m_pToL             ;
   KDateWidget*  m_pFromDW          ;
   KDateWidget*  m_pToDW            ;
   QCheckBox*    m_pAllTimeCB       ;
   QPushButton*  m_pLinkPB          ;
   HistoryList   m_pHistory         ;
   QDate         m_pCurrentFromDate ;
   QDate         m_pCurrentToDate   ;

   //Mutator
   void updateLinkedDate(KDateWidget* item, QDate& prevDate, QDate& newDate);
   
public slots:
   void enableDateRange(bool enable);
   virtual void keyPressEvent(QKeyEvent* event);

private slots:
   void filter               (QString text );
   void updateLinkedFromDate (QDate   date );
   void updateLinkedToDate   (QDate   date );
   void reload               (             );
   void updateContactInfo    (             );
};


///@class HistoryTree Simple tree view with additional keybpard filter
class HistoryTree : public QTreeWidget {
   Q_OBJECT
public:
   HistoryTree(QWidget* parent) : QTreeWidget(parent) {}
   virtual QMimeData* mimeData( const QList<QTreeWidgetItem *> items) const;
   bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
};

class KeyPressEater : public QObject
{
   Q_OBJECT
public:
   KeyPressEater(HistoryDock* parent) : QObject(parent) {
      m_pDock =  parent;
   }
protected:
   bool eventFilter(QObject *obj, QEvent *event);
private:
   HistoryDock* m_pDock;
};

#endif