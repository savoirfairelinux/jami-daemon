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
 **************************************************************************/
#ifndef CALLTREE_ITEM_H
#define CALLTREE_ITEM_H

#include <QtCore/QVector>
#include <QtCore/QList>
#include <QtGui/QWidget>

//Qt
class QLabel;

//KDE
class KIcon;

//SFLPhone
class Call;

class CallTreeItem : public QWidget
{
   Q_OBJECT
 public:
    CallTreeItem(QWidget* parent =0);
    ~CallTreeItem();
    
    Call* call() const;
    void setCall(Call *call);
    static const char * callStateIcons[12];
    
 private:
    Call *itemCall;

    QLabel* labelIcon;
    QLabel* labelPeerName;
    QLabel* labelCallNumber2;
    QLabel* labelTransferPrefix;
    QLabel* labelTransferNumber;
    QLabel* labelCodec;
    QLabel* labelSecure;
    
    QWidget* historyItemWidget;
    QLabel* labelHistoryIcon;
    QLabel* labelHistoryPeerName;
    QLabel* labelHistoryCallNumber;
    QLabel* labelHistoryTime;
    bool init;

public slots:
   void updated();
signals:
   void over(Call*);  
 };

#endif // CALLTREE_ITEM_H
