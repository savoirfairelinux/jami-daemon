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
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

//Parent
#include "Dialpad.h"

//Qt
#include <QtGui/QLabel>
#include <QtGui/QGridLayout>

const char* Dialpad::m_pNumbers[] =
       {"1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "*", "0", "#"};

const char* Dialpad::m_pTexts[12] =
        { ""  ,  "abc",  "def" ,
        "ghi" ,  "jkl",  "mno" ,
        "pqrs",  "tuv",  "wxyz",
          ""  ,   ""  ,   ""   };

///Constructor
Dialpad::Dialpad(QWidget *parent)
 : QWidget(parent),gridLayout(new QGridLayout(this)),m_pButtons(new DialpadButton*[12])
{
   for (uint i=0; i < 12;i++) {
      m_pButtons[i]       = new DialpadButton( this,m_pNumbers[i] );
      QHBoxLayout* layout = new QHBoxLayout  ( m_pButtons[i]      );
      QLabel* number      = new QLabel       ( m_pNumbers[i]      );
      QLabel* text        = new QLabel       ( m_pTexts[i]        );
      m_pButtons[i]->setMinimumHeight(30);
      gridLayout->addWidget( m_pButtons[i],i/3,i%3              );
      number->setFont      ( QFont("", m_NumberSize)            );
      number->setAlignment ( Qt::AlignRight | Qt::AlignVCenter  );
      text->setFont        ( QFont("", m_TextSize)              );
      layout->setSpacing ( m_Spacing );
      layout->addWidget  ( number    );
      layout->addWidget  ( text      );
      connect(m_pButtons[i],SIGNAL(typed(QString&)),this,SLOT(clicked(QString&)));
   }
} //Dialpad

///Destructor
Dialpad::~Dialpad()
{
   delete[] m_pButtons;
   delete gridLayout;
}

///Proxy to make the view more convinient to use
void Dialpad::clicked(QString& text)
{
   emit typed(text);
}
