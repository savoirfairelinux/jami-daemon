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
#include <QtCore/QDebug>
// #include "config.h"
// #include <errno.h>
// #include <stdlib.h>
// #include <string.h>
// #include <gobject/gvaluecollector.h>
// #include "gtkliststore.h"
// #include "gtktreedatalist.h"
// #include "gtktreednd.h"

struct _GtkQModelPrivate
{
  GType *column_headers;
  gint  *column_roles;

  gint stamp;
  gint n_columns;
  // gint length;

  // guint columns_dirty : 1;

  // gpointer seq;         /* head of the list */
  QAbstractItemModel *model;
};

typedef union _int_ptr_t
{
  gint value;
  gpointer ptr;
} int_ptr_t;

/* static prototypes */

/* GtkTreeModel prototypes */
static void              gtk_q_model_tree_model_init (GtkTreeModelIface * );
static void              gtk_q_model_finalize        (GObject *           );
static GtkTreeModelFlags gtk_q_model_get_flags       (GtkTreeModel *      );
static gint              gtk_q_model_get_n_columns   (GtkTreeModel *      );
static GType             gtk_q_model_get_column_type (GtkTreeModel *,
                                                      gint                );
static gboolean          gtk_q_model_get_iter        (GtkTreeModel *,
                                                      GtkTreeIter *,
                                                      GtkTreePath *       );
static GtkTreePath *     gtk_q_model_get_path        (GtkTreeModel *,
                                                      GtkTreeIter *       );
static void              gtk_q_model_get_value       (GtkTreeModel *,
						                                          GtkTreeIter *,
						                                          gint,
						                                          GValue *            );
static gboolean          gtk_q_model_iter_next       (GtkTreeModel *,
						                                          GtkTreeIter *       );
static gboolean          gtk_q_model_iter_previous   (GtkTreeModel *,
						                                          GtkTreeIter *       );
static gboolean          gtk_q_model_iter_children   (GtkTreeModel *,
						                                          GtkTreeIter *,
						                                          GtkTreeIter *       );
static gboolean          gtk_q_model_iter_has_child  (GtkTreeModel *,
						                                          GtkTreeIter *       );
static gint              gtk_q_model_iter_n_children (GtkTreeModel *,
						                                          GtkTreeIter *       );
static gboolean          gtk_q_model_iter_nth_child  (GtkTreeModel *,
						                                          GtkTreeIter *,
						                                          GtkTreeIter *,
						                                          gint                );
static gboolean          gtk_q_model_iter_parent     (GtkTreeModel *,
						                                          GtkTreeIter *,
						                                          GtkTreeIter *       );

/* implementation prototypes */
static void gtk_q_model_increment_stamp (GtkQModel * );

static gint gtk_q_model_length          (GtkQModel * );

static void gtk_q_model_set_n_columns   (GtkQModel *,
                                            gint     );
static void gtk_q_model_set_column_type (GtkQModel *,
                                            gint    ,
                                            gint    ,
                                            GType    );

/* End private prototypes */

/* define type, inherit from GObject, define implemented interface(s) */
G_DEFINE_TYPE_WITH_CODE (GtkQModel, gtk_q_model, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkQModel)
       G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
            gtk_q_model_tree_model_init))

static void
gtk_q_model_class_init (GtkQModelClass *klass)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) klass;

  object_class->finalize = gtk_q_model_finalize;
}

static void
gtk_q_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_q_model_get_flags;
  iface->get_n_columns = gtk_q_model_get_n_columns;
  iface->get_column_type = gtk_q_model_get_column_type;
  iface->get_iter = gtk_q_model_get_iter;
  iface->get_path = gtk_q_model_get_path;
  iface->get_value = gtk_q_model_get_value;
  iface->iter_next = gtk_q_model_iter_next;
  iface->iter_previous = gtk_q_model_iter_previous;
  iface->iter_children = gtk_q_model_iter_children;
  iface->iter_has_child = gtk_q_model_iter_has_child;
  iface->iter_n_children = gtk_q_model_iter_n_children;
  iface->iter_nth_child = gtk_q_model_iter_nth_child;
  iface->iter_parent = gtk_q_model_iter_parent;
}

