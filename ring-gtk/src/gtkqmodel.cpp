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

#include "gtkqmodel.h"
#include <gtk/gtk.h>
#include <QtCore/QAbstractItemModel>
// #include "config.h"
// #include <errno.h>
// #include <stdlib.h>
// #include <string.h>
// #include <gobject/gvaluecollector.h>
// #include "gtkliststore.h"
// #include "gtktreedatalist.h"
// #include "gtktreednd.h"

struct _GtkListStorePrivate
{
  GType *column_headers;

  gint stamp;
  gint n_columns;
  gint length;

  guint columns_dirty : 1;

  gpointer seq;         /* head of the list */
};

#define GTK_LIST_STORE_IS_SORTED(list) (((GtkListStore*)(list))->priv->sort_column_id != GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
static void         gtk_list_store_tree_model_init (GtkTreeModelIface *iface);
static void         gtk_list_store_finalize        (GObject           *object);
static GtkTreeModelFlags gtk_list_store_get_flags  (GtkTreeModel      *tree_model);
static gint         gtk_list_store_get_n_columns   (GtkTreeModel      *tree_model);
static GType        gtk_list_store_get_column_type (GtkTreeModel      *tree_model,
						    gint               index);
static gboolean     gtk_list_store_get_iter        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreePath       *path);
static GtkTreePath *gtk_list_store_get_path        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static void         gtk_list_store_get_value       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    gint               column,
						    GValue            *value);
static gboolean     gtk_list_store_iter_next       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_list_store_iter_previous   (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_list_store_iter_children   (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent);
static gboolean     gtk_list_store_iter_has_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gint         gtk_list_store_iter_n_children (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_list_store_iter_nth_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent,
						    gint               n);
static gboolean     gtk_list_store_iter_parent     (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *child);


static void
gtk_list_store_class_init (GtkListStoreClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;

  object_class->finalize = gtk_list_store_finalize;
}

static void
gtk_list_store_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_list_store_get_flags;
  iface->get_n_columns = gtk_list_store_get_n_columns;
  iface->get_column_type = gtk_list_store_get_column_type;
  iface->get_iter = gtk_list_store_get_iter;
  iface->get_path = gtk_list_store_get_path;
  iface->get_value = gtk_list_store_get_value;
  iface->iter_next = gtk_list_store_iter_next;
  iface->iter_previous = gtk_list_store_iter_previous;
  iface->iter_children = gtk_list_store_iter_children;
  iface->iter_has_child = gtk_list_store_iter_has_child;
  iface->iter_n_children = gtk_list_store_iter_n_children;
  iface->iter_nth_child = gtk_list_store_iter_nth_child;
  iface->iter_parent = gtk_list_store_iter_parent;
}

static void
gtk_list_store_init (GtkListStore *list_store)
{
  GtkListStorePrivate *priv;

  list_store->priv = gtk_list_store_get_instance_private (list_store);
  priv = list_store->priv;

  priv->stamp = g_random_int ();
  priv->columns_dirty = FALSE;
  priv->length = 0;
}

/**
 * gtk_list_store_new:
 * @n_columns: number of columns in the list store
 * @...: all #GType types for the columns, from first to last
 *
 * Creates a new list store as with @n_columns columns each of the types passed
 * in.  Note that only types derived from standard GObject fundamental types
 * are supported.
 *
 * As an example, <literal>gtk_list_store_new (3, G_TYPE_INT, G_TYPE_STRING,
 * GDK_TYPE_PIXBUF);</literal> will create a new #GtkListStore with three columns, of type
 * int, string and #GdkPixbuf respectively.
 *
 * Return value: a new #GtkListStore
 */
GtkListStore *
gtk_list_store_new (gint n_columns,
		    ...)
{
  // GtkListStore *retval;
  // va_list args;
  // gint i;

  // g_return_val_if_fail (n_columns > 0, NULL);

  // retval = g_object_new (GTK_TYPE_LIST_STORE, NULL);
  // gtk_list_store_set_n_columns (retval, n_columns);

  // va_start (args, n_columns);

  // for (i = 0; i < n_columns; i++)
  //   {
  //     GType type = va_arg (args, GType);
  //     if (! _gtk_tree_data_list_check_type (type))
  //       {
  //         g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (type));
  //         g_object_unref (retval);
  //         va_end (args);

  //         return NULL;
  //       }

  //     gtk_list_store_set_column_type (retval, i, type);
  //   }

  // va_end (args);

  // return retval;

  return g_object_new (GTK_TYPE_LIST_STORE, NULL);
}


static void
gtk_list_store_finalize (GObject *object)
{
  // is there anything to do?


  // GtkListStore *list_store = GTK_LIST_STORE (object);
  // GtkListStorePrivate *priv = list_store->priv;

  // g_sequence_foreach (priv->seq,
		//       (GFunc) _gtk_tree_data_list_free, priv->column_headers);

  // g_sequence_free (priv->seq);

  // _gtk_tree_data_list_header_free (priv->sort_list);
  // g_free (priv->column_headers);

  // if (priv->default_sort_destroy)
  //   {
  //     GDestroyNotify d = priv->default_sort_destroy;

  //     priv->default_sort_destroy = NULL;
  //     d (priv->default_sort_data);
  //     priv->default_sort_data = NULL;
  //   }

  // G_OBJECT_CLASS (gtk_list_store_parent_class)->finalize (object);
}

/* Start Fulfill the GtkTreeModel requirements */

/* flags supported by this interface */
static GtkTreeModelFlags
gtk_list_store_get_flags (GtkTreeModel *tree_model)
{
  // TODO: possibly return based on the model?
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

/* number of columns supported by this tree model */
static gint
gtk_list_store_get_n_columns (GtkTreeModel *tree_model)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;

  priv->columns_dirty = TRUE;

  return priv->n_columns;
}

