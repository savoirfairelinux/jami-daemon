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

//Qt
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

//SFLPhone
#include "../lib/Call.h"
#include "../lib/Contact.h"
#include "../lib/CallModel.h"
#include "AkonadiBackend.h"
#include "HelperFunctions.h"
#include "ConfigurationSkeleton.h"

//Define
#define CALLMODEL_TEMPLATE template<typename CallWidget, typename Index>
#define SORTABLE_T SortableDockCommon<CallWidget,Index>

CALLMODEL_TEMPLATE QStringList         SORTABLE_T::m_slHistoryConst = QStringList();
CALLMODEL_TEMPLATE StaticEventHandler* SORTABLE_T::m_spEvHandler = new StaticEventHandler(0,&(SORTABLE_T::m_slHistoryConst));

CALLMODEL_TEMPLATE SORTABLE_T::SortableDockCommon()
{
   /*if (not m_spEvHandler) {
      m_spEvHandler = new StaticEventHandler(0,&(SORTABLE_T::m_slHistoryConst));
   }*/
}


/*****************************************************************************
 *                                                                           *
 *                                  Helpers                                  *
 *                                                                           *
 ****************************************************************************/

CALLMODEL_TEMPLATE QString SORTABLE_T::timeToHistoryCategory(QDate date)
{
   if (m_slHistoryConst.size() < 10)
      m_spEvHandler->update();

   //m_spEvHandler->update();
   if (QDate::currentDate()  == date || QDate::currentDate()  < date) //The future case would be a bug, but it have to be handled anyway or it will appear in "very long time ago"
      return m_slHistoryConst[HistoryConst::Today];

   //Check for last week
   for (int i=1;i<7;i++) {
      if (QDate::currentDate().addDays(-i)  == date)
         return m_slHistoryConst[i]; //Yesterday to Six_days_ago
   }

   //Check for last month
   for (int i=1;i<4;i++) {
      if (QDate::currentDate().addDays(-(i*7))  >= date && QDate::currentDate().addDays(-(i*7) -7)  < date)
         return m_slHistoryConst[i+Last_week-1]; //Last_week to Three_weeks_ago
   }

   //Check for last year
   for (int i=1;i<12;i++) {
      if (QDate::currentDate().addMonths(-i)  >= date && QDate::currentDate().addMonths((-i) - 1)  < date)
         return m_slHistoryConst[i+Last_month-1]; //Last_month to Twelve_months ago
   }

   if (QDate::currentDate().addYears(-1)  >= date && QDate::currentDate().addYears(-2)  < date)
      return m_slHistoryConst[Last_year];

   //Every other senario
   return m_slHistoryConst[Very_long_time_ago];
}

///Return the list of contact from history (in order, most recently used first)
CALLMODEL_TEMPLATE QHash<Contact*, QDateTime> SORTABLE_T::getContactListByTime(/*ContactList list*/)
{
   const CallMap& history= CallModel<CallWidget,Index>::getHistory();
   QHash<Contact*, QDateTime> toReturn;
   QSet<QString> alreadyUsed;
   QMapIterator<QString, Call*> i(history);
   i.toBack();
   while (i.hasPrevious()) { //Iterate from the end up
      i.previous();
      (alreadyUsed.find(i.value()->getPeerPhoneNumber()) == alreadyUsed.constEnd()); //Don't ask, leave it there Elv13(2012)
      if (alreadyUsed.find(i.value()->getPeerPhoneNumber()) == alreadyUsed.constEnd()) {
         Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(i.value()->getPeerPhoneNumber(),true);
         if (contact && toReturn.find(contact) == toReturn.end()) {
            toReturn[contact] = QDateTime::fromTime_t(i.value()->getStartTimeStamp().toUInt());
         }
         alreadyUsed << i.value()->getPeerPhoneNumber();
      }
   }
   return toReturn;
}

CALLMODEL_TEMPLATE void SORTABLE_T::setHistoryCategory(QList<Call*>& calls,HistorySortingMode mode)
{
   QHash<QString,uint> popularityCount;
   QMap<QString, QList<Call*> > byDate;
   switch (mode) {
      case HistorySortingMode::Date:
         foreach (QString cat, m_slHistoryConst) {
            byDate[cat] = QList<Call*>();
         }
         break;
      case HistorySortingMode::Popularity:
         foreach (Call* call, calls) {
            popularityCount[getIdentity(call)]++;
         }
         break;
      default:
         break;
   }
   foreach (Call* call, calls) {
      QString category;
      switch (mode) {
         case HistorySortingMode::Date:
         {
            category = timeToHistoryCategory(QDateTime::fromTime_t(call->getStartTimeStamp().toUInt()).date());
            byDate[category] <<call;
         }
            break;
         case HistorySortingMode::Name2:
            category = getIdentity(call);
            break;
         case HistorySortingMode::Popularity:
            {
               QString identity = getIdentity(call);
               category = identity+"("+QString::number(popularityCount[identity])+")";
            }
            break;
         case HistorySortingMode::Length:
            category = "TODO";
            break;
         default:
            break;
      }
      call->setProperty("section",category);
   }
   switch (mode) {
      case HistorySortingMode::Date:
         calls.clear();
         foreach (QString cat, m_slHistoryConst) {
            foreach (Call* call, byDate[cat]) {
               calls << call;
            }
         }
         break;
      default:
         break;
   }
}

CALLMODEL_TEMPLATE void SORTABLE_T::setContactCategory(QList<Contact*> contacts,ContactSortingMode mode)
{
   QHash<Contact*, QDateTime> recentlyUsed;
   switch (mode) {
      case ContactSortingMode::Recently_used:
         recentlyUsed = getContactListByTime();
         foreach (QString cat, m_slHistoryConst) {
            //m_pContactView->addCategory(cat);
         }
         break;
      default:
         break;
   }
   foreach (Contact* cont, contacts) {
      if (cont->getPhoneNumbers().count() && usableNumberCount(cont)) {
         QString category;
         switch (mode) {
            case ContactSortingMode::Name:
               category = QString(cont->getFormattedName()[0]);
               break;
            case ContactSortingMode::Organisation:
               category = (cont->getOrganization().isEmpty())?"Unknow":cont->getOrganization();
               break;
            case ContactSortingMode::Recently_used:
               if (recentlyUsed.find(cont) != recentlyUsed.end())
                  category = timeToHistoryCategory(recentlyUsed[cont].date());
               else
                  category = m_slHistoryConst[Never];
               break;
            case ContactSortingMode::Group:
               category = "TODO";
               break;
            case ContactSortingMode::Department:
               category = (cont->getDepartment().isEmpty())?"Unknow":cont->getDepartment();;
               break;
            default:
               break;
         }
      }
   }
}

///Return the identity of the call caller, try to return something usefull
CALLMODEL_TEMPLATE QString SORTABLE_T::getIdentity(Call* item)
{
   Contact* contact = AkonadiBackend::getInstance()->getContactByPhone(item->getPeerPhoneNumber());
   if (contact)
      return contact->getFormattedName();
   else if (!item->getPeerName().isEmpty())
      return item->getPeerName();
   else
      return item->getPeerPhoneNumber();
}

CALLMODEL_TEMPLATE int SORTABLE_T::usableNumberCount(Contact* cont)
{
   uint result =0;
   QStringList list = ConfigurationSkeleton::phoneTypeList();
   foreach (Contact::PhoneNumber* pn,cont->getPhoneNumbers()) {
      result += list.indexOf(pn->getType()) != -1;
   }
   return result;
}
