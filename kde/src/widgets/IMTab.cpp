/***************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                              *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/
#include "IMTab.h"

#include "../lib/InstantMessagingModel.h"
#include "../lib/Call.h"
#include <QtGui/QPainter>
#include <KDebug>
#include <KIcon>
#include <QtGui/QFont>


ImDelegates::ImDelegates(IMTab* parent) : QStyledItemDelegate(parent),m_pParent(parent)
{

}

QSize ImDelegates::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
//    QSize orig = QStyledItemDelegate::sizeHint(option,index);
   int height = 0;
   QPixmap* icon = (QPixmap*)index.data(InstantMessagingModel::MESSAGE_IMAGE_ROLE).value<void*>();
   QFontMetrics metric( option.font);
   QRect requiredRect = metric.boundingRect(0,0,m_pParent->width()-30 - 48 - 10 /*margin*/,500,Qt::TextWordWrap|Qt::AlignLeft,index.data(InstantMessagingModel::MESSAGE_TYPE_ROLE).toString());
   height+=requiredRect.height();
   height+=metric.height()+10;
   if (icon && dynamic_cast<QPixmap*>(icon) && height < icon->height()) {
      height = icon->height();
   }
   return QSize(m_pParent->width()-30,height);
}

void ImDelegates::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
   Q_ASSERT(index.isValid());
//    const QRegion cl = painter->clipRegion();
//    painter->setClipRect(option.rect);
 
  
 
   QPixmap* icon = (QPixmap*)index.data(InstantMessagingModel::MESSAGE_IMAGE_ROLE).value<void*>();
   int icnWidth = 50;
   if (icon && dynamic_cast<QPixmap*>(icon)) {
      painter->drawPixmap(option.rect.x()+5,option.rect.y()+(option.rect.height()/2)-(icon->height()/2),*icon);
      icnWidth = icon->width();
   }
   else {
      ((QAbstractListModel*) index.model())->setData(index,QPixmap(KIcon("user-identity").pixmap(QSize(48,48))),InstantMessagingModel::MESSAGE_IMAGE_ROLE);
   }

   QFontMetrics metric(painter->font());
   QString text = index.data(InstantMessagingModel::MESSAGE_TYPE_ROLE).toString();
   QRect requiredRect = metric.boundingRect(option.rect.x()+icnWidth+10,option.rect.y()+metric.height()+5,option.rect.width() - icnWidth - 10 /*margin*/,500,Qt::TextWordWrap|Qt::AlignLeft,text);
   painter->drawText(requiredRect,Qt::AlignLeft|Qt::TextWordWrap,text);

   QFont font = painter->font();
   font.setBold(true);
   painter->setFont(font);
   painter->drawText(option.rect.x()+icnWidth+10,option.rect.y()+metric.height(),index.data(InstantMessagingModel::MESSAGE_FROM_ROLE).toString());
   font.setBold(false);
   painter->setFont(font);

}

IMTab::IMTab(InstantMessagingModel* model,QWidget* parent) : QListView(parent)
{
   setModel(model);
   setAlternatingRowColors(true);
//    setWrapping(true);
   setUniformItemSizes(false);
   setItemDelegate(new ImDelegates(this));
   setVerticalScrollMode(ScrollPerPixel);
   connect(model,SIGNAL(dataChanged(QModelIndex, QModelIndex)),this,SLOT(scrollBottom()));
}


void IMTab::scrollBottom()
{
   scrollTo(model()->index(model()->rowCount()-1,0));
}