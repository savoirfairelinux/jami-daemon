/***************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                              *
 *   Author : Jérémy Quentin                                               *
 *   jeremy.quentin@savoirfairelinux.com                                   *
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

#include "sflphone_const.h"

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
	hlayout->setContentsMargins(0,0,0,0);
	hlayout->addWidget(checkBox);
	hlayout->addWidget(textLabel);
	hlayout->addItem(horizontalSpacer);
	hlayout->addWidget(led);
	this->setLayout(hlayout);
	state = Unregistered;
	enabled = false;
	updateDisplay();
	
	QMetaObject::connectSlotsByName(this);
}


AccountItemWidget::~AccountItemWidget()
{
	delete led;
	delete checkBox;
	delete textLabel;
}


void AccountItemWidget::updateStateDisplay()
{
	switch(state)
	{
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
	
void AccountItemWidget::updateEnabledDisplay()
{
	checkBox->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
}
	
void AccountItemWidget::updateDisplay()
{
	updateStateDisplay();
	updateEnabledDisplay();
}

void AccountItemWidget::setState(int state)
{
	this->state = state;
	updateStateDisplay();
}

void AccountItemWidget::setEnabled(bool enabled)
{
	this->enabled = enabled;
	updateEnabledDisplay();
}

void AccountItemWidget::setAccountText(QString text)
{
	this->textLabel->setText(text);
}
	
bool AccountItemWidget::getEnabled()
{
	return checkBox->checkState();
}

void AccountItemWidget::on_checkBox_stateChanged(int state)
{
	emit checkStateChanged(state == Qt::Checked);
}
