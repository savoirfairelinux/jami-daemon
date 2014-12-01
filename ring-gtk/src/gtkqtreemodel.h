/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_Q_TREE_MODEL_H__
#define __GTK_Q_TREE_MODEL_H__

#include <gtk/gtk.h>
#include <QtCore/QAbstractItemModel>
#include "gtkaccessproxymodel.h"

G_BEGIN_DECLS

typedef union _int_ptr_t
{
  gint value;
  gpointer ptr;
} int_ptr_t;

typedef struct _QIter {
  gint stamp;
  int_ptr_t row;
  int_ptr_t column;
  quintptr id;
  gpointer user_data;
} QIter;

#define Q_ITER(iter) ((QIter *)iter)

#define GTK_TYPE_Q_TREE_MODEL	        (gtk_q_tree_model_get_type ())
#define GTK_Q_TREE_MODEL(obj)	        (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_Q_TREE_MODEL, GtkQTreeModel))
#define GTK_Q_TREE_MODEL_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass),  GTK_TYPE_Q_TREE_MODEL, GtkQTreeModelClass))
#define GTK_IS_Q_TREE_MODEL(obj)	    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_Q_TREE_MODEL))
#define GTK_IS_Q_TREE_MODEL_CLASS(klass)(G_TYPE_CHECK_CLASS_TYPE ((klass),  GTK_TYPE_Q_TREE_MODEL))
#define GTK_Q_TREE_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),  GTK_TYPE_Q_TREE_MODEL, GtkQTreeModelClass))

typedef struct _GtkQTreeModel              GtkQTreeModel;
typedef struct _GtkQTreeModelPrivate       GtkQTreeModelPrivate;
typedef struct _GtkQTreeModelClass         GtkQTreeModelClass;

struct _GtkQTreeModel
{
  GObject parent;

  /*< private >*/
  GtkQTreeModelPrivate *priv;
};

struct _GtkQTreeModelClass
{
  GObjectClass parent_class;
};


GType               gtk_q_tree_model_get_type            (void) G_GNUC_CONST;
GtkQTreeModel      *gtk_q_tree_model_new                 (QAbstractItemModel *, size_t, ...);
QAbstractItemModel *gtk_q_tree_model_get_qmodel          (GtkQTreeModel *);
QModelIndex         gtk_q_tree_model_get_source_idx      (GtkQTreeModel *, GtkTreeIter *);
gboolean            gtk_q_tree_model_source_index_to_iter(GtkQTreeModel *, const QModelIndex &, GtkTreeIter *);

G_END_DECLS


#endif /* __GTK_Q_TREE_MODEL_H__ */
