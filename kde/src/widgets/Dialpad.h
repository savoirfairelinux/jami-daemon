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
#ifndef DIALPAD_H
#define DIALPAD_H

#include <QWidget>
#include <QPushButton>
#include <QGridLayout>

/**
A widget that represents a phone dialpad, with numbers and letters associated.

   @author Jérémy Quentin <jeremy.quentin@gmail.com>
*/
class Dialpad : public QWidget
{
Q_OBJECT

private:
   QGridLayout * gridLayout;
   QPushButton * pushButton_0;
   QPushButton * pushButton_1;
   QPushButton * pushButton_2;
   QPushButton * pushButton_3;
   QPushButton * pushButton_4;
   QPushButton * pushButton_5;
   QPushButton * pushButton_6;
   QPushButton * pushButton_7;
   QPushButton * pushButton_8;
   QPushButton * pushButton_9;
   QPushButton * pushButton_diese;
   QPushButton * pushButton_etoile;

public:
    Dialpad(QWidget *parent = 0);

//     ~Dialpad();

private:
   void fillButtons();

private slots:
   void on_pushButton_1_clicked();
   void on_pushButton_2_clicked();
   void on_pushButton_3_clicked();
   void on_pushButton_4_clicked();
   void on_pushButton_5_clicked();
   void on_pushButton_6_clicked();
   void on_pushButton_7_clicked();
   void on_pushButton_8_clicked();
   void on_pushButton_9_clicked();
   void on_pushButton_0_clicked();
   void on_pushButton_diese_clicked();
   void on_pushButton_etoile_clicked();

signals:
   /**
    *   This signal is emitted when the user types a button of the dialpad.
    * @param  text the text of the button typed by the user.
    */
   void typed(QString text);
};

#endif
