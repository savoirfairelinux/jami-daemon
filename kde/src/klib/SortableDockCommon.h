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

#ifndef SORTABLE_DOCK_COMMON
#define SORTABLE_DOCK_COMMON

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QModelIndex>
#include <QtGui/QWidget>

#include "HelperFunctions.h"

//Qt
class QString;
class QStringList;
class QDate;
class QDateTime;

//SFLPhone
class StaticEventHandler;
class Contact;
class Call;

///@enum ContactSortingMode Available sorting mode for the contact dock
enum ContactSortingMode {
   Name              ,
   Organisation      ,
   Recently_used     ,
   Group             ,
   Department        ,
};

///@enum HistorySortingMode Mode used to sort the history dock
enum HistorySortingMode {
   Date       = 0,
   Name2      = 1,
   Popularity = 2,
   Length     = 3,
};

///SortableDockCommon: Common code for filtering
template  <typename CallWidget = QWidget*, typename Index = QModelIndex*>
class LIB_EXPORT SortableDockCommon {
   public:
      friend class StaticEventHandler;
      
      //Helpers
      static QString getIdentity(Call* item);
      static int usableNumberCount(Contact* cont);
      static void setHistoryCategory ( QList<Call*>& calls       , HistorySortingMode mode );
      static void setContactCategory ( QList<Contact*> contacts , ContactSortingMode mode );
      
   protected:
      SortableDockCommon();
      //Helpers
      static QString                    timeToHistoryCategory ( QDate date );
      static QHash<Contact*, QDateTime> getContactListByTime  (            );

      //Attributes
      static QStringList         m_slHistoryConst;
      
      ///@enum HistoryConst match m_slHistoryConst
      enum HistoryConst {
         Today             = 0  ,
         Yesterday         = 1  ,
         Two_days_ago      = 2  ,
         Three_days_ago    = 3  ,
         Four_days_ago     = 4  ,
         Five_days_ago     = 5  ,
         Six_days_ago      = 6  ,
         Last_week         = 7  ,
         Two_weeks_ago     = 8  ,
         Three_weeks_ago   = 9  ,
         Last_month        = 10 ,
         Two_months_ago    = 11 ,
         Three_months_ago  = 12 ,
         Four_months_ago   = 13 ,
         Five_months_ago   = 14 ,
         Six_months_ago    = 15 ,
         Seven_months_ago  = 16 ,
         Eight_months_ago  = 17 ,
         Nine_months_ago   = 18 ,
         Ten_months_ago    = 19 ,
         Eleven_months_ago = 20 ,
         Twelve_months_ago = 21 ,
         Last_year         = 22 ,
         Very_long_time_ago= 23 ,
         Never             = 24
      };

   private:
      static StaticEventHandler* m_spEvHandler   ;
};


///StaticEventHandler: "cron jobs" for static member;
class LIB_EXPORT StaticEventHandler : public QObject
{
   Q_OBJECT
   public:
      StaticEventHandler(QObject* parent, QStringList* list);

   public slots:
      void update();
   private:
      QStringList* m_pList;
};

#include "SortableDockCommon.hpp"

#endif
