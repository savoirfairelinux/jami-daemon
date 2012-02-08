/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>         *
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
#ifndef DIALPAD_H
#define DIALPAD_H

#include <QWidget>
#include <QPushButton>

//Qt
class QGridLayout;

///@class DialpadButton the 12 button of the dialpad
class DialpadButton : public QPushButton
{
   Q_OBJECT
public:
   DialpadButton(QWidget* parent, const QString& value): QPushButton(parent),m_Value(value) {
      connect(this,SIGNAL(clicked()),this,SLOT(sltClicked()));
   }
private slots:
   void sltClicked() { emit typed(m_Value); }
private:
   QString m_Value;
signals:
   void typed(QString&);
};


///@class Dialpad A widget that representing a phone dialpad with associated numbers and letters
class Dialpad : public QWidget
{
Q_OBJECT

private:
   //Attributes
   QGridLayout*    gridLayout;
   DialpadButton** m_pButtons;

   static const char* m_pNumbers[];
   static const char* m_pTexts  [];
   static const int m_Spacing    = 5  ;
   static const int m_NumberSize = 14 ;
   static const int m_TextSize   = 8  ;

public:
    Dialpad(QWidget *parent = 0);

private slots:
   void clicked(QString& text);

signals:
   void typed(QString text);
};

#endif