static void
gtk_q_model_init (GtkQModel *q_model)
{
  GtkQModelPrivate *priv;

  q_model->priv = (GtkQModelPrivate *)gtk_q_model_get_instance_private (q_model);
  priv = q_model->priv;

  priv->stamp = g_random_int ();
  // priv->columns_dirty = FALSE;
  // priv->length = 0;
  priv->model = NULL;
}

/**
 * gtk_q_model_new:
 * @n_columns: number of columns in the list store
 * @...: all #GType types for the columns, from first to last
 *
 * Creates a new list store as with @n_columns columns each of the types passed
 * in.  Note that only types derived from standard GObject fundamental types
 * are supported.
 *
 * As an example, <literal>gtk_q_model_new (3, G_TYPE_INT, G_TYPE_STRING,
 * GDK_TYPE_PIXBUF);</literal> will create a new #GtkQModel with three columns, of type
 * int, string and #GdkPixbuf respectively.
 *
 * Return value: a new #GtkQModel
 */
GtkQModel *
gtk_q_model_new (QAbstractItemModel *model, gint n_columns, ...)
{
  GtkQModel *retval;
  va_list args;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = (GtkQModel *)g_object_new (GTK_TYPE_Q_MODEL, NULL);
  gtk_q_model_set_n_columns (retval, n_columns);

  retval->priv->model = model;

  va_start (args, 2*n_columns);

  for (i = 0; i < n_columns; i++)
    {
      // first get the role
      // TODO: check if its a valid type?
      gint role = va_arg(args, gint);
      // then get the type
      GType type = va_arg (args, GType);
      // if (! _gtk_tree_data_list_check_type (type))
      //   {
      //     g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (type));
      //     g_object_unref (retval);
      //     va_end (args);

      //     return NULL;
      //   }

      gtk_q_model_set_column_type (retval, i, role, type);
    }

  va_end (args);

  gtk_q_model_length(retval);

  /* connect signals */
  QObject::connect(
      model,
      &QAbstractItemModel::rowsInserted,
      [=](const QModelIndex & parent, int first, int last) {
        g_debug("rows inserted, first: %d, last: %d", first, last);
        for( int row = first; row <= last; row++) {
          GtkTreeIter *iter = g_new0(GtkTreeIter, 1);
          GtkTreePath *path = gtk_tree_path_new();
          int_ptr_t row_ptr;
          row_ptr.value = row;
          iter->stamp = retval->priv->stamp;
          iter->user_data = row_ptr.ptr;
          gtk_tree_path_append_index(path, row);
          gtk_tree_model_row_inserted(GTK_TREE_MODEL(retval), path, iter);
        }
      }
  );

  QObject::connect(
      model,
      &QAbstractItemModel::rowsMoved,
      [=](const QModelIndex & sourceParent, int sourceStart, int sourceEnd, const QModelIndex & destinationParent, int destinationRow) {
        g_debug("rows moved, start: %d, end: %d, moved to: %d", sourceStart, sourceEnd, destinationRow);
        for( int row = sourceStart; row <= sourceEnd; row++) {
          /* first remove the row from old location */
          GtkTreePath *path_old = gtk_tree_path_new();
          gtk_tree_path_append_index(path_old, row);
          gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path_old);
          /* then insert it at new location */
          GtkTreeIter *iter_new = g_new0(GtkTreeIter, 1);
          GtkTreePath *path_new = gtk_tree_path_new();
          int_ptr_t row_ptr;
          row_ptr.value = destinationRow;
          iter_new->stamp = retval->priv->stamp;
          iter_new->user_data = row_ptr.ptr;
          gtk_tree_path_append_index(path_new, destinationRow);
          gtk_tree_model_row_inserted(GTK_TREE_MODEL(retval), path_new, iter_new);
          destinationRow++;
        }
      }
  );

  QObject::connect(
      model,
      &QAbstractItemModel::rowsRemoved,
      [=](const QModelIndex & parent, int first, int last) {
        g_debug("rows removed");
        for( int row = first; row <= last; row++) {
          GtkTreePath *path = gtk_tree_path_new();
          gtk_tree_path_append_index(path, row);
          gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path);
        }
      }
  );

  QObject::connect(
      model,
      &QAbstractItemModel::dataChanged,
      [=](const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> & roles = QVector<int> ()) {
        g_debug("data changed");
        /* for now assume only 1 column and no children, so get the rows */
        int first = topLeft.row();
        int last = bottomRight.row();
        for( int row = first; row <= last; row++) {
          GtkTreeIter *iter = g_new0(GtkTreeIter, 1);
          GtkTreePath *path = gtk_tree_path_new();
          int_ptr_t row_ptr;
          row_ptr.value = row;
          iter->stamp = retval->priv->stamp;
          iter->user_data = row_ptr.ptr;
          gtk_tree_path_append_index(path, row);
          gtk_tree_model_row_changed(GTK_TREE_MODEL(retval), path, iter);
        }
      }
  );

  return retval;
}

