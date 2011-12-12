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
 
#include "AccountItemWidget.h"

#include <QtGui/QHBoxLayout>
#include <QtCore/QDebug>

#include "lib/sflphone_const.h"

///Constructor
AccountItemWidget::AccountItemWidget(QWidget *parent)
 : QWidget(parent)
{
   checkBox = new QCheckBox(this);
   checkBox->setObjectName("checkBox");
   
   led = new QLabel();
   led->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
   textLabel = new QLabel();
   
   QSpacerItem * horizontalSpacer = new QSpacerItem(16777215, 20, QSizePolicy::Preferred, QSizePolicy::Minimum);
   QHBoxLayout* hlayout = new QHBoxLayout();
   hlayout->setContentsMargins( 0,0,0,0          );
   hlayout->addWidget         ( checkBox         );
   hlayout->addWidget         ( textLabel        );
   hlayout->addItem           ( horizontalSpacer );
   hlayout->addWidget         ( led              );
   
   this->setLayout(hlayout);
   state = Unregistered;
   enabled = false;
   updateDisplay();
   
   QMetaObject::connectSlotsByName(this);
}

///Destructor
AccountItemWidget::~AccountItemWidget()
{
   delete led;
   delete checkBox;
   delete textLabel;
}

///Update the LED widget color
void AccountItemWidget::updateStateDisplay()
{
   switch(state) {
      case Registered:
         led->setPixmap(QPixmap(ICON_ACCOUNT_LED_GREEN));
         break;
      case Unregistered:
         led->setPixmap(QPixmap(ICON_ACCOUNT_LED_GRAY));
         break;
      case NotWorking:
         led->setPixmap(QPixmap(ICON_ACCOUNT_LED_RED));
         break;
      default:
         qDebug() << "Calling AccountItemWidget::setState with value " << state << ", not part of enum AccountItemWidget::State.";
   }
}

///If this item is enable or not
void AccountItemWidget::updateEnabledDisplay()
{
   checkBox->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
}

///Update the widget
void AccountItemWidget::updateDisplay()
{
   updateStateDisplay();
   updateEnabledDisplay();
}

///Set the model state of the widget
void AccountItemWidget::setState(int state)
{
   this->state = state;
   updateStateDisplay();
}

///If this widget is enabled or not
void AccountItemWidget::setEnabled(bool enabled)
{
   this->enabled = enabled;
   updateEnabledDisplay();
}

///Set the widget text
void AccountItemWidget::setAccountText(QString text)
{
   this->textLabel->setText(text);
}

///Is this widget enabled
bool AccountItemWidget::getEnabled()
{
   return checkBox->checkState();
}

///Model state changed
void AccountItemWidget::on_checkBox_stateChanged(int state)
{
   emit checkStateChanged(state == Qt::Checked);
}
