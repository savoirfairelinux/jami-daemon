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
#include "lastusednumbermodel.h"
#include "call.h"
#include "uri.h"
#include "phonenumber.h"

LastUsedNumberModel* LastUsedNumberModel::m_spInstance = nullptr;

struct ChainedPhoneNumber {
   ChainedPhoneNumber(PhoneNumber* n) : m_pPrevious(nullptr),m_pNext(nullptr),m_pSelf(n){}
   ChainedPhoneNumber* m_pPrevious;
   ChainedPhoneNumber* m_pNext;
   PhoneNumber*  m_pSelf;
};

LastUsedNumberModel::LastUsedNumberModel() : QAbstractListModel(),m_pFirstNode(nullptr),m_IsValid(false)
{
   for (int i=0;i<MAX_ITEM;i++) m_lLastNumbers[i] = nullptr;
}

LastUsedNumberModel* LastUsedNumberModel::instance()
{
   if (!m_spInstance) {
      m_spInstance = new LastUsedNumberModel();
   }
   return m_spInstance;
}

///Push 'call' phoneNumber on the top of the stack
void LastUsedNumberModel::addCall(Call* call)
{
   PhoneNumber* number = call->peerPhoneNumber();
   ChainedPhoneNumber* node = m_hNumbers[number];
   if (!number || ( node && m_pFirstNode == node) ) {

      //TODO enable threaded numbers now
      return;
   }

   if (!node) {
      node = new ChainedPhoneNumber(number);
      m_hNumbers[number] = node;
   }
   else {
      if (node->m_pPrevious)
         node->m_pPrevious->m_pNext = node->m_pNext;
      if (node->m_pNext)
         node->m_pNext->m_pPrevious = node->m_pPrevious;
   }
   if (m_pFirstNode) {
      m_pFirstNode->m_pPrevious = node;
      node->m_pNext = m_pFirstNode;
   }
   m_pFirstNode = node;
   m_IsValid = false;
   emit layoutChanged();
}


QVariant LastUsedNumberModel::data( const QModelIndex& index, int role) const
{
   if (!index.isValid())
      return QVariant();
   if (!m_IsValid) {
      ChainedPhoneNumber* current = m_pFirstNode;
      for (int i=0;i<MAX_ITEM;i++) { //Can only grow, no need to clear
         const_cast<LastUsedNumberModel*>(this)->m_lLastNumbers[i] = current;
         current = current->m_pNext;
         if (!current)
            break;
      }
      const_cast<LastUsedNumberModel*>(this)->m_IsValid = true;
   }
   switch (role) {
      case Qt::DisplayRole: {
         return m_lLastNumbers[index.row()]->m_pSelf->uri();
      }
   };
   return QVariant();
}

int LastUsedNumberModel::rowCount( const QModelIndex& parent) const
{
   if (parent.isValid())
      return 0;
   return m_hNumbers.size() < LastUsedNumberModel::MAX_ITEM?m_hNumbers.size():LastUsedNumberModel::MAX_ITEM;
}

Qt::ItemFlags LastUsedNumberModel::flags( const QModelIndex& index) const
{
   Q_UNUSED(index)
   return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

bool LastUsedNumberModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role)
   return false;
}
