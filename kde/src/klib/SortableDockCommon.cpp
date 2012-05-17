#include "SortableDockCommon.h"

//Qt
#include <QtCore/QDateTime>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

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