/* get given column type */
static GType
gtk_list_store_get_column_type (GtkTreeModel *tree_model,
				gint          index)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;

  g_return_val_if_fail (index < priv->n_columns, G_TYPE_INVALID);

  priv->columns_dirty = TRUE;

  return priv->column_headers[index];
}

/* Sets @iter to a valid iterator pointing to @path.  If @path does
 * not exist, @iter is set to an invalid iterator and %FALSE is returned.
 */
static gboolean
gtk_list_store_get_iter (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter,
			 GtkTreePath  *path)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  GSequence *seq;
  gint i;

  priv->columns_dirty = TRUE;

  seq = priv->seq;

  i = gtk_tree_path_get_indices (path)[0];

  if (i >= g_sequence_get_length (seq))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->stamp = priv->stamp;
  iter->user_data = g_sequence_get_iter_at_pos (seq, i);

  return TRUE;
}

/* Returns a newly-created #GtkTreePath referenced by @iter.
 *
 * This path should be freed with gtk_tree_path_free().
 */
static GtkTreePath *
gtk_list_store_get_path (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  GtkTreePath *path;

  g_return_val_if_fail (iter->stamp == priv->stamp, NULL);

  if (g_sequence_iter_is_end (iter->user_data))
    return NULL;

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, g_sequence_iter_get_position (iter->user_data));

  return path;
}

/* Returns a newly-created #GtkTreePath referenced by @iter. */
static void
gtk_list_store_get_value (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  GtkTreeDataList *list;
  gint tmp_column = column;

  g_return_if_fail (column < priv->n_columns);
  g_return_if_fail (iter_is_valid (iter, list_store));

  list = g_sequence_get (iter->user_data);

  while (tmp_column-- > 0 && list)
    list = list->next;

  if (list == NULL)
    g_value_init (value, priv->column_headers[column]);
  else
    _gtk_tree_data_list_node_to_value (list,
				       priv->column_headers[column],
				       value);
}

/* Sets @iter to point to the node following it at the current level.
 *
 * If there is no next @iter, %FALSE is returned and @iter is set
 * to be invalid.
 */
static gboolean
gtk_list_store_iter_next (GtkTreeModel  *tree_model,
			  GtkTreeIter   *iter)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  gboolean retval;

  g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
  iter->user_data = g_sequence_iter_next (iter->user_data);

  retval = g_sequence_iter_is_end (iter->user_data);
  if (retval)
    iter->stamp = 0;

  return !retval;
}

/* Sets @iter to point to the previous node at the current level.
 *
 * If there is no previous @iter, %FALSE is returned and @iter is
 * set to be invalid.
 */
static gboolean
gtk_list_store_iter_previous (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;

  g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);

  if (g_sequence_iter_is_begin (iter->user_data))
    {
      iter->stamp = 0;
      return FALSE;
    }

  iter->user_data = g_sequence_iter_prev (iter->user_data);

  return TRUE;
}

/* Sets @iter to point to the first child of @parent.
 *
 * If @parent has no children, %FALSE is returned and @iter is
 * set to be invalid. @parent will remain a valid node after this
 * function has been called.
 *
 * If @parent is %NULL returns the first node
 */
static gboolean
gtk_list_store_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
  GtkListStore *list_store = (GtkListStore *) tree_model;
  GtkListStorePrivate *priv = list_store->priv;

  /* this is a list, nodes have no children */
  if (parent)
    {
      iter->stamp = 0;
      return FALSE;
    }

  if (g_sequence_get_length (priv->seq) > 0)
    {
      iter->stamp = priv->stamp;
      iter->user_data = g_sequence_get_begin_iter (priv->seq);
      return TRUE;
    }
  else
    {
      iter->stamp = 0;
      return FALSE;
    }
}

/* Returns %TRUE if @iter has children, %FALSE otherwise. */
static gboolean
gtk_list_store_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  return FALSE;
}

/* Returns the number of children that @iter has.
 *
 * As a special case, if @iter is %NULL, then the number
 * of toplevel nodes is returned.
 */
static gint
gtk_list_store_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;

  if (iter == NULL)
    return g_sequence_get_length (priv->seq);

  g_return_val_if_fail (priv->stamp == iter->stamp, -1);

  return 0;
}

/* Sets @iter to be the child of @parent, using the given index.
 *
 * The first index is 0. If @n is too big, or @parent has no children,
 * @iter is set to an invalid iterator and %FALSE is returned. @parent
 * will remain a valid node after this function has been called. As a
 * special case, if @parent is %NULL, then the @n<!-- -->th root node
 * is set.
 */
static gboolean
gtk_list_store_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
  GtkListStore *list_store = GTK_LIST_STORE (tree_model);
  GtkListStorePrivate *priv = list_store->priv;
  GSequenceIter *child;

  iter->stamp = 0;

  if (parent)
    return FALSE;

  child = g_sequence_get_iter_at_pos (priv->seq, n);

  if (g_sequence_iter_is_end (child))
    return FALSE;

  iter->stamp = priv->stamp;
  iter->user_data = child;

  return TRUE;
}

/* Sets @iter to be the parent of @child.
 *
 * If @child is at the toplevel, and doesn't have a parent, then
 * @iter is set to an invalid iterator and %FALSE is returned.
 * @child will remain a valid node after this function has been
 * called.
 */
static gboolean
gtk_list_store_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  iter->stamp = 0;
  return FALSE;
}

/* End Fulfill the GtkTreeModel requirements */