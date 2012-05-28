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
class QPushButton;
class QMimeData;
class QTimer;

//KDE
class KIcon;

//SFLPhone
class Call;
class TranslucentButtons;

///@class CallTreeItem Widget for the central call treeview
class CallTreeItem : public QWidget
{
   Q_OBJECT
 public:
    //Constructor
    CallTreeItem(QWidget* parent =0);
    ~CallTreeItem();

    //Getters
    Call* call() const;

    //Setters
    void setCall(Call *call);

    //Const
    static const char* callStateIcons[12];

 private:
    //Attributes
    Call*    m_pItemCall        ;
    bool     m_Init             ;
    bool     m_isHover          ;
    QLabel*  m_pIconL           ;
    QLabel*  m_pPeerL           ;
    QLabel*  m_pCallNumberL     ;
    QLabel*  m_pTransferPrefixL ;
    QLabel*  m_pTransferNumberL ;
    QLabel*  m_pCodecL          ;
    QLabel*  m_pSecureL         ;
    QLabel*  m_pHistoryPeerL    ;
    QLabel*  m_pElapsedL        ;
    QTimer*  m_pTimer           ;
    
    TranslucentButtons* m_pBtnConf ;
    TranslucentButtons* m_pBtnTrans;

  protected:
    //Reimplementation
    virtual void dragEnterEvent ( QDragEnterEvent *e );
    virtual void dragMoveEvent  ( QDragMoveEvent  *e );
    virtual void dragLeaveEvent ( QDragLeaveEvent *e );
    virtual void resizeEvent    ( QResizeEvent    *e );
    virtual void dropEvent      ( QDropEvent      *e );

private slots:
   void transferEvent(QMimeData* data);
   void conversationEvent(QMimeData* data);
   void hide();
   void incrementTimer();

public slots:
   void updated();

signals:
   void over(Call*);
   void changed();
   void showChilds(CallTreeItem*);
   void askTransfer(Call*);
   void transferDropEvent(Call*,QMimeData*);
   void conversationDropEvent(Call*,QMimeData*);
 };

#endif // CALLTREE_ITEM_H
