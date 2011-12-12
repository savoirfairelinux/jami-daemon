/***************************************************************************
 *   Copyright (C) 2009-2010 by Savoir-Faire Linux                         *
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
 ***************************************************************************/

//Parent
#include "Dialpad.h"

//Qt
#include <QtCore/QDebug>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QGridLayout>

///Constructor
Dialpad::Dialpad(QWidget *parent)
 : QWidget(parent)
{
   gridLayout = new QGridLayout(this);
   gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
   
   pushButton_0      = new QPushButton(this);
   pushButton_1      = new QPushButton(this);
   pushButton_2      = new QPushButton(this);
   pushButton_3      = new QPushButton(this);
   pushButton_4      = new QPushButton(this);
   pushButton_5      = new QPushButton(this);
   pushButton_6      = new QPushButton(this);
   pushButton_7      = new QPushButton(this);
   pushButton_8      = new QPushButton(this);
   pushButton_9      = new QPushButton(this);
   pushButton_diese  = new QPushButton(this);
   pushButton_etoile = new QPushButton(this);
      
   pushButton_0->setObjectName(QString::fromUtf8("pushButton_0"));
   pushButton_1->setObjectName(QString::fromUtf8("pushButton_1"));
   pushButton_2->setObjectName(QString::fromUtf8("pushButton_2"));
   pushButton_3->setObjectName(QString::fromUtf8("pushButton_3"));
   pushButton_4->setObjectName(QString::fromUtf8("pushButton_4"));
   pushButton_5->setObjectName(QString::fromUtf8("pushButton_5"));
   pushButton_6->setObjectName(QString::fromUtf8("pushButton_6"));
   pushButton_7->setObjectName(QString::fromUtf8("pushButton_7"));
   pushButton_8->setObjectName(QString::fromUtf8("pushButton_8"));
   pushButton_9->setObjectName(QString::fromUtf8("pushButton_9"));
   pushButton_diese->setObjectName(QString::fromUtf8("pushButton_diese"));
   pushButton_etoile->setObjectName(QString::fromUtf8("pushButton_etoile"));
   
   gridLayout->addWidget(pushButton_1      , 0, 0 );
   gridLayout->addWidget(pushButton_2      , 0, 1 );
   gridLayout->addWidget(pushButton_3      , 0, 2 );
   gridLayout->addWidget(pushButton_4      , 1, 0 );
   gridLayout->addWidget(pushButton_5      , 1, 1 );
   gridLayout->addWidget(pushButton_6      , 1, 2 );
   gridLayout->addWidget(pushButton_7      , 2, 0 );
   gridLayout->addWidget(pushButton_8      , 2, 1 );
   gridLayout->addWidget(pushButton_9      , 2, 2 );
   gridLayout->addWidget(pushButton_etoile , 3, 0 );
   gridLayout->addWidget(pushButton_0      , 3, 1 );
   gridLayout->addWidget(pushButton_diese  , 3, 2 );
   
   fillButtons();
   
   QMetaObject::connectSlotsByName(this);
}

///Make the buttons
void Dialpad::fillButtons()
{
   QHBoxLayout * layout;
   QLabel * number;
   QLabel * text;
   int spacing    = 5  ;
   int numberSize = 14 ;
   int textSize   = 8  ;
   
   QPushButton * buttons[12] = 
       {pushButton_1,      pushButton_2,   pushButton_3, 
        pushButton_4,      pushButton_5,   pushButton_6, 
        pushButton_7,      pushButton_8,   pushButton_9, 
        pushButton_etoile, pushButton_0,   pushButton_diese};
        
   QString numbers[12] = 
       {"1", "2", "3", 
        "4", "5", "6", 
        "7", "8", "9", 
        "*", "0", "#"};
   
   QString texts[12] = 
       {  ""  ,  "abc",  "def" , 
        "ghi" ,  "jkl",  "mno" , 
        "pqrs",  "tuv",  "wxyz", 
          ""  ,   ""  ,   ""   };
   
   for(int i = 0 ; i < 12 ; i++) {
      layout = new QHBoxLayout();
      layout->setSpacing(spacing);
      number = new QLabel(numbers[i]);
      number->setFont(QFont("", numberSize));
      layout->addWidget(number);
      number->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      text = new QLabel(texts[i]);
      text->setFont(QFont("", textSize));
      layout->addWidget(text);
      buttons[i]->setLayout(layout);
      buttons[i]->setMinimumHeight(30);
      buttons[i]->setText("");
   }
}

///Slots
void Dialpad::on_pushButton_1_clicked()      { emit typed("1"); }
void Dialpad::on_pushButton_2_clicked()      { emit typed("2"); }
void Dialpad::on_pushButton_3_clicked()      { emit typed("3"); }
void Dialpad::on_pushButton_4_clicked()      { emit typed("4"); }
void Dialpad::on_pushButton_5_clicked()      { emit typed("5"); }
void Dialpad::on_pushButton_6_clicked()      { emit typed("6"); }
void Dialpad::on_pushButton_7_clicked()      { emit typed("7"); }
void Dialpad::on_pushButton_8_clicked()      { emit typed("8"); }
void Dialpad::on_pushButton_9_clicked()      { emit typed("9"); }
void Dialpad::on_pushButton_0_clicked()      { emit typed("0"); }
void Dialpad::on_pushButton_diese_clicked()  { emit typed("#"); }
void Dialpad::on_pushButton_etoile_clicked() { emit typed("*"); }
