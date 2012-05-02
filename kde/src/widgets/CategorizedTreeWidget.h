/* This file is part of the KDE libraries
   Copyright (C) 2012 Dominik Haumann <dhaumann kde org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KATE_COLOR_TREE_WIDGET_H
#define KATE_COLOR_TREE_WIDGET_H

#include <QtGui/QTreeWidget>

class KConfigGroup;
class KateColorTreeItem;
class QTreeWidgetItem;

class CategorizedTreeWidget : public QTreeWidget
{
  Q_OBJECT
  friend class KateColorTreeItem;
  friend class KateColorTreeDelegate;

  public:
    explicit CategorizedTreeWidget(QWidget *parent = 0);

  public:
    template <class T = QTreeWidgetItem> T* addItem(QString category);
    QTreeWidgetItem* addCategory(QString name);

    QVector<QTreeWidgetItem*> realItems() const;

  Q_SIGNALS:
    void changed();

  protected:
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const;
  private:
    QVector<QTreeWidgetItem*> m_lItems;
};

template <class T> T* CategorizedTreeWidget::addItem(QString category)
{
  QTreeWidgetItem* categoryItem = 0;
  for (int i = 0; i < topLevelItemCount(); ++i) {
    if (topLevelItem(i)->text(0) == category) {
      categoryItem = topLevelItem(i);
      break;
    }
  }

  if (!categoryItem) {
    categoryItem =addCategory(category);
  }
  setItemHidden(categoryItem,false);

  T* iwdg =  new T(categoryItem);
  resizeColumnToContents(0);
  m_lItems << iwdg;
  return iwdg;
}

// QTreeWidgetItem* CategorizedTreeWidget::addItem(QString category)
// {
//   return addItem<QTreeWidgetItem>(category);
// }

#endif