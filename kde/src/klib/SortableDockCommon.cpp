/****************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                               *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

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
   (*m_pList) << 
      "Today"                                                    <<//0
      "Yesterday"                                                <<//1
      QDate::currentDate().addDays(-2).toString("dddd").toAscii()<<//2
      QDate::currentDate().addDays(-3).toString("dddd").toAscii()<<//3
      QDate::currentDate().addDays(-4).toString("dddd").toAscii()<<//4
      QDate::currentDate().addDays(-5).toString("dddd").toAscii()<<//5
      QDate::currentDate().addDays(-6).toString("dddd").toAscii()<<//6
      "Last week"                                                <<//7
      "Two weeks ago"                                            <<//8
      "Three weeks ago"                                          <<//9
      "Last month"                                               <<//10
      "Two months ago"                                           <<//11
      "Three months ago"                                         <<//12
      "Four months ago"                                          <<//13
      "Five months ago"                                          <<//14
      "Six months ago"                                           <<//15
      "Seven months ago"                                         <<//16
      "Eight months ago"                                         <<//17
      "Nine months ago"                                          <<//18
      "Ten months ago"                                           <<//19
      "Eleven months ago"                                        <<//20
      "Twelve months ago"                                        <<//21
      "Last year"                                                <<//22
      "Very long time ago"                                       <<//23
      "Never"                                                     ;//24
}
