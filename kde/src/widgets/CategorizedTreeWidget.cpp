/***************************************************************************
 *   Copyright (C) 2009 by Rafael Fernández López <ereslibre@kde.org>      *
 *   @author: Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *                                                                         *
 * This library is free software; you can redistribute it and/or           *
 * modify it under the terms of the GNU Library General Public             *
 * License version 2 as published by the Free Software Foundation.         *
 *                                                                         *
 * This library is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       *
 * Library General Public License for more details.                        *
 *                                                                         *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to    *
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,    *
 * Boston, MA 02110-1301, USA.                                             *
 **************************************************************************/

#include "CategorizedTreeWidget.h"

#include "CategoryDrawer.h"

#include <QtGui/QStyledItemDelegate>
#include <QtGui/QPainter>
#include <QtGui/QHeaderView>

#include <klocale.h>
#include <kdebug.h>

#include <QDebug>
#include <QEvent>
#include <QKeyEvent>

//BEGIN KateColorTreeDelegate
class KateColorTreeDelegate : public QStyledItemDelegate
{
  public:
    KateColorTreeDelegate(CategorizedTreeWidget* widget)
      : QStyledItemDelegate(widget)
      , m_tree(widget)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
      QSize sh = QStyledItemDelegate::sizeHint(option, index);
      if (!index.parent().isValid()) {
        sh.rheight() += 2 * m_categoryDrawer.leftMargin();
      } else {
        sh.rheight() += m_categoryDrawer.leftMargin();
      }
      if (index.column() == 0) {
        sh.rwidth() += m_categoryDrawer.leftMargin();
      } else if (index.column() == 1) {
        sh.rwidth() = 150;
      } else {
        sh.rwidth() += m_categoryDrawer.leftMargin();
      }

      return sh;
    }

    QRect fullCategoryRect(const QStyleOptionViewItem& option, const QModelIndex& index) const {
      QModelIndex i(index),old(index);
      if (i.parent().isValid()) {
        i = i.parent();
      }

      //Avoid repainting the category over and over (optimization)
      //note: 0,0,0,0 is actually wrong, but it wont change anything for this use case
      if (i != old && old.row()>2)
         return QRect(0,0,0,0);

      QTreeWidgetItem* item = m_tree->itemFromIndex(i);
      QRect r = m_tree->visualItemRect(item);

      // adapt width
      r.setLeft(m_categoryDrawer.leftMargin());
      r.setWidth(m_tree->viewport()->width() - m_categoryDrawer.leftMargin() - m_categoryDrawer.rightMargin());

      // adapt height
      if (item->isExpanded() && item->childCount() > 0) {
        const int childCount = item->childCount();
        const int h = sizeHint(option, index.child(0, 0)).height();
        //There is a massive implact on CPU usage to have massive rect
        r.setHeight((r.height() + childCount * h > 100)?100:(r.height() + childCount * h));
      }

      r.setTop(r.top() + m_categoryDrawer.leftMargin());

      return r;
    }

    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
      Q_ASSERT(index.isValid());
      
      //BEGIN: draw toplevel items
      if (!index.parent().isValid()) {
        QStyleOptionViewItem opt(option);
        const QRegion cl = painter->clipRegion();
        painter->setClipRect(opt.rect);
        opt.rect = fullCategoryRect(option, index);
        m_categoryDrawer.drawCategory(index, 0, opt, painter);
        painter->setClipRegion(cl);
        return;
      }
      //END: draw toplevel items
      
      //BEGIN: draw background of category for all other items
      if (!index.parent().parent().isValid()) {
        QStyleOptionViewItem opt(option);
        opt.rect = fullCategoryRect(option, index);
        const QRegion cl = painter->clipRegion();
        QRect cr = option.rect;
        if (index.column() == 0) {
          if (m_tree->layoutDirection() == Qt::LeftToRight) {
            cr.setLeft(5);
          } else {
            cr.setRight(opt.rect.right());
          }
        }
        painter->setClipRect(cr);
        m_categoryDrawer.drawCategory(index, 0, opt, painter);
        painter->setClipRegion(cl);
        painter->setRenderHint(QPainter::Antialiasing, false);
      }
      //END: draw background of category for all other items

      painter->setClipRect(option.rect);
      if (option.state & QStyle::State_Selected) {
         QStyledItemDelegate::paint(painter,option,index);
      }

      QTreeWidgetItem* item = m_tree->itemFromIndex(index);
      if (item) {
         QWidget* widget = m_tree->itemWidget(item,0);
         if (widget) {
            widget->setMinimumSize((m_tree->viewport()->width() - m_categoryDrawer.leftMargin() - m_categoryDrawer.rightMargin())-m_categoryDrawer.leftMargin(),10);
            widget->setMaximumSize((m_tree->viewport()->width() - m_categoryDrawer.leftMargin() - m_categoryDrawer.rightMargin())-m_categoryDrawer.leftMargin(),99999);
         }
      }
    }

  private:
    CategorizedTreeWidget* m_tree;
    CategoryDrawer m_categoryDrawer;
};
//END KateColorTreeDelegate

CategorizedTreeWidget::CategorizedTreeWidget(QWidget *parent)
  : QTreeWidget(parent)
{
  setItemDelegate(new KateColorTreeDelegate(this));
  setHeaderHidden(true);
  setRootIsDecorated(false);
  //setUniformRowHeights(false);
  setIndentation(25);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerItem);
}

void CategorizedTreeWidget::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
  Q_UNUSED(painter)
  Q_UNUSED(rect)
  Q_UNUSED(index)
//   if (index.parent() != QModelIndex() && index.parent().parent() != QModelIndex())
//     QTreeWidget::drawBranches(painter,rect,index);
}

QVector<QTreeWidgetItem*> CategorizedTreeWidget::realItems() const
{
  return m_lItems;
}