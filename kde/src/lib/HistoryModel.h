/************************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                                       *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
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
#ifndef HISTORY_MODEL_H
#define HISTORY_MODEL_H
//Base
#include "typedefs.h"
#include <QtCore/QObject>

//Qt
class QSharedMemory;
class QTimer;

//SFLPhone
class Call;

//Typedef
typedef QMap<QString, Call*>  CallMap;
typedef QList<Call*>          CallList;

///@class HistoryModel History call manager
class LIB_EXPORT HistoryModel : public QObject {
   Q_OBJECT
public:
   //Singleton
   static HistoryModel* self();
   ~HistoryModel();

   //Getters
   static const CallMap&    getHistory             ();
      static const QStringList getHistoryCallId       ();
      static const QStringList getNumbersByPopularity ();
   
   //Setters
   static void add(Call* call);

private:
   
   //Constructor
   HistoryModel();
   /*static*/  bool initHistory ();

   //Mutator
   void addPriv(Call* call);

   //Static attributes
   static HistoryModel* m_spInstance;

   //Attributes
   static CallMap m_sHistoryCalls;
   bool m_HistoryInit;
   
public slots:
   

private slots:
   
signals:
   void historyChanged          (            );
   void newHistoryCall          ( Call* call );
};

#endif