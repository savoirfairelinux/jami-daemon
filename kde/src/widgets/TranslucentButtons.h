/***************************************************************************
 *   Copyright (C) 2011 by Savoir-Faire Linux                              *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
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
#ifndef TRANSLUCENTBUTTONS_H
#define TRANSLUCENTBUTTONS_H
#include <QtGui/QPushButton>
#include <QtGui/QPen>

class QTimer;
class QMimeData;
class QImage;

///TranslucentButtons: Fancy buttons for the call widget
class TranslucentButtons : public QPushButton
{
   Q_OBJECT
public:
   //Constructor
   TranslucentButtons(QWidget* parent);
   ~TranslucentButtons();

   //Setters
   void setHoverState(bool hover);
   void setPixmap(QImage* img);

protected:
   //Reimplementation
   virtual void paintEvent(QPaintEvent* event);
   virtual void dragEnterEvent ( QDragEnterEvent *e );
   virtual void dragMoveEvent  ( QDragMoveEvent  *e );
   virtual void dragLeaveEvent ( QDragLeaveEvent *e );
   virtual void dropEvent      ( QDropEvent      *e );

private:
   //Attributes
   bool    m_enabled     ;
   uint    m_step        ;
   QTimer* m_pTimer      ;
   QColor  m_CurrentColor;
   QPen    m_Pen         ;
   bool    m_CurrentState;
   QImage* m_pImg        ;

public slots:
   void setVisible(bool enabled);
private slots:
   void changeVisibility();
signals:
   ///Emitted when data is dropped on the button
   void dataDropped(QMimeData*);
};
#endif
