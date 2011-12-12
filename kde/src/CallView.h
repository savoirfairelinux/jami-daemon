#ifndef CALL_VIEW
#define CALL_VIEW
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

#include <QtGui/QItemDelegate>
#include <QtGui/QTreeWidget>
#include "lib/CallModel.h"

//Qt
class QTreeWidgetItem;

//SFLPhone
class CallTreeItem;

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

typedef CallModel<CallTreeItem*,QTreeWidgetItem*> TreeWidgetCallModel;

///@class CallView Central tree widget managing active calls
class CallView : public QTreeWidget {
   Q_OBJECT
   public:
      CallView                    ( QWidget* parent = 0                                                               );
      Call* getCurrentItem        (                                                                                   );
      QWidget* getWidget          (                                                                                   );
      void setTitle               ( QString title                                                                     );
      bool selectItem             ( Call* item                                                                        );
      bool removeItem             ( Call* item                                                                        );
      bool dropMimeData           ( QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action  );
      virtual QMimeData* mimeData ( const QList<QTreeWidgetItem *> items                                              ) const;
      
   private:
      QTreeWidgetItem* extractItem ( QString callId                                    );
      QTreeWidgetItem* extractItem ( QTreeWidgetItem* item                             );
      CallTreeItem* insertItem     ( QTreeWidgetItem* item, QTreeWidgetItem* parent=0  );
      CallTreeItem* insertItem     ( QTreeWidgetItem* item, Call* parent               );
      void clearArtefact           ( QTreeWidgetItem* item                             );

   protected:
      void dragEnterEvent( QDragEnterEvent *e) { e->accept(); }
      void dragMoveEvent ( QDragMoveEvent *e)  { e->accept(); }
      bool callToCall        ( QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action );
      bool phoneNumberToCall ( QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action );
      bool contactToCall     ( QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action );
      
   public slots:
      void destroyCall        ( Call* toDestroy);
      void itemDoubleClicked  ( QTreeWidgetItem* item, int column  );
      void itemClicked        ( QTreeWidgetItem* item, int column  );
      Call* addCall           ( Call* call, Call* parent =0        );
      Call* addConference     ( Call* conf                         );
      bool conferenceChanged  ( Call* conf                         );
      void conferenceRemoved  ( Call* conf                         );

      virtual void keyPressEvent(QKeyEvent* event);

   public slots:
      void clearHistory();

   signals:
      void itemChanged(Call*);
      
};
#endif
