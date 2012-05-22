/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

#include "SortableDockCommon.h"

//Qt
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QString>

//KDE
#include <KLocale>

//SFLPhone
#include "../lib/Call.h"
#include "../lib/Contact.h"
#include "../lib/CallModel.h"
#include "AkonadiBackend.h"

///StaticEventHandler constructor
StaticEventHandler::StaticEventHandler(QObject* parent, QStringList* list) : QObject(parent),m_pList(list)
{
   QTimer* timer = new QTimer(this);
   connect(timer, SIGNAL(timeout()), this, SLOT(update()));
   timer->start(86400000); //1 day
   update();
}

///Update the days constant, necessary to cycle after midnight
void StaticEventHandler::update()
{
   (*m_pList)= {
      i18n("Today"                                                    ),//0
      i18n("Yesterday"                                                ),//1
      i18n(QDate::currentDate().addDays(-2).toString("dddd").toAscii()),//2
      i18n(QDate::currentDate().addDays(-3).toString("dddd").toAscii()),//3
      i18n(QDate::currentDate().addDays(-4).toString("dddd").toAscii()),//4
      i18n(QDate::currentDate().addDays(-5).toString("dddd").toAscii()),//5
      i18n(QDate::currentDate().addDays(-6).toString("dddd").toAscii()),//6
      i18n("Last week"                                                ),//7
      i18n("Two weeks ago"                                            ),//8
      i18n("Three weeks ago"                                          ),//9
      i18n("Last month"                                               ),//10
      i18n("Two months ago"                                           ),//11
      i18n("Three months ago"                                         ),//12
      i18n("Four months ago"                                          ),//13
      i18n("Five months ago"                                          ),//14
      i18n("Six months ago"                                           ),//15
      i18n("Seven months ago"                                         ),//16
      i18n("Eight months ago"                                         ),//17
      i18n("Nine months ago"                                          ),//18
      i18n("Ten months ago"                                           ),//19
      i18n("Eleven months ago"                                        ),//20
      i18n("Twelve months ago"                                        ),//21
      i18n("Last year"                                                ),//22
      i18n("Very long time ago"                                       ),//23
      i18n("Never"                                                    ) //24
   };
}
