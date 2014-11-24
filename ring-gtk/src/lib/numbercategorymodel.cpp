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
#include "numbercategorymodel.h"
#include "visitors/numbercategoryvisitor.h"
#include "phonenumber.h"
#include "numbercategory.h"

NumberCategoryModel* NumberCategoryModel::m_spInstance = nullptr;
NumberCategory*      NumberCategoryModel::m_spOther    = nullptr;

NumberCategoryModel::NumberCategoryModel(QObject* parent) : QAbstractListModel(parent),m_pVisitor(nullptr)
{

}

//Abstract model member
QVariant NumberCategoryModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid()) return QVariant();
   switch (role) {
      case Qt::DisplayRole: {
         const QString name = m_lCategories[index.row()]->category->name();
         return name.isEmpty()?tr("Uncategorized"):name;
      }
      case Qt::DecorationRole:
         return m_lCategories[index.row()]->category->icon();//m_pVisitor->icon(m_lCategories[index.row()]->icon);
      case Qt::CheckStateRole:
         return m_lCategories[index.row()]->enabled?Qt::Checked:Qt::Unchecked;
      case Role::INDEX:
         return m_lCategories[index.row()]->index;
      case Qt::UserRole:
         return 'x'+QString::number(m_lCategories[index.row()]->counter);
   }
   return QVariant();
}

int NumberCategoryModel::rowCount(const QModelIndex& parent) const
{
   if (parent.isValid()) return 0;
   return m_lCategories.size();
}

Qt::ItemFlags NumberCategoryModel::flags(const QModelIndex& index) const
{
   Q_UNUSED(index)
   return (m_lCategories[index.row()]->category->name().isEmpty()?Qt::NoItemFlags :Qt::ItemIsEnabled) | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

bool NumberCategoryModel::setData(const QModelIndex& idx, const QVariant &value, int role)
{
   if (idx.isValid() && role == Qt::CheckStateRole) {
      m_lCategories[idx.row()]->enabled = value.toBool();
      emit dataChanged(idx,idx);
      return true;
   }
   return false;
}

NumberCategory* NumberCategoryModel::addCategory(const QString& name, QVariant icon, int index, bool enabled)
{
   InternalTypeRepresentation* rep = m_hByName[name];
   if (!rep) {
      rep = new InternalTypeRepresentation();
      rep->counter = 0      ;
   }
   NumberCategory* cat = new NumberCategory(this,name);
   cat->setIcon(icon);
   rep->category   = cat    ;
   rep->index      = index  ;
   rep->enabled    = enabled;
   m_hByIdx[index] = rep    ;
   m_hByName[name] = rep    ;
   m_lCategories  << rep    ;
   emit layoutChanged()     ;
   return cat;
}

NumberCategoryModel* NumberCategoryModel::instance()
{
   if (!m_spInstance)
      m_spInstance = new NumberCategoryModel();
   return m_spInstance;
}

void NumberCategoryModel::setIcon(int idx, QVariant icon)
{
   InternalTypeRepresentation* rep = m_hByIdx[idx];
   if (rep) {
      rep->category->setIcon(icon);
      emit dataChanged(index(m_lCategories.indexOf(rep),0),index(m_lCategories.indexOf(rep),0));
   }
}

void NumberCategoryModel::setVisitor(NumberCategoryVisitor* visitor)
{
   m_pVisitor = visitor;
   m_pVisitor->load(this);
}

NumberCategoryVisitor* NumberCategoryModel::visitor() const
{
   return m_pVisitor;
}

void NumberCategoryModel::save()
{
   if (m_pVisitor) {
      m_pVisitor->serialize(this);
   }
   else
      qDebug() << "Cannot save NumberCategoryModel as there is no defined backend";
}

QModelIndex NumberCategoryModel::nameToIndex(const QString& name) const
{
   if (!m_hByName[name])
      return QModelIndex();
   else {
      return index(m_hByName[name]->index,0);
   }
}

///Be sure the category exist, increment the counter
void NumberCategoryModel::registerNumber(PhoneNumber* number)
{
   InternalTypeRepresentation* rep = m_hByName[number->category()->name()];
   if (!rep) {
      addCategory(number->category()->name(),QVariant(),-1,true);
      rep = m_hByName[number->category()->name()];
   }
   rep->counter++;
}

void NumberCategoryModel::unregisterNumber(PhoneNumber* number)
{
   InternalTypeRepresentation* rep = m_hByName[number->category()->name()];
   if (rep)
      rep->counter--;
}

NumberCategory* NumberCategoryModel::getCategory(const QString& type)
{
   InternalTypeRepresentation* internal = m_hByName[type];
   if (internal)
      return internal->category;
   return addCategory(type,QVariant());
}


NumberCategory* NumberCategoryModel::other()
{
   if (instance()->m_hByName["Other"])
      return instance()->m_hByName["Other"]->category;
   if (!m_spOther)
      m_spOther = new NumberCategory(instance(),"Other");
   return m_spOther;
}
