/****************************************************************************
 *   Copyright (C) 2012-2014 by Savoir-Faire Linux                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#include "historytimecategorymodel.h"

#include <QtCore/QDate>

QVector<QString> HistoryTimeCategoryModel::m_lCategories;
HistoryTimeCategoryModel* HistoryTimeCategoryModel::m_spInstance = new HistoryTimeCategoryModel();

HistoryTimeCategoryModel::HistoryTimeCategoryModel(QObject* parent) : QAbstractListModel(parent)
{
   m_lCategories << tr("Today")                                 ;//0
   m_lCategories << tr("Yesterday")                             ;//1
   m_lCategories << QDate::currentDate().addDays(-2).toString("dddd");//2
   m_lCategories << QDate::currentDate().addDays(-3).toString("dddd");//3
   m_lCategories << QDate::currentDate().addDays(-4).toString("dddd");//4
   m_lCategories << QDate::currentDate().addDays(-5).toString("dddd");//5
   m_lCategories << QDate::currentDate().addDays(-6).toString("dddd");//6
   m_lCategories << tr("Last week")                             ;//7
   m_lCategories << tr("Two weeks ago")                         ;//8
   m_lCategories << tr("Three weeks ago")                       ;//9
   m_lCategories << tr("Last month")                            ;//10
   m_lCategories << tr("Two months ago")                        ;//11
   m_lCategories << tr("Three months ago")                      ;//12
   m_lCategories << tr("Four months ago")                       ;//13
   m_lCategories << tr("Five months ago")                       ;//14
   m_lCategories << tr("Six months ago")                        ;//15
   m_lCategories << tr("Seven months ago")                      ;//16
   m_lCategories << tr("Eight months ago")                      ;//17
   m_lCategories << tr("Nine months ago")                       ;//18
   m_lCategories << tr("Ten months ago")                        ;//19
   m_lCategories << tr("Eleven months ago")                     ;//20
   m_lCategories << tr("Twelve months ago")                     ;//21
   m_lCategories << tr("Last year")                             ;//22
   m_lCategories << tr("Very long time ago")                    ;//23
   m_lCategories << tr("Never")                                 ;//24
}

//Abstract model member
QVariant HistoryTimeCategoryModel::data(const QModelIndex& index, int role ) const
{
   if (!index.isValid()) return QVariant();
   switch (role) {
      case Qt::DisplayRole:
         return m_lCategories[index.row()];
   }
   return QVariant();
}

int HistoryTimeCategoryModel::rowCount(const QModelIndex& parent ) const
{
   if (parent.isValid()) return 0;
   return m_lCategories.size();
}

Qt::ItemFlags HistoryTimeCategoryModel::flags(const QModelIndex& index ) const
{
   Q_UNUSED(index)
   return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool HistoryTimeCategoryModel::setData(const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role)
   return false;
}


QString HistoryTimeCategoryModel::timeToHistoryCategory(const time_t time)
{
   int period = (int)HistoryTimeCategoryModel::timeToHistoryConst(time);
   if (period >= 0 && period <= 24)
      return m_lCategories[period];
   else
      return m_lCategories[24];
}

HistoryTimeCategoryModel::HistoryConst HistoryTimeCategoryModel::timeToHistoryConst(const time_t time)
{
   time_t time2 = time;
   time_t currentTime;
   ::time(&currentTime);
   if (!time || time < 0)
      return HistoryTimeCategoryModel::HistoryConst::Never;

   //Check if part if the current Nychthemeron
   if (currentTime - time <= 3600*24) //The future case would be a bug, but it have to be handled anyway or it will appear in "very long time ago"
      return HistoryConst::Today;

   time2 -= time%(3600*24); //Reset to midnight
   currentTime -= currentTime%(3600*24); //Reset to midnight
   //Check for last week
   if (currentTime-(6)*3600*24 < time2) {
      for (int i=1;i<7;i++) {
         if (currentTime-((i)*3600*24) == time2)
            return (HistoryTimeCategoryModel::HistoryConst)(i); //Yesterday to Six_days_ago
      }
   }
   //Check for last month
   else if (currentTime - ((4)*7*24*3600) < time2) {
      for (int i=1;i<4;i++) {
         if (currentTime - ((i+1)*7*24*3600) < time2)
            return (HistoryTimeCategoryModel::HistoryConst)(i+((int)HistoryTimeCategoryModel::HistoryConst::Last_week)-1); //Last_week to Three_weeks_ago
      }
   }
   //Check for last year
   else if (currentTime-(12)*30.4f*24*3600 < time2) {
      for (int i=1;i<12;i++) {
         if (currentTime-(i+1)*30.4f*24*3600 < time2) //Not exact, but faster
            return (HistoryTimeCategoryModel::HistoryConst)(i+((int)HistoryTimeCategoryModel::HistoryConst::Last_month)-1); //Last_month to Twelve_months ago
      }
   }
   //if (QDate::currentDate().addYears(-1)  >= date && QDate::currentDate().addYears(-2)  < date)
   else if (currentTime-365*24*3600 < time2)
      return HistoryConst::Last_year;

   //Every other senario
   return HistoryTimeCategoryModel::HistoryConst::Very_long_time_ago;
}

QString HistoryTimeCategoryModel::indexToName(int idx)
{
   if (idx > 24) return m_lCategories[24];
   return m_lCategories[idx];
}
