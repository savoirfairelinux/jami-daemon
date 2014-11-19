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
#include "numbercompletionmodel.h"

//Qt
#include <QtCore/QCoreApplication>

//System
#include <cmath>

//SFLPhone
#include "phonedirectorymodel.h"
#include "phonenumber.h"
#include "call.h"
#include "uri.h"
#include "numbercategory.h"
#include "accountlistmodel.h"
#include "numbercategorymodel.h"
#include "visitors/pixmapmanipulationvisitor.h"

NumberCompletionModel::NumberCompletionModel() : QAbstractTableModel(QCoreApplication::instance()),
   m_pCall(nullptr),m_Enabled(false),m_UseUnregisteredAccount(true)
{
   setObjectName("NumberCompletionModel");
}

NumberCompletionModel::~NumberCompletionModel()
{

}

QVariant NumberCompletionModel::data(const QModelIndex& index, int role ) const
{
   if (!index.isValid()) return QVariant();
   const QMap<int,PhoneNumber*>::iterator i = const_cast<NumberCompletionModel*>(this)->m_hNumbers.end()-1-index.row();
   const PhoneNumber* n = i.value();
   const int weight     = i.key  ();

   bool needAcc = (role>=100 || role == Qt::UserRole) && n->account() && n->account() != AccountListModel::instance()->currentAccount()
                  && n->account()->alias() != Account::ProtocolName::IP2IP;

   switch (static_cast<NumberCompletionModel::Columns>(index.column())) {
      case NumberCompletionModel::Columns::CONTENT:
         switch (role) {
            case Qt::DisplayRole:
               return n->uri();
               break;
            case Qt::ToolTipRole:
               return QString("<table><tr><td>%1</td></tr><tr><td>%2</td></tr></table>").arg(n->primaryName()).arg(n->category()->name());
               break;
            case Qt::DecorationRole:
               return n->icon();
               break;
            case NumberCompletionModel::Role::ALTERNATE_ACCOUNT:
            case Qt::UserRole:
               if (needAcc)
                  return n->account()->alias();
               else
                  return QString();
            case NumberCompletionModel::Role::FORCE_ACCOUNT:
               return needAcc;
            case NumberCompletionModel::Role::ACCOUNT:
               if (needAcc)
                  return QVariant::fromValue(const_cast<Account*>(n->account()));
               break;
         };
         break;
      case NumberCompletionModel::Columns::NAME:
         switch (role) {
            case Qt::DisplayRole:
               return n->primaryName();
         };
         break;
      case NumberCompletionModel::Columns::ACCOUNT:
         switch (role) {
            case Qt::DisplayRole:
               return n->account()?n->account()->id():AccountListModel::instance()->currentAccount()->id();
         };
         break;
      case NumberCompletionModel::Columns::WEIGHT:
         switch (role) {
            case Qt::DisplayRole:
               return weight;
         };
         break;
   };
   return QVariant();
}

int NumberCompletionModel::rowCount(const QModelIndex& parent ) const
{
   if (parent.isValid())
      return 0;
   return m_hNumbers.size();
}

int NumberCompletionModel::columnCount(const QModelIndex& parent ) const
{
   if (parent.isValid())
      return 0;
   return 4;
}

Qt::ItemFlags NumberCompletionModel::flags(const QModelIndex& index ) const
{
   if (!index.isValid()) return Qt::NoItemFlags;
   return Qt::ItemIsEnabled|Qt::ItemIsSelectable;
}

QVariant NumberCompletionModel::headerData (int section, Qt::Orientation orientation, int role) const
{
   Q_UNUSED(section)
   Q_UNUSED(orientation)
   static const QString headers[] = {tr("URI"), tr("Name"), tr("Account"), tr("Weight")};
   if (role == Qt::DisplayRole) return headers[section];
   return QVariant();
}

bool NumberCompletionModel::setData(const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED( index )
   Q_UNUSED( value )
   Q_UNUSED( role  )
   return false;
}

//Set the current call
void NumberCompletionModel::setCall(Call* call)
{
   if (m_pCall)
      disconnect(m_pCall,SIGNAL(dialNumberChanged(QString)),this,SLOT(setPrefix(QString)));
   m_pCall = call;
   if (m_pCall)
      connect(m_pCall,SIGNAL(dialNumberChanged(QString)),this,SLOT(setPrefix(QString)));
   setPrefix(call?call->dialNumber():QString());
}

