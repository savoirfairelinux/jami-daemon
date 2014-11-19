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
#include "presencestatusmodel.h"

//Qt
#include <QtCore/QCoreApplication>

//SFLPhone
#include "accountlistmodel.h"
#include "dbus/presencemanager.h"
#include "visitors/presenceserializationvisitor.h"

//Static
PresenceStatusModel* PresenceStatusModel::m_spInstance = nullptr;

///Constructor
PresenceStatusModel::PresenceStatusModel(QObject* parent) : QAbstractTableModel(parent?parent:QCoreApplication::instance()),
m_pCurrentStatus(nullptr),m_pDefaultStatus(nullptr),m_UseCustomStatus(false),m_CustomStatus(false),m_pVisitor(nullptr)
{
   setObjectName("PresenceStatusModel");
}

PresenceStatusModel::~PresenceStatusModel()
{
   foreach (StatusData* data, m_lStatuses) {
      delete data;
   }
   if (m_pVisitor) delete m_pVisitor;
}

///Get model data
QVariant PresenceStatusModel::data(const QModelIndex& index, int role ) const
{
   if (index.isValid()) {
      switch (static_cast<PresenceStatusModel::Columns>(index.column())) {
         case PresenceStatusModel::Columns::Name:
            switch (role) {
               case Qt::DisplayRole:
               case Qt::EditRole:
                  return m_lStatuses[index.row()]->name;
               case Qt::ToolTipRole:
                  return m_lStatuses[index.row()]->message;
            }
            break;
         case PresenceStatusModel::Columns::Message:
            switch (role) {
               case Qt::DisplayRole:
               case Qt::EditRole:
                  return m_lStatuses[index.row()]->message;
            }
            break;
         case PresenceStatusModel::Columns::Color:
            switch (role) {
               case Qt::BackgroundColorRole:
                  return m_lStatuses[index.row()]->color;
            }
            break;
         case PresenceStatusModel::Columns::Status:
            switch (role) {
               case Qt::CheckStateRole:
                  return m_lStatuses[index.row()]->status?Qt::Checked:Qt::Unchecked;
               case Qt::TextAlignmentRole:
                  return Qt::AlignCenter;
            }
            break;
         case PresenceStatusModel::Columns::Default:
            switch (role) {
               case Qt::CheckStateRole:
                  return m_lStatuses[index.row()]->defaultStatus?Qt::Checked:Qt::Unchecked;
               case Qt::TextAlignmentRole:
                  return Qt::AlignCenter;
            }
            break;
      }
   }
   return QVariant();
}

///Return the number of pre defined status
int PresenceStatusModel::rowCount(const QModelIndex& parent ) const
{
   if (parent.isValid()) return 0;
   return m_lStatuses.size();
}

///Return the number of column (static: {"Name","Message","Color","Present","Default"})
int PresenceStatusModel::columnCount(const QModelIndex& parent ) const
{
   if (parent.isValid()) return 0;
   return 5;
}

///All the items are enabled, selectable and editable
Qt::ItemFlags PresenceStatusModel::flags(const QModelIndex& index ) const
{
   const int col = index.column();
   return Qt::ItemIsEnabled
      | Qt::ItemIsSelectable
      | (col<2||col>=3?Qt::ItemIsEditable:Qt::NoItemFlags)
      | (col>=3?Qt::ItemIsUserCheckable:Qt::NoItemFlags);
}

