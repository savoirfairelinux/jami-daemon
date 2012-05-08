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
#include "dlggeneral.h"
#include <QToolButton>
#include <QAction>

#include "conf/ConfigurationSkeleton.h"
#include "conf/ConfigurationDialog.h"

DlgGeneral::DlgGeneral(QWidget *parent)
 : QWidget(parent)
{
   setupUi(this);
   connect(toolButton_historyClear, SIGNAL(clicked()), this, SIGNAL(clearCallHistoryAsked()));

   kcfg_historyMax->setValue(ConfigurationSkeleton::historyMax());
}

DlgGeneral::~DlgGeneral()
{
}

void DlgGeneral::updateWidgets()
{
   
}

void DlgGeneral::updateSettings()
{
   ConfigurationSkeleton::setHistoryMax(kcfg_historyMax->value());
}