/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                          *
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
#ifndef RINGTONEMODEL_H
#define RINGTONEMODEL_H
#include "typedefs.h"

//Qt
#include <QtCore/QString>
#include <QtCore/QAbstractTableModel>
class QTimer;

//SFLPhone
class Account;

///CredentialModel: A model for account credentials
class LIB_EXPORT RingToneModel : public QAbstractTableModel {
   Q_OBJECT
public:

   enum Role {
      IsPlaying = 100,
      FullPath  = 101,
   };

   explicit RingToneModel(Account* a);
   virtual ~RingToneModel();

   //Model functions
   QVariant      data        ( const QModelIndex& index, int role = Qt::DisplayRole     ) const;
   int           rowCount    ( const QModelIndex& parent = QModelIndex()                ) const;
   int           columnCount ( const QModelIndex& parent = QModelIndex()                ) const;
   Qt::ItemFlags flags       ( const QModelIndex& index                                 ) const;
   virtual bool  setData     ( const QModelIndex& index, const QVariant &value, int role)      ;

   //Getters
   QString     currentRingTone() const;
   QModelIndex currentIndex   () const;

   //Setter
   void setCurrentIndex(const QModelIndex& idx);

   //Mutator
   void play(const QModelIndex& index);

private:
   struct RingToneInfo {
      explicit RingToneInfo() : isPlaying(false),isCurrent(false){}
      QString name;
      QString path;
      bool isPlaying;
      bool isCurrent;
   };
   QList<RingToneInfo*> m_lRingTone;
   Account* m_pAccount;
   QTimer* m_pTimer;
   RingToneInfo* m_pCurrent;

private Q_SLOTS:
   void slotStopTimer();
};

#endif //RINGTONEMODEL_H