void NumberCompletionModel::setPrefix(const QString& str)
{
   m_Prefix = str;
   const bool e = ((m_pCall && m_pCall->state() == Call::State::DIALING) || (!m_pCall)) && (!str.isEmpty());
   if (m_Enabled != e) {
      m_Enabled = e;
      emit enabled(e);
   }
   if (m_Enabled)
      updateModel();
   else {
      m_hNumbers.clear();
      emit layoutChanged();
   }
}

Call* NumberCompletionModel::call() const
{
   return m_pCall;
}

PhoneNumber* NumberCompletionModel::number(const QModelIndex& idx) const
{
   if (idx.isValid()) {
      return (const_cast<NumberCompletionModel*>(this)->m_hNumbers.end()-1-idx.row()).value();
   }
   return nullptr;
}

void NumberCompletionModel::updateModel()
{
   QSet<PhoneNumber*> numbers;
   beginResetModel();
   m_hNumbers.clear();
   if (!m_Prefix.isEmpty()) {
      locateNameRange  ( m_Prefix, numbers );
      locateNumberRange( m_Prefix, numbers );

      foreach(PhoneNumber* n,numbers) {
         if (m_UseUnregisteredAccount || ((n->account() && n->account()->isRegistered()) || !n->account()))
            m_hNumbers.insert(getWeight(n),n);
      }
   }
   endResetModel();
   emit layoutChanged();
}

void NumberCompletionModel::getRange(QMap<QString,PhoneDirectoryModel::NumberWrapper*> map, const QString& prefix, QSet<PhoneNumber*>& set) const
{
   if (prefix.isEmpty())
      return;
   QMap<QString,PhoneDirectoryModel::NumberWrapper*>::iterator iBeg = map.begin();
   QMap<QString,PhoneDirectoryModel::NumberWrapper*>::iterator iEnd = map.end  ()-1;

   const QString pref = prefix.toLower();

   const int prefixLen = pref.size();
   int size = map.size()/2;
   bool startOk(false),endOk(false);
   while (size > 1 && !(startOk&&endOk)) {
      QMap<QString,PhoneDirectoryModel::NumberWrapper*>::iterator mid;
      if (size > 7)
         mid = (iBeg+size);
      else {
         //We have to be careful with "::ceil" it may cause an overflow in some rare case
         int toAdd = size-1;
         mid = iBeg;
         while (toAdd && mid != map.end()) {
            ++mid;
            --toAdd;
         }
      }
      if (mid != map.end() && mid.key().left(prefixLen) == pref && iBeg.key().left(prefixLen) < pref) {
         //Too far, need to go back
         iBeg = mid;
         while ((iBeg-1).key().left(prefixLen) == pref && iBeg != map.begin())
            iBeg--;
         startOk = true;
      }
      else if ((!startOk) && mid != map.end() && mid.key().left(prefixLen) < pref) {
         iBeg = mid;
      }
      else if(!endOk) {
         iEnd = mid;
      }

      while ((iEnd).key().left(prefixLen) == pref && iEnd+1 != map.end()) {
         ++iEnd;
      }

      endOk = (iEnd.key().left(prefixLen) == pref);

      size = ::ceil(size/2.0f);
   }

   while (iBeg.key().left(prefixLen) != pref && iBeg != iEnd)
      ++iBeg;

   if (iEnd == iBeg && iBeg.key().left(prefixLen) != pref) {
      iEnd = map.end();
      iBeg = map.end();
   }
   while(iBeg != iEnd) {
      foreach(PhoneNumber* n,iBeg.value()->numbers) {
         if (n) {
            set << n;
         }
      }
      ++iBeg;
   }
}

void NumberCompletionModel::locateNameRange(const QString& prefix, QSet<PhoneNumber*>& set)
{
   getRange(PhoneDirectoryModel::instance()->m_lSortedNames,prefix,set);
}

void NumberCompletionModel::locateNumberRange(const QString& prefix, QSet<PhoneNumber*>& set)
{
   getRange(PhoneDirectoryModel::instance()->m_hSortedNumbers,prefix,set);
}

uint NumberCompletionModel::getWeight(PhoneNumber* number)
{
   Q_UNUSED(number)
   uint weight = 1;
   weight += (number->weekCount()+1)*150;
   weight += (number->trimCount()+1)*75 ;
   weight += (number->callCount()+1)*35 ;
   weight *= (number->uri().indexOf(m_Prefix)!= -1?3:1);
   weight *= (number->isPresent()?2:1);
   return weight;
}

QString NumberCompletionModel::prefix() const
{
   return m_Prefix;
}

void NumberCompletionModel::setUseUnregisteredAccounts(bool value)
{
   m_UseUnregisteredAccount = value;
}

bool NumberCompletionModel::isUsingUnregisteredAccounts()
{
   return m_UseUnregisteredAccount;
}
