#ifndef CALL_VIEW
#define CALL_VIEW
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
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/

#include <QtGui/QItemDelegate>
#include <QtGui/QTreeWidget>
#include <QtGui/QPainter>
#include <QtCore/QTimer>
#include "lib/CallModel.h"

//Qt
class QTreeWidgetItem;
class QPushButton;

//KDE
class KLineEdit;

//SFLPhone
class CallTreeItem;

//Typedef
typedef CallModel<CallTreeItem*,QTreeWidgetItem*> TreeWidgetCallModel;

///@class CallTreeItemDelegate Delegates for CallTreeItem
class CallTreeItemDelegate : public QItemDelegate
{
   public:
      CallTreeItemDelegate() { }
      QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index ) const 
      {  
         Q_UNUSED(option)
         Q_UNUSED(index)
         return QSize(0,60); 
      }
};

///@class CallViewOverlay Display overlay on top of the call tree
class CallViewOverlay : public QWidget {
   Q_OBJECT
public:
   CallViewOverlay(QWidget* parent) : QWidget(parent),m_pIcon(0),m_enabled(true),black("black"),m_pTimer(0)
   {
      black.setAlpha(75);
   }
   void setCornerWidget(QWidget* wdg) {
      wdg->setParent(this);
      wdg->setMinimumSize(100,100);
      wdg->resize(100,100);
      wdg->move(width()-100,height()-100);
      m_pIcon = wdg;
   }

   void setVisible(bool enabled) {
      if (m_enabled != enabled) {
         if (m_pTimer) {
            m_pTimer->stop();
            disconnect(m_pTimer);
         }
         m_pTimer = new QTimer(this); //TODO LEAK
         connect(m_pTimer, SIGNAL(timeout()), this, SLOT(changeVisibility()));
         m_step = 0;
         black.setAlpha(0);
         repaint();
         m_pTimer->start(10);
      }
      m_enabled = enabled;
      QWidget::setVisible(enabled);
   }
protected:
   void paintEvent(QPaintEvent* event) {
      QPainter customPainter(this);
      customPainter.fillRect(rect(),black);
   }
   virtual void resizeEvent(QResizeEvent *e) {
      if (m_pIcon) {
         m_pIcon->setMinimumSize(100,100);
         m_pIcon->move(width()-100,height()-100);
      }
   }
private:
   QWidget* m_pIcon;
   uint m_step;
   QTimer* m_pTimer;
   bool m_enabled;
   QColor black;
   
private slots:
   void changeVisibility() {
      m_step++;
      black.setAlpha(0.1*m_step*m_step);
      repaint();
      if (m_step >= 35)
         m_pTimer->stop();
   }
};

///@class CallView Central tree widget managing active calls
class CallView : public QTreeWidget {
   Q_OBJECT
   public:
      CallView                    ( QWidget* parent = 0                                                               );
      Call* getCurrentItem        (                                                                                   );
      QWidget* getWidget          (                                                                                   );
      void setTitle               ( const QString& title                                                              );
      bool selectItem             ( Call* item                                                                        );
      bool removeItem             ( Call* item                                                                        );
      bool dropMimeData           ( QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action  );
      virtual QMimeData* mimeData ( const QList<QTreeWidgetItem *> items                                              ) const;
      bool haveOverlay();
      
   private:
      QTreeWidgetItem* extractItem ( const QString& callId                             );
      QTreeWidgetItem* extractItem ( QTreeWidgetItem* item                             );
      CallTreeItem* insertItem     ( QTreeWidgetItem* item, QTreeWidgetItem* parent=0  );
      CallTreeItem* insertItem     ( QTreeWidgetItem* item, Call* parent               );
      void clearArtefact           ( QTreeWidgetItem* item                             );

      QPushButton*     m_pTransferB;
      KLineEdit*       m_pTransferLE;
      CallViewOverlay* m_pTransferOverlay;
      CallViewOverlay* m_pActiveOverlay;
      Call*            m_pCallPendingTransfer;

   protected:
      virtual void dragEnterEvent ( QDragEnterEvent *e );
      virtual void dragMoveEvent  ( QDragMoveEvent  *e );
      virtual void dragLeaveEvent ( QDragLeaveEvent *e );
      virtual void resizeEvent    ( QResizeEvent    *e );
      bool callToCall        ( QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action );
      bool phoneNumberToCall ( QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action );
      bool contactToCall     ( QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action );
      
   public slots:
      void destroyCall        ( Call* toDestroy);
      void itemDoubleClicked  ( QTreeWidgetItem* item, int column    );
      void itemClicked        ( QTreeWidgetItem* item, int column =0 );
      Call* addCall           ( Call* call, Call* parent =0          );
      Call* addConference     ( Call* conf                           );
      bool conferenceChanged  ( Call* conf                           );
      void conferenceRemoved  ( Call* conf                           );
      void showDropOptions    ( CallTreeItem* widget                 );

      virtual void keyPressEvent(QKeyEvent* event);

   public slots:
      void clearHistory();
      void showTransferOverlay(Call* call);
      void transfer();
      void transferDropEvent(Call* call,QMimeData* data);
      void conversationDropEvent(Call* call,QMimeData* data);
      void hideOverlay();

   signals:
      void itemChanged(Call*);
      
};
#endif
