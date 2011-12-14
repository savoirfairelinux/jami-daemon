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

//Parent
#include "AccountItemWidget.h"

//Qt
#include <QtGui/QHBoxLayout>

//KDE
#include <KDebug>

//SFLPhone library
#include "lib/sflphone_const.h"

///Constructor
AccountItemWidget::AccountItemWidget(QWidget *parent)
 : QWidget(parent)
{
   m_pCheckBox = new QCheckBox(this);
   m_pCheckBox->setObjectName("m_pCheckBox");
   
   m_pLed = new QLabel();
   m_pLed->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
   m_pTextLabel = new QLabel();
   
   QSpacerItem* horizontalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Preferred, QSizePolicy::Minimum);
   QHBoxLayout* hlayout = new QHBoxLayout();
   hlayout->setContentsMargins( 0,0,0,0          );
   hlayout->addWidget         ( m_pCheckBox      );
   hlayout->addWidget         ( m_pTextLabel     );
   hlayout->addItem           ( horizontalSpacer );
   hlayout->addWidget         ( m_pLed           );
   
   this->setLayout(hlayout);
   m_State = Unregistered;
   m_Enabled = false;
   updateDisplay();
   
   QMetaObject::connectSlotsByName(this);
}

///Destructor
AccountItemWidget::~AccountItemWidget()
{
   delete m_pLed;
   delete m_pCheckBox;
   delete m_pTextLabel;
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Update the LED widget color
void AccountItemWidget::updateStateDisplay()
{
   switch(m_State) {
      case Registered:
         m_pLed->setPixmap(QPixmap(ICON_ACCOUNT_LED_GREEN));
         break;
      case Unregistered:
         m_pLed->setPixmap(QPixmap(ICON_ACCOUNT_LED_GRAY));
         break;
      case NotWorking:
         m_pLed->setPixmap(QPixmap(ICON_ACCOUNT_LED_RED));
         break;
      default:
         kDebug() << "Calling AccountItemWidget::setState with value " << m_State << ", not part of enum AccountItemWidget::State.";
   }
}

///If this item is enable or not
void AccountItemWidget::updateEnabledDisplay()
{
   m_pCheckBox->setCheckState(m_Enabled ? Qt::Checked : Qt::Unchecked);
}

///Update the widget
void AccountItemWidget::updateDisplay()
{
   updateStateDisplay();
   updateEnabledDisplay();
}


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Set the model state of the widget
void AccountItemWidget::setState(int state)
{
   m_State = state;
   updateStateDisplay();
}

///If this widget is enabled or not
void AccountItemWidget::setEnabled(bool enabled)
{
   m_Enabled = enabled;
   updateEnabledDisplay();
}

///Set the widget text
void AccountItemWidget::setAccountText(const QString& text)
{
   this->m_pTextLabel->setText(text);
}

///Is this widget enabled
bool AccountItemWidget::getEnabled()
{
   return m_pCheckBox->checkState();
}


/*****************************************************************************
 *                                                                           *
 *                                    SLOTS                                  *
 *                                                                           *
 ****************************************************************************/

///Model state changed
void AccountItemWidget::on_m_pCheckBox_stateChanged(int state)
{
   emit checkStateChanged(state == Qt::Checked);
}