static void
gtk_list_store_increment_stamp (GtkQModel *q_model)
{
  GtkQModelPrivate *priv = q_model->priv;

  do
    {
      priv->stamp++;
    }
  while (priv->stamp == 0);
}

static gint
gtk_q_model_length (GtkQModel *q_model)
{
  GtkQModelPrivate *priv = q_model->priv;
  // g_debug("q model length: %d", priv->model->rowCount());
  return priv->model->rowCount();
}

static void
gtk_q_model_set_n_columns (GtkQModel *q_model,
            gint          n_columns)
{
  GtkQModelPrivate *priv = q_model->priv;
  int i;

  if (priv->n_columns == n_columns)
    return;

  priv->column_headers = g_renew (GType, priv->column_headers, n_columns);
  for (i = priv->n_columns; i < n_columns; i++)
    priv->column_headers[i] = G_TYPE_INVALID;
  priv->n_columns = n_columns;

  priv->column_roles = g_renew (gint, priv->column_roles, n_columns);
  for (i = priv->n_columns; i < n_columns; i++)
    priv->column_roles[i] = -1;
}

static void
gtk_q_model_set_column_type (GtkQModel *q_model,
        gint          column,
        gint          role,
        GType         type)
{
  GtkQModelPrivate *priv = q_model->priv;

  // if (!_gtk_tree_data_list_check_type (type))
  //   {
  //     g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (type));
  //     return;
  //   }

  priv->column_headers[column] = type;
  priv->column_roles[column] = role;
}


static void
gtk_q_model_finalize (GObject *object)
{
  // is there anything to do?


  // GtkQModel *q_model = GTK_Q_MODEL (object);
  // GtkQModelPrivate *priv = q_model->priv;

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

  G_OBJECT_CLASS (gtk_q_model_parent_class)->finalize (object);
}

static gboolean
iter_is_valid (GtkTreeIter *iter,
               GtkQModel   *q_model)
{
  gboolean retval;
  g_return_val_if_fail(iter != NULL, FALSE);
  // g_return_val_if_fail(iter->user_data != NULL, FALSE);
  int_ptr_t row;
  row.ptr = iter->user_data;
  // return q_model->priv->stamp == iter->stamp &&
  //        row.value >= 0 &&
  //        row.value < gtk_q_model_length(q_model);
  retval = q_model->priv->model->index(row.value, 0).isValid();

  // if (retval) {
  //   g_debug("got valid iter for model");
  // } else {
  //   g_debug("invlaide model iter, retval: %d", (int)retval);
  // }
  return retval;
}

/* Start Fulfill the GtkTreeModel requirements */

/* flags supported by this interface */
static GtkTreeModelFlags
gtk_q_model_get_flags (GtkTreeModel *tree_model)
{
  // TODO: possibly return based on the model?
  return (GtkTreeModelFlags)(GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY);
}

/* number of columns supported by this tree model */
static gint
gtk_q_model_get_n_columns (GtkTreeModel *tree_model)
{

  GtkQModel *q_model = GTK_Q_MODEL (tree_model);
  GtkQModelPrivate *priv = q_model->priv;

  // priv->columns_dirty = TRUE;

  // g_debug("getting model column number: %d", priv->model->columnCount());
  return priv->model->columnCount();
}