///Set model data
bool PresenceStatusModel::setData(const QModelIndex& index, const QVariant &value, int role )
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role)
   if (index.isValid()) {
      StatusData* dat = m_lStatuses[index.row()];
      switch(static_cast<PresenceStatusModel::Columns>(index.column())) {
         case PresenceStatusModel::Columns::Name:
            if (role == Qt::EditRole) {
               dat->name = value.toString();
               emit dataChanged(index,index);
               return true;
            }
            break;
         case PresenceStatusModel::Columns::Message:
            if (role == Qt::EditRole) {
               dat->message = value.toString();
               emit dataChanged(index,index);
               return true;
            }
            break;
         case PresenceStatusModel::Columns::Color:
            if (role == Qt::EditRole) {

            }
            break;
         case PresenceStatusModel::Columns::Status:
            if (role == Qt::CheckStateRole) {
               dat->status = value.toBool();
               emit dataChanged(index,index);
               return true;
            }
            break;
         case PresenceStatusModel::Columns::Default:
            if (role == Qt::CheckStateRole) {
               dat->defaultStatus = value.toBool();
               setDefaultStatus(index);
               return true;
            }
            break;
      };
   }
   return false;
}

///Return header data
QVariant PresenceStatusModel::headerData(int section, Qt::Orientation orientation, int role ) const
{
   Q_UNUSED(section)
   Q_UNUSED(orientation)
   static const QString rows[] = {tr("Name"),tr("Message"),tr("Color"),tr("Present"),tr("Default")};
   if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
      return QVariant(rows[section]);
   }
   return QVariant();
}

///Add a status to the model
void PresenceStatusModel::addStatus(StatusData* status)
{
   m_lStatuses << status;
   if (status->defaultStatus) {
      m_pDefaultStatus = status;
      if (!m_pCurrentStatus)
         setCurrentIndex(index(m_lStatuses.size()-1,0));
   }
}


void PresenceStatusModel::setPresenceVisitor(PresenceSerializationVisitor* visitor)
{
   m_pVisitor = visitor;
   if (m_pVisitor)
      m_pVisitor->load();
}

///Add a new status
void PresenceStatusModel::addRow()
{
   StatusData* newRow = new StatusData();
   newRow->status = false;
   m_lStatuses << newRow;
   emit layoutChanged();
}

///Remove status[index]
void PresenceStatusModel::removeRow(const QModelIndex& index)
{
   StatusData* toDel = m_lStatuses[index.row()];
   m_lStatuses.remove(index.row());
   emit layoutChanged();
   delete toDel;
}

///Serialize model TODO a backend visitor need to be created
void PresenceStatusModel::save()
{
   if (m_pVisitor)
      m_pVisitor->serialize();
}

///Singleton
PresenceStatusModel* PresenceStatusModel::instance()
{
   if (!m_spInstance) {
      m_spInstance = new PresenceStatusModel();
   }
   return m_spInstance;
}

///Move idx up
void PresenceStatusModel::moveUp(const QModelIndex& idx)
{
   const int row = idx.row();
   if (row > 0) {
      StatusData* tmp      = m_lStatuses[row-1];
      m_lStatuses[ row-1 ] = m_lStatuses[row  ];
      m_lStatuses[ row]    = tmp;
      emit dataChanged(index(row-1,0),index(row,0));
   }
}

///Move idx down
void PresenceStatusModel::moveDown(const QModelIndex& idx)
{
   const int row = idx.row();
   if (row-1 < m_lStatuses.size()) {
      StatusData* tmp      = m_lStatuses[row+1];
      m_lStatuses[ row+1 ] = m_lStatuses[row  ];
      m_lStatuses[ row   ] = tmp;
      emit dataChanged(index(row,0),index(row+1,0));
   }
}

///Return the (user defined) custom message;
QString PresenceStatusModel::customMessage() const
{
   return m_CustomMessage;
}

///Set the (user defined) custom message
void PresenceStatusModel::setCustomMessage(const QString& message)
{
   const bool hasChanged = m_CustomMessage != message;
   m_CustomMessage = message;
   if (hasChanged) {
      emit customMessageChanged(message);
      if (m_UseCustomStatus)
         emit currentMessageChanged(message);
   }
}

///Set the custom status
void PresenceStatusModel::setCustomStatus(bool status)
{
   const bool hasChanged = status != m_CustomStatus;
   m_CustomStatus = status;
   if (hasChanged) {
      emit customStatusChanged(status);
      if (m_UseCustomStatus)
         emit currentStatusChanged(status);
   }
}

