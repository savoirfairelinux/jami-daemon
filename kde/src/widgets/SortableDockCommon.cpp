#include "SortableDockCommon.h"

//Qt
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

//SFLPhone
#include "lib/Call.h"
#include "lib/Contact.h"
#include "lib/CallModel.h"
#include "SFLPhone.h"
#include "AkonadiBackend.h"

///StaticEventHandler constructor
StaticEventHandler::StaticEventHandler(QObject* parent) : QObject(parent)
{
   QTimer* timer = new QTimer(this);
   connect(timer, SIGNAL(timeout()), this, SLOT(update()));
   timer->start(86400000); //1 day
   update();
}

///Update the days constant, necessary to cycle after midnight
void StaticEventHandler::update()
{
   SortableDockCommon::m_slHistoryConst = {
      "Today"                                          ,//0
      "Yesterday"                                      ,//1
      QDate::currentDate().addDays(-2).toString("dddd"),//2
      QDate::currentDate().addDays(-3).toString("dddd"),//3
      QDate::currentDate().addDays(-4).toString("dddd"),//4
      QDate::currentDate().addDays(-5).toString("dddd"),//5
      QDate::currentDate().addDays(-6).toString("dddd"),//6
      "Last week"                                      ,//7
      "Two weeks ago"                                  ,//8
      "Three weeks ago"                                ,//9
      "Last month"                                     ,//10
      "Two months ago"                                 ,//11
      "Three months ago"                               ,//12
      "Four months ago"                                ,//13
      "Five months ago"                                ,//14
      "Six months ago"                                 ,//15
      "Seven months ago"                               ,//16
      "Eight months ago"                               ,//17
      "Nine months ago"                                ,//18
      "Ten months ago"                                 ,//19
      "Eleven months ago"                              ,//20
      "Twelve months ago"                              ,//21
      "Last year"                                      ,//22
      "Very long time ago"                             ,//23
      "Never"                                           //24
   };
}

QStringList         SortableDockCommon::m_slHistoryConst;
StaticEventHandler* SortableDockCommon::m_spEvHandler = new StaticEventHandler(0);

/*****************************************************************************
 *                                                                           *
 *                                  Helpers                                  *
 *                                                                           *
 ****************************************************************************/

QString SortableDockCommon::timeToHistoryCategory(QDate date)
{
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
QHash<Contact*, QDateTime> SortableDockCommon::getContactListByTime(/*ContactList list*/)
{
   const CallMap& history= SFLPhone::model()->getHistory();
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