/* get given column type */
static GType
gtk_q_model_get_column_type (GtkTreeModel *tree_model,
				gint          index)
{

  GtkQModel *q_model = GTK_Q_MODEL (tree_model);
  GtkQModelPrivate *priv = q_model->priv;

  g_return_val_if_fail (index < gtk_q_model_get_n_columns(tree_model), G_TYPE_INVALID);

  // priv->columns_dirty = TRUE;

  // g_debug("getting column type: %s", g_type_name(priv->column_headers[index]));
  return priv->column_headers[index];
}

/* Sets @iter to a valid iterator pointing to @path.  If @path does
 * not exist, @iter is set to an invalid iterator and %FALSE is returned.
 */
static gboolean
gtk_q_model_get_iter (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter,
			 GtkTreePath  *path)
{
  // g_debug("getting model iter");
  GtkQModel *q_model = GTK_Q_MODEL (tree_model);
  GtkQModelPrivate *priv = q_model->priv;
  // GSequence *seq;
  // gint i;

  // priv->columns_dirty = TRUE;

  // seq = priv->seq;

  // i = gtk_tree_path_get_indices (path)[0];

  // if (i >= g_sequence_get_length (seq))
  //   {
  //     iter->stamp = 0;
  //     return FALSE;
  //   }

  // iter->stamp = priv->stamp;
  // iter->user_data = g_sequence_get_iter_at_pos (seq, i);

  // return TRUE;

  // temp

  /* GtkTreePath is basically the row;
   * It is the path in case the model has children,
   * but this is not the case for a list type model
   */
  /* GtkTreeIter is basically the QModelIndex,
   * it is the pointer to the item.
   * In the case of a list model, there is only one item per row.
   */
  int_ptr_t row;
  row.value = gtk_tree_path_get_indices (path)[0];
  iter->stamp = priv->stamp;
  iter->user_data = row.ptr;
  if (!iter_is_valid(iter, q_model)){
    iter->stamp = 0;
    return FALSE;
  }

  return TRUE;
}

/* Returns a newly-created #GtkTreePath referenced by @iter.
 *
 * This path should be freed with gtk_tree_path_free().
 */
static GtkTreePath *
gtk_q_model_get_path (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  // g_debug("getting model path");
  GtkQModel *q_model = GTK_Q_MODEL (tree_model);
  GtkQModelPrivate *priv = q_model->priv;
  GtkTreePath *path;

  g_return_val_if_fail (iter->stamp == priv->stamp, NULL);
  g_return_val_if_fail (iter_is_valid(iter, q_model), NULL);

  int_ptr_t row;
  row.ptr = iter->user_data;

  // if (g_sequence_iter_is_end (iter->user_data))
  //   return NULL;

  // path = gtk_tree_path_new ();
  // gtk_tree_path_append_index (path, g_sequence_iter_get_position (iter->user_data));

  // return path;

  path = gtk_tree_path_new();
  gtk_tree_path_append_index (path, row.value);
  return path;
}

static inline GType
get_fundamental_type (GType type)
{
  GType result;

  result = G_TYPE_FUNDAMENTAL (type);

  if (result == G_TYPE_INTERFACE)
    {
      if (g_type_is_a (type, G_TYPE_OBJECT))
  result = G_TYPE_OBJECT;
    }

  return result;
}