///Switch between the pre-defined status list and custom ones
void PresenceStatusModel::setUseCustomStatus(bool useCustom)
{
   const bool changed = m_UseCustomStatus != useCustom;
   m_UseCustomStatus = useCustom;
   if (changed) {
      emit useCustomStatusChanged( useCustom                                                                                );
      emit currentIndexChanged   ( useCustom||!m_pCurrentStatus?index(-1,-1):index(m_lStatuses.indexOf(m_pCurrentStatus),0) );
      emit currentNameChanged    ( useCustom?tr("Custom"):(m_pCurrentStatus?m_pCurrentStatus->name:tr("N/A"))               );
      emit currentStatusChanged  ( useCustom?m_CustomStatus:(m_pCurrentStatus?m_pCurrentStatus->status:false)               );
      emit currentMessageChanged ( useCustom?m_CustomMessage:(m_pCurrentStatus?m_pCurrentStatus->message:tr("N/A"))         );
   }
}

///Return if the presence status is from the predefined list or custom
bool PresenceStatusModel::useCustomStatus() const
{
   return m_UseCustomStatus;
}

///Return the custom status
bool PresenceStatusModel::customStatus() const
{
   return m_CustomStatus;
}

///Set the current status and publish it on the network
void PresenceStatusModel::setCurrentIndex  (const QModelIndex& index)
{
   if (!index.isValid()) return;
   m_pCurrentStatus = m_lStatuses[index.row()];
   emit currentIndexChanged(index);
   emit currentNameChanged(m_pCurrentStatus->name);
   emit currentMessageChanged(m_pCurrentStatus->message);
   emit currentStatusChanged(m_pCurrentStatus->status);
   foreach(Account* a, AccountListModel::instance()->getAccounts()) {
      DBus::PresenceManager::instance().publish(a->id(), m_pCurrentStatus->status,m_pCurrentStatus->message);
   }
}

///Return the current status
bool PresenceStatusModel::currentStatus() const
{
   if (m_UseCustomStatus) return m_CustomStatus;
   if (!m_pCurrentStatus) return false;
   return m_UseCustomStatus?m_CustomStatus:m_pCurrentStatus->status;
}

///Return the current status message
QString PresenceStatusModel::currentMessage() const
{
   if (m_UseCustomStatus) return m_CustomMessage;
   if (!m_pCurrentStatus) return tr("N/A");
   return m_pCurrentStatus->message;
}

///Return current name
QString PresenceStatusModel::currentName() const
{
   return m_UseCustomStatus?tr("Custom"):m_pCurrentStatus?m_pCurrentStatus->name:tr("N/A");
}

///Return the default status index
QModelIndex PresenceStatusModel::defaultStatus() const
{
   if (!m_pDefaultStatus) return index(-1,-1);
   return index(m_lStatuses.indexOf(m_pDefaultStatus),0);
}

///Set the new default status
void PresenceStatusModel::setDefaultStatus( const QModelIndex& idx )
{
   if (!idx.isValid()) return;
   if (m_pDefaultStatus) {
      m_pDefaultStatus->defaultStatus = false;
      const QModelIndex& oldIdx = index(m_lStatuses.indexOf(m_pDefaultStatus),static_cast<int>(PresenceStatusModel::Columns::Default));
      emit dataChanged(oldIdx,oldIdx);
   }
   m_pDefaultStatus = m_lStatuses[idx.row()];
   m_pDefaultStatus->defaultStatus = true;
   emit defaultStatusChanged(idx);
   emit dataChanged(idx,idx);
}

bool PresenceStatusModel::isAutoTracked(AbstractItemBackendBase* backend) const
{
   return m_pVisitor->isTracked(backend);
}

void PresenceStatusModel::setAutoTracked(AbstractItemBackendBase* backend, bool tracked) const
{
   m_pVisitor->setTracked(backend,tracked);
}
