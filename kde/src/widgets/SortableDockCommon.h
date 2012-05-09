#ifndef SORTABLE_DOCK_COMMON
#define SORTABLE_DOCK_COMMON

#include <QObject>
#include <QHash>

//Qt
class QString;
class QStringList;
class QDate;
class QDateTime;

//SFLPhone
class StaticEventHandler;
class Contact;

class SortableDockCommon {
   public:
      friend class StaticEventHandler;
      
   protected:
      SortableDockCommon(){};
      //Helpers
      static QString timeToHistoryCategory(QDate date);
      static QHash<Contact*, QDateTime> getContactListByTime(/*ContactList list*/);

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


///@class StaticEventHandler "cron jobs" for static member;
class StaticEventHandler : public QObject
{
   Q_OBJECT
   public:
      StaticEventHandler(QObject* parent);

   private slots:
      void update();
};

#endif