/* Initializes and sets @value to that at @column. */
static void
gtk_q_model_get_value (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  // g_debug("getting model iter value");

  GtkQModel *q_model = GTK_Q_MODEL (tree_model);
  GtkQModelPrivate *priv = q_model->priv;
  // GtkTreeDataList *list;
  gint tmp_column = column;

  g_return_if_fail (column < priv->n_columns);

  g_return_if_fail (iter_is_valid(iter, q_model));

  int_ptr_t row;
  row.ptr = iter->user_data;

  // g_return_if_fail (iter_is_valid (iter, q_model));

  // list = g_sequence_get (iter->user_data);

  // while (tmp_column-- > 0 && list)
  //   list = list->next;

  // if (list == NULL)
  //   g_value_init (value, priv->column_headers[column]);
  // else
  //   _gtk_tree_data_list_node_to_value (list,
		// 		       priv->column_headers[column],
		// 		       value);

  /* get the data */
  QModelIndex idx = priv->model->index(row.value, 0);
  int role = priv->column_roles[column];
  QVariant var = priv->model->data(idx, role);
  GType type = priv->column_headers[column];
  g_value_init (value, type);
  switch (get_fundamental_type (type))
    {
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (value, (gboolean) var.toBool());
      break;
    // case G_TYPE_CHAR:
    //   g_value_set_schar (value, (gchar) list->data.v_char);
    //   break;
    // case G_TYPE_UCHAR:
    //   g_value_set_uchar (value, (guchar) list->data.v_uchar);
    //   break;
    case G_TYPE_INT:
      g_value_set_int (value, (gint) var.toInt());
      break;
    case G_TYPE_UINT:
      g_value_set_uint (value, (guint) var.toUInt());
      break;
    // case G_TYPE_LONG:
    //   g_value_set_long (value, list->data.v_long);
    //   break;
    // case G_TYPE_ULONG:
    //   g_value_set_ulong (value, list->data.v_ulong);
    //   break;
    // case G_TYPE_INT64:
    //   g_value_set_int64 (value, list->data.v_int64);
    //   break;
    // case G_TYPE_UINT64:
    //   g_value_set_uint64 (value, list->data.v_uint64);
    //   break;
    // case G_TYPE_ENUM:
    //   g_value_set_enum (value, list->data.v_int);
    //   break;
    // case G_TYPE_FLAGS:
    //   g_value_set_flags (value, list->data.v_uint);
    //   break;
    // case G_TYPE_FLOAT:
    //   g_value_set_float (value, (gfloat) list->data.v_float);
    //   break;
    // case G_TYPE_DOUBLE:
    //   g_value_set_double (value, (gdouble) list->data.v_double);
    //   break;
    case G_TYPE_STRING:
      g_value_set_string (value, (gchar *) var.toString().toLocal8Bit().data());
      // qDebug() << "got account alias: " << var.toString();
      // g_debug("GGGGG, got account alias: %s", var.toString().toLocal8Bit().data());
      break;
    // case G_TYPE_POINTER:
    //   g_value_set_pointer (value, (gpointer) list->data.v_pointer);
    //   break;
    // case G_TYPE_BOXED:
    //   g_value_set_boxed (value, (gpointer) list->data.v_pointer);
    //   break;
    // case G_TYPE_VARIANT:
    //   g_value_set_variant (value, (gpointer) list->data.v_pointer);
    //   break;
    // case G_TYPE_OBJECT:
    //   g_value_set_object (value, (GObject *) list->data.v_pointer);
    //   break;
    default:
      g_warning ("%s: Unsupported type (%s) retrieved.", G_STRLOC, g_type_name (value->g_type));
      break;
    }

  return;
}

/* Sets @iter to point to the node following it at the current level.
 *
 * If there is no next @iter, %FALSE is returned and @iter is set
 * to be invalid.
 */
static gboolean
gtk_q_model_iter_next (GtkTreeModel  *tree_model,
			  GtkTreeIter   *iter)
{
  // g_debug("getting iter next");
  GtkQModel *q_model = GTK_Q_MODEL (tree_model);
  // GtkQModelPrivate *priv = q_model->priv;
  gboolean retval;

  g_return_val_if_fail (iter_is_valid(iter, q_model), NULL);
  int_ptr_t row;
  row.ptr = iter->user_data;

  // g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
  // iter->user_data = g_sequence_iter_next (iter->user_data);

  // retval = g_sequence_iter_is_end (iter->user_data);
  // if (retval)
  //   iter->stamp = 0;

  // return !retval;

  row.value++;
  iter->user_data = row.ptr;
  retval = iter_is_valid(iter, q_model);

  if (retval) {
    // g_debug("got next iter");
  } else {
    // g_debug("no next iter");
    iter->stamp = 0;
  }

  return retval;
}

