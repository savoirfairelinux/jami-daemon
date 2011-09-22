/***************************************************************************
 *   Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).     *
 *   All rights reserved.                                                  *
 *   Contact: Nokia Corporation (qt-info@nokia.com)                        *
 *   Author : Mathieu Leduc-Hamel mathieu.leduc-hamel@savoirfairelinux.com *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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

/**
 * http://doc.trolltech.com/4.5/itemviews-editabletreemodel.html
 */

#ifndef HISTORYTREE_ITEM_H
#define HISTORYTREE_ITEM_H

#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#include <QtGui/QWidget>
#include <QtGui/QLabel>
#include <QtGui/QSpacerItem>
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>
#include <KIcon>

#include "lib/Call.h"

class QTreeWidgetItem;
class KAction;
class QMenu;

class HistoryTreeItem : public QWidget
{
   Q_OBJECT
 public:
    HistoryTreeItem(QWidget* parent =0);
    ~HistoryTreeItem();
    
    Call* call() const;
    void setCall(Call *call);
    static const char * callStateIcons[12];

    uint getTimeStamp();
    uint getDuration();
    QString getName();
    QString getPhoneNumber();
    QTreeWidgetItem* getItem();

    void setItem(QTreeWidgetItem* item);
    
 private:
    Call *itemCall;

    QLabel* labelIcon;
    QLabel* labelPeerName;
    QLabel* labelCallNumber2;
    QLabel* m_pTimeL;
    QLabel* m_pDurationL;

    KAction* m_pCallAgain;
    KAction* m_pAddContact;
    KAction* m_pAddToContact;
    KAction* m_pCopy;
    KAction* m_pEmail;
    QMenu*   m_pMenu;

    uint m_pTimeStamp;
    uint m_pDuration;
    QString m_pName;
    QString m_pPhoneNumber;
    QTreeWidgetItem* m_pItem;
    
    bool init;

public slots:
   void updated();
private slots:
   void showContext(const QPoint& pos);
   void sendEmail();
   void callAgain();
   void copy();
   void addContact();
   void addToContact();
signals:
   void over(Call*);
};

#endif // CALLTREE_ITEM_H
