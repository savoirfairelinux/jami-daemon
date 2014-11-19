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
#ifndef KEYEXCHANGEMODEL_H
#define KEYEXCHANGEMODEL_H

#include "typedefs.h"
#include <QtCore/QAbstractListModel>

class Account;

///Static model for handling encryption types
class LIB_EXPORT KeyExchangeModel : public QAbstractListModel {
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
   friend class Account;

public:
   ///@enum Type Every supported encryption types
   enum class Type {
      ZRTP = 0,
      SDES = 1,
      NONE = 2,
      __COUNT,
   };

   ///@enum Options Every Key exchange options
   enum class Options {
      RTP_FALLBACK     = 0,
      DISPLAY_SAS      = 1,
      NOT_SUPP_WARNING = 2,
      HELLO_HASH       = 3,
      DISPLAY_SAS_ONCE = 4,
      __COUNT,
   };

   class Name {
   public:
      constexpr static const char* NONE = "None";
      constexpr static const char* ZRTP = "ZRTP";
      constexpr static const char* SDES = "SDES";
   };

   class DaemonName {
   public:
      constexpr static const char* NONE = ""    ;
      constexpr static const char* ZRTP = "zrtp";
      constexpr static const char* SDES = "sdes";
   };

   //Private constructor, can only be called by 'Account'
   explicit KeyExchangeModel(Account* account);

   //Model functions
   QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole     ) const;
   int           rowCount ( const QModelIndex& parent = QModelIndex()                ) const;
   Qt::ItemFlags flags    ( const QModelIndex& index                                 ) const;
   virtual bool  setData  ( const QModelIndex& index, const QVariant &value, int role)      ;

   //Getters
   QModelIndex                   toIndex       (KeyExchangeModel::Type type) const;
   static const char*            toDaemonName  (KeyExchangeModel::Type type)      ;
   static KeyExchangeModel::Type fromDaemonName(const QString& name        )      ;

   bool isRtpFallbackEnabled() const;
   bool isDisplaySASEnabled () const;
   bool isDisplaySasOnce    () const;
   bool areWarningSupressed () const;
   bool isHelloHashEnabled  () const;


private:
   Account* m_pAccount;
   static const TypedStateMachine< TypedStateMachine< bool , KeyExchangeModel::Type > , KeyExchangeModel::Options > availableOptions;

public Q_SLOTS:
   void enableSRTP(bool enable);

Q_SIGNALS:
   void srtpEnabled(bool);
};
Q_DECLARE_METATYPE(KeyExchangeModel*)
#endif