/* Sets @iter to point to the previous node at the current level.
 *
 * If there is no previous @iter, %FALSE is returned and @iter is
 * set to be invalid.
 */
static gboolean
gtk_q_model_iter_previous (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter)
{
  // g_debug("getting prev iter");

  GtkQModel *q_model = GTK_Q_MODEL (tree_model);
  // GtkQModelPrivate *priv = q_model->priv;
  gboolean retval;

  g_return_val_if_fail (iter_is_valid(iter, q_model), NULL);
  int_ptr_t row;
  row.ptr = iter->user_data;

  // g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);

  // if (g_sequence_iter_is_begin (iter->user_data))
  //   {
  //     iter->stamp = 0;
  //     return FALSE;
  //   }

  // iter->user_data = g_sequence_iter_prev (iter->user_data);

  // return TRUE;

  row.value--;
  iter->user_data = row.ptr;
  retval = iter_is_valid(iter, q_model);

  if (!retval)
    iter->stamp = 0;

  return retval;
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
gtk_q_model_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
  // g_debug("getting model children");

  GtkQModel *q_model = (GtkQModel *) tree_model;
  GtkQModelPrivate *priv = q_model->priv;

  /* this is a list, nodes have no children */
  if (parent) {
    iter->stamp = 0;
    return FALSE;
  } else {
    // get the first node
    int_ptr_t row;
    row.value = 0;
    iter->stamp = priv->stamp;
    iter->user_data = row.ptr;
    if (iter_is_valid(iter, q_model)) {
      return TRUE;
    } else {
      iter->stamp = 0;
      return FALSE;
    }
  }

  // if (g_sequence_get_length (priv->seq) > 0)
  //   {
  //     iter->stamp = priv->stamp;
  //     iter->user_data = g_sequence_get_begin_iter (priv->seq);
  //     return TRUE;
  //   }
  // else
  //   {
  //     iter->stamp = 0;
  //     return FALSE;
  //   }
}

/* Returns %TRUE if @iter has children, %FALSE otherwise. */
static gboolean
gtk_q_model_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  // g_debug("checking if iter has children");
  return FALSE;
}

/* Returns the number of children that @iter has.
 *
 * As a special case, if @iter is %NULL, then the number
 * of toplevel nodes is returned.
 */
static gint
gtk_q_model_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  // g_debug("getting number of children of iter");
  GtkQModel *q_model = GTK_Q_MODEL (tree_model);
  GtkQModelPrivate *priv = q_model->priv;

  if (iter == NULL)
    return gtk_q_model_length(q_model);
  // if (iter == NULL)
  //   return g_sequence_get_length (priv->seq);

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
gtk_q_model_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
  // g_debug("getting nth child of itter");
  GtkQModel *q_model = GTK_Q_MODEL (tree_model);
  GtkQModelPrivate *priv = q_model->priv;
  GSequenceIter *child;

  iter->stamp = 0;

  if (parent) {
    return FALSE;
  } else {
    // get the nth node
    int_ptr_t row;
    row.value = n;
    iter->stamp = priv->stamp;
    iter->user_data = row.ptr;
    if (iter_is_valid(iter, q_model)) {
      return TRUE;
    } else {
      iter->stamp = 0;
      return FALSE;
    }
  }


  // child = g_sequence_get_iter_at_pos (priv->seq, n);

  // if (g_sequence_iter_is_end (child))
  //   return FALSE;

  // iter->stamp = priv->stamp;
  // iter->user_data = child;

  // return TRUE;
}

/* Sets @iter to be the parent of @child.
 *
 * If @child is at the toplevel, and doesn't have a parent, then
 * @iter is set to an invalid iterator and %FALSE is returned.
 * @child will remain a valid node after this function has been
 * called.
 */
static gboolean
gtk_q_model_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  // g_debug("getting parent of iter of child iter");
  iter->stamp = 0;
  return FALSE;
}

/* End Fulfill the GtkTreeModel requirements */