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
 ***************************************************************************/

#ifndef ACCOUNTITEMWIDGET_H
#define ACCOUNTITEMWIDGET_H

//Base
#include <QWidget>

//Qt
class QCheckBox;
class QLabel;


///@class AccountItemWidget Widget for the config dialog account list
class AccountItemWidget : public QWidget
{
Q_OBJECT

public:
   //Enum
   enum State {Registered, Unregistered, NotWorking};

   //Constructors & Destructors
   AccountItemWidget(QWidget *parent = 0);
   ~AccountItemWidget();

   //Getters
   int  getState  ();
   bool getEnabled();

   //Setters
   void setState       (int        state    );
   void setEnabled     (bool        enabled );
   void setAccountText (const QString& text );

   //Updates
   void updateStateDisplay   ();
   void updateEnabledDisplay ();
   void updateDisplay        ();

private:
   //Attributes
   int        m_State     ;
   bool       m_Enabled   ;
   QLabel*    m_pLed      ;
   QCheckBox* m_pCheckBox ;
   QLabel*    m_pTextLabel;

private slots:
   void on_m_pCheckBox_stateChanged(int state);

signals:
   void checkStateChanged(bool checked);

};

#endif
