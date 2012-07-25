/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
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
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#ifndef HISTORY_DOCK_H
#define HISTORY_DOCK_H

#include <QtGui/QDockWidget>
#include <QtGui/QTreeWidget>
#include <QtCore/QDate>
#include "../klib/SortableDockCommon.h"
#include "CategorizedTreeWidget.h"
#include "CallTreeItem.h"
#include <QtGui/QTreeWidgetItem>

//Qt
class QTreeWidgetItem;
class QString;
class QTreeWidget;
class KComboBox;
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
class KeyPressEater;
class QNumericTreeWidgetItem;

//Typedef
typedef QList<HistoryTreeItem*> HistoryList;
typedef QHash<QString,QNumericTreeWidgetItem*> GroupHash;

///HistoryDock: Dock to see the previous SFLPhone calls
class HistoryDock : public QDockWidget, public SortableDockCommon<CallTreeItem*,QTreeWidgetItem*> {
   Q_OBJECT

public:
   //Friends
   friend class KeyPressEater;

   //Constructors
   HistoryDock(QWidget* parent);
   virtual ~HistoryDock();

private:
   //Attributes
   HistoryTree*   m_pItemView        ;
   KLineEdit*     m_pFilterLE        ;
   KComboBox*     m_pSortByCBB       ;
   QLabel*        m_pSortByL         ;
   QLabel*        m_pFromL           ;
   QLabel*        m_pToL             ;
   KDateWidget*   m_pFromDW          ;
   KDateWidget*   m_pToDW            ;
   QCheckBox*     m_pAllTimeCB       ;
   QPushButton*   m_pLinkPB          ;
   HistoryList    m_History          ;
   QDate          m_CurrentFromDate  ;
   QDate          m_CurrentToDate    ;
   KeyPressEater* m_pKeyPressEater   ;
   GroupHash      m_hGroup           ;

   //Mutator
   void updateLinkedDate(KDateWidget* item, QDate& prevDate, QDate& newDate);

public slots:
   void enableDateRange(bool disable);
   virtual void keyPressEvent(QKeyEvent* event);

private slots:
   void filter               ( QString text );
   void updateLinkedFromDate ( QDate   date );
   void updateLinkedToDate   ( QDate   date );
   void reload               (              );
   void updateContactInfo    (              );
   void newHistoryCall       ( Call*   call );
};


///HistoryTree: Simple tree view with additional keybpard filter
class HistoryTree : public CategorizedTreeWidget {
   Q_OBJECT
public:
   HistoryTree(QWidget* parent) : CategorizedTreeWidget(parent) {}
   virtual QMimeData* mimeData( const QList<QTreeWidgetItem *> items) const;
   bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);
};

///KeyPressEater: Intercept each keypress to manage it globally
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
