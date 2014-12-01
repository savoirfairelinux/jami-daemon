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

#include "gtkqtreemodel.h"
#include <gtk/gtk.h>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QDebug>
#include "gtkaccessproxymodel.h"

struct _GtkQTreeModelPrivate
{
  GType *column_headers;
  gint  *column_roles;

  gint stamp;
  gint n_columns;

  GtkAccessProxyModel *model;
};

/* static prototypes */

/* GtkTreeModel prototypes */
static void              gtk_q_tree_model_tree_model_init (GtkTreeModelIface * );
static void              gtk_q_tree_model_finalize        (GObject *           );
static GtkTreeModelFlags gtk_q_tree_model_get_flags       (GtkTreeModel *      );
static gint              gtk_q_tree_model_get_n_columns   (GtkTreeModel *      );
static GType             gtk_q_tree_model_get_column_type (GtkTreeModel *,
                                                      gint                );
static gboolean          gtk_q_tree_model_get_iter        (GtkTreeModel *,
                                                      GtkTreeIter *,
                                                      GtkTreePath *       );
static GtkTreePath *     gtk_q_tree_model_get_path        (GtkTreeModel *,
                                                      GtkTreeIter *       );
static void              gtk_q_tree_model_get_value       (GtkTreeModel *,
						                                          GtkTreeIter *,
						                                          gint,
						                                          GValue *            );
static gboolean          gtk_q_tree_model_iter_next       (GtkTreeModel *,
						                                          GtkTreeIter *       );
static gboolean          gtk_q_tree_model_iter_previous   (GtkTreeModel *,
						                                          GtkTreeIter *       );
static gboolean          gtk_q_tree_model_iter_children   (GtkTreeModel *,
						                                          GtkTreeIter *,
						                                          GtkTreeIter *       );
static gboolean          gtk_q_tree_model_iter_has_child  (GtkTreeModel *,
						                                          GtkTreeIter *       );
static gint              gtk_q_tree_model_iter_n_children (GtkTreeModel *,
						                                          GtkTreeIter *       );
static gboolean          gtk_q_tree_model_iter_nth_child  (GtkTreeModel *,
						                                          GtkTreeIter *,
						                                          GtkTreeIter *,
						                                          gint                );
static gboolean          gtk_q_tree_model_iter_parent     (GtkTreeModel *,
						                                          GtkTreeIter *,
						                                          GtkTreeIter *       );

/* implementation prototypes */
static void qmodelindex_to_iter              (const QModelIndex &,
                                              GtkTreeIter *);
// static void gtk_q_tree_model_increment_stamp (GtkQTreeModel * );

static gint gtk_q_tree_model_length          (GtkQTreeModel * );

static void gtk_q_tree_model_set_n_columns   (GtkQTreeModel *,
                                            gint     );
static void gtk_q_tree_model_set_column_type (GtkQTreeModel *,
                                            gint    ,
                                            gint    ,
                                            GType    );

/* End private prototypes */

/* define type, inherit from GObject, define implemented interface(s) */
G_DEFINE_TYPE_WITH_CODE (GtkQTreeModel, gtk_q_tree_model, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkQTreeModel)
       G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
            gtk_q_tree_model_tree_model_init))

static void
gtk_q_tree_model_class_init (GtkQTreeModelClass *klass)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) klass;

  object_class->finalize = gtk_q_tree_model_finalize;
}

static void
gtk_q_tree_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_q_tree_model_get_flags;
  iface->get_n_columns = gtk_q_tree_model_get_n_columns;
  iface->get_column_type = gtk_q_tree_model_get_column_type;
  iface->get_iter = gtk_q_tree_model_get_iter;
  iface->get_path = gtk_q_tree_model_get_path;
  iface->get_value = gtk_q_tree_model_get_value;
  iface->iter_next = gtk_q_tree_model_iter_next;
  iface->iter_previous = gtk_q_tree_model_iter_previous;
  iface->iter_children = gtk_q_tree_model_iter_children;
  iface->iter_has_child = gtk_q_tree_model_iter_has_child;
  iface->iter_n_children = gtk_q_tree_model_iter_n_children;
  iface->iter_nth_child = gtk_q_tree_model_iter_nth_child;
  iface->iter_parent = gtk_q_tree_model_iter_parent;
}

static void
gtk_q_tree_model_init (GtkQTreeModel *q_tree_model)
{
  GtkQTreeModelPrivate *priv;

  q_tree_model->priv = (GtkQTreeModelPrivate *)gtk_q_tree_model_get_instance_private (q_tree_model);
  priv = q_tree_model->priv;

  priv->stamp = g_random_int ();

  priv->model = NULL;
}

/**
 * gtk_q_tree_model_get_qmodel
 * returns the original model from which this GtkQTreeModel is created
 */
QAbstractItemModel *
gtk_q_tree_model_get_qmodel (GtkQTreeModel *q_tree_model)
{
  GtkQTreeModelPrivate *priv;
  priv = (GtkQTreeModelPrivate *)gtk_q_tree_model_get_instance_private(q_tree_model);
  return priv->model->sourceModel();
}

/**
 * gtk_q_tree_model_get_source_idx
 * Returns the index of the original model used to create this GtkQTreeModel from
 * the given iter, if there is one.
 */
QModelIndex
gtk_q_tree_model_get_source_idx(GtkQTreeModel *q_tree_model, GtkTreeIter *iter)
{
  GtkQTreeModelPrivate *priv;
  priv = (GtkQTreeModelPrivate *)gtk_q_tree_model_get_instance_private(q_tree_model);
  /* get the call */
  QIter *qiter = Q_ITER(iter);
  GtkAccessProxyModel *proxy_model = priv->model;
  QModelIndex proxy_idx = proxy_model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
  if (proxy_idx.isValid()) {
    /* we have the proxy model idx, now get the actual idx so we can get the call object */
    g_debug("got valid model index");
    return proxy_model->mapToSource(proxy_idx);
  } else {
    g_debug("returning invlaid model index");
    return QModelIndex();
  }
}

/**
 * Takes a QModelIndex from the original QAbstractItemModel and returns a valid GtkTreeIter in the corresponding
 * GtkQTreeModel
 */
gboolean
gtk_q_tree_model_source_index_to_iter(GtkQTreeModel *q_tree_model, const QModelIndex &idx, GtkTreeIter *iter)
{
  GtkQTreeModelPrivate *priv;
  priv = (GtkQTreeModelPrivate *)gtk_q_tree_model_get_instance_private(q_tree_model);

  /* make sure its an iter from the right model */
  g_return_val_if_fail(idx.model() == priv->model->sourceModel(), FALSE);

  /* make sure iter is valid */
  iter->stamp = priv->stamp;

  /* the the proxy_idx from the source idx */
  QModelIndex proxy_idx = priv->model->mapFromSource(idx);

  /* map the proxy idx to iter */
  Q_ITER(iter)->row.value = proxy_idx.row();
  Q_ITER(iter)->column.value = proxy_idx.row();
  Q_ITER(iter)->id = proxy_idx.internalId();
  return TRUE;
}

/**
 * gtk_q_tree_model_new:
 * @model: QAbstractItemModel to which this model will bind.
 * @n_columns: number of columns in the list store
 * @...: all #GType follwed by the #Role pair for each column.
 *
 * Return value: a new #GtkQTreeModel
 */
GtkQTreeModel *
gtk_q_tree_model_new (QAbstractItemModel *model, size_t n_columns, ...)
{
  GtkQTreeModel *retval;
  va_list args;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = (GtkQTreeModel *)g_object_new (GTK_TYPE_Q_TREE_MODEL, NULL);
  gtk_q_tree_model_set_n_columns (retval, n_columns);

  /* get proxy model from given model */
  GtkAccessProxyModel* proxy_model = new GtkAccessProxyModel();
  proxy_model->setSourceModel(model);
  retval->priv->model = proxy_model;
  gint stamp = retval->priv->stamp;

  n_columns = 2*n_columns;
  va_start (args, n_columns);

  for (i = 0; i < (gint)(n_columns/2); i++)
    {
      // g_debug("adding column %d", i);
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

      gtk_q_tree_model_set_column_type (retval, i, role, type);
    }

  va_end (args);

  gtk_q_tree_model_length(retval);

  /* connect signals */
  QObject::connect(
      model,
      &QAbstractItemModel::rowsInserted,
      [=](const QModelIndex & parent, int first, int last) {
        // g_debug("rows inserted, first: %d, last: %d", first, last);
        for( int row = first; row <= last; row++) {
          GtkTreeIter *iter = g_new0(GtkTreeIter, 1);
          QModelIndex idx = retval->priv->model->index(row, 0, parent);
          iter->stamp = stamp; //retval->priv->stamp;
          qmodelindex_to_iter(idx, iter);
          GtkTreePath *path = gtk_q_tree_model_get_path(GTK_TREE_MODEL(retval), iter);
          gtk_tree_model_row_inserted(GTK_TREE_MODEL(retval), path, iter);
        }
      }
  );

  QObject::connect(
      model,
      &QAbstractItemModel::rowsAboutToBeMoved,
      [=](const QModelIndex & sourceParent, int sourceStart, int sourceEnd, const QModelIndex & destinationParent, int destinationRow) {
        g_debug("rows about to be moved, start: %d, end: %d, moved to: %d", sourceStart, sourceEnd, destinationRow);
        /* first remove the row from old location
         * then insert them at the new location on the "rowsMoved signal */
        for( int row = sourceStart; row <= sourceEnd; row++) {
          QModelIndex idx = retval->priv->model->index(row, 0, sourceParent); //sourceParent.child(row, 0);
          GtkTreeIter iter_old;
          qmodelindex_to_iter(idx, &iter_old);
          GtkTreePath *path_old = gtk_q_tree_model_get_path(GTK_TREE_MODEL(retval), &iter_old);
          gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path_old);
        }
      }
  );

  QObject::connect(
      model,
      &QAbstractItemModel::rowsMoved,
      [=](const QModelIndex & sourceParent, int sourceStart, int sourceEnd, const QModelIndex & destinationParent, int destinationRow) {
        g_debug("rows moved, start: %d, end: %d, moved to: %d", sourceStart, sourceEnd, destinationRow);
        /* these rows should have been removed in the "rowsAboutToBeMoved" handler
         * now insert them in the new location */
        for( int row = sourceStart; row <= sourceEnd; row++) {
          GtkTreeIter *iter_new = g_new0(GtkTreeIter, 1);
          QModelIndex idx = retval->priv->model->index(destinationRow, 0, destinationParent); //destinationParent.child(destinationRow, 0);
          iter_new->stamp = stamp; //retval->priv->stamp;
          qmodelindex_to_iter(idx, iter_new);
          GtkTreePath *path_new = gtk_q_tree_model_get_path(GTK_TREE_MODEL(retval), iter_new);
          gtk_tree_model_row_inserted(GTK_TREE_MODEL(retval), path_new, iter_new);
          destinationRow++;
        }
      }
  );

  QObject::connect(
      model,
      &QAbstractItemModel::rowsAboutToBeRemoved,
      [=](const QModelIndex & parent, int first, int last) {
        // g_debug("rows about to be removed");
        for( int row = first; row <= last; row++) {
          QModelIndex idx = retval->priv->model->index(row, 0, parent); //parent.child(row, 0);
          GtkTreeIter iter;
          iter.stamp = stamp;
          qmodelindex_to_iter(idx, &iter);
          GtkTreePath *path = gtk_q_tree_model_get_path(GTK_TREE_MODEL(retval), &iter);
          gtk_tree_model_row_deleted(GTK_TREE_MODEL(retval), path);
        }
      }
  );

  QObject::connect(
      model,
      &QAbstractItemModel::dataChanged,
      [=](const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> & roles = QVector<int> ()) {
        // g_debug("data changed");
        /* we have to assume only one column */
        int first = topLeft.row();
        int last = bottomRight.row();
        if (topLeft.column() != bottomRight.column() ) {
          g_warning("more than one column is not supported!");
        }
        /* the first idx IS topLeft, the reset are his siblings */
        GtkTreeIter *iter = g_new0(GtkTreeIter, 1);
        QModelIndex idx = topLeft;
        iter->stamp = stamp; //retval->priv->stamp;
        qmodelindex_to_iter(idx, iter);
        GtkTreePath *path = gtk_q_tree_model_get_path(GTK_TREE_MODEL(retval), iter);
        gtk_tree_model_row_changed(GTK_TREE_MODEL(retval), path, iter);
        for( int row = first + 1; row <= last; row++) {
          iter = g_new0(GtkTreeIter, 1);
          idx = topLeft.sibling(row, 0);
          iter->stamp = stamp; //retval->priv->stamp;
          qmodelindex_to_iter(idx, iter);
          path = gtk_q_tree_model_get_path(GTK_TREE_MODEL(retval), iter);
          gtk_tree_model_row_changed(GTK_TREE_MODEL(retval), path, iter);
        }
      }
  );

  return retval;
}

// static void
// gtk_list_store_increment_stamp (GtkQTreeModel *q_tree_model)
// {
//   GtkQTreeModelPrivate *priv = q_tree_model->priv;

//   do
//     {
//       priv->stamp++;
//     }
//   while (priv->stamp == 0);
// }

static gint
gtk_q_tree_model_length (GtkQTreeModel *q_tree_model)
{
  GtkQTreeModelPrivate *priv = q_tree_model->priv;
  // g_debug("q model length: %d", priv->model->rowCount());
  return priv->model->rowCount();
}

static void
gtk_q_tree_model_set_n_columns (GtkQTreeModel *q_tree_model,
            gint          n_columns)
{
  GtkQTreeModelPrivate *priv = q_tree_model->priv;
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
gtk_q_tree_model_set_column_type (GtkQTreeModel *q_tree_model,
        gint          column,
        gint          role,
        GType         type)
{
  GtkQTreeModelPrivate *priv = q_tree_model->priv;

  // if (!_gtk_tree_data_list_check_type (type))
  //   {
  //     g_warning ("%s: Invalid type %s\n", G_STRLOC, g_type_name (type));
  //     return;
  //   }

  priv->column_headers[column] = type;
  priv->column_roles[column] = role;
}


static void
gtk_q_tree_model_finalize (GObject *object)
{
  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (object);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;

  g_free(priv->column_headers);
  g_free(priv->column_roles);

  /* delete the created proxy model */
  delete priv->model;

  G_OBJECT_CLASS (gtk_q_tree_model_parent_class)->finalize (object);
}

static gboolean
iter_is_valid (GtkTreeIter *iter,
               GtkQTreeModel   *q_tree_model)
{
  gboolean retval;
  g_return_val_if_fail(iter != NULL, FALSE);
  QIter *qiter = Q_ITER(iter);
  retval = q_tree_model->priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id).isValid();
  return retval;
}

static void
qmodelindex_to_iter(const QModelIndex &idx, GtkTreeIter *iter)
{
  Q_ITER(iter)->row.value = idx.row();
  Q_ITER(iter)->column.value = idx.row();
  Q_ITER(iter)->id = idx.internalId();
}

static gboolean
validate_index(gint stamp, const QModelIndex &idx, GtkTreeIter *iter)
{
  if (idx.isValid()) {
    iter->stamp = stamp;
    qmodelindex_to_iter(idx, iter);
  } else {
    iter->stamp = 0;
    return FALSE;
  }
  return TRUE;
}

/* Start Fulfill the GtkTreeModel requirements */

/* flags supported by this interface */
static GtkTreeModelFlags
gtk_q_tree_model_get_flags (G_GNUC_UNUSED GtkTreeModel *tree_model)
{
  // TODO: possibly return based on the model?
  return (GtkTreeModelFlags)(GTK_TREE_MODEL_ITERS_PERSIST);
}

/* number of columns supported by this tree model */
static gint
gtk_q_tree_model_get_n_columns (GtkTreeModel *tree_model)
{

  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;

  // g_debug("getting model column number: %d", priv->model->columnCount());
  return priv->model->columnCount();
}

/* get given column type */
static GType
gtk_q_tree_model_get_column_type (GtkTreeModel *tree_model,
				gint          index)
{

  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;

  g_return_val_if_fail (index < gtk_q_tree_model_get_n_columns(tree_model), G_TYPE_INVALID);

  // g_debug("getting column type: %s", g_type_name(priv->column_headers[index]));
  return priv->column_headers[index];
}

/* Sets @iter to a valid iterator pointing to @path.  If @path does
 * not exist, @iter is set to an invalid iterator and %FALSE is returned.
 */
static gboolean
gtk_q_tree_model_get_iter (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter,
			 GtkTreePath  *path)
{
  // g_debug("getting model iter");
  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;

  /* GtkTreePath is a list of indices, each one indicates
   * the child of the previous one.
   * Since GtkTreeModel only has rows (columns are only used to
   * store data in each item), each index is basically the row
   * at the given tree level.
   * To get the iter, we want to get the QModelIndex. To get
   * the QModelIndex we simply start at the first level and
   * traverse the model the number of layers equal to the number
   * of indices in the path.
   */
  gint depth;
  gint* indices = gtk_tree_path_get_indices_with_depth(path, &depth);
  QModelIndex idx = priv->model->index(indices[0], 0);
  for(int layer = 1; layer < depth; layer++ ) {
    /* check if previous iter is valid */
    if (!idx.isValid()) {
      break;
    } else {
      idx = idx.child(indices[layer], 0); //priv->model.index(indices[layer], 0, idx);
    }
  }

  if (!idx.isValid()) {
    iter->stamp = 0;
    return FALSE;
  } else {
    /* we have a valid QModelIndex; turn it into an iter */
    iter->stamp = priv->stamp;
    qmodelindex_to_iter(idx, iter);
  }

  return TRUE;
}

/* Returns a newly-created #GtkTreePath referenced by @iter.
 *
 * This path should be freed with gtk_tree_path_free().
 */
static GtkTreePath *
gtk_q_tree_model_get_path (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  // g_debug("getting model path");
  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;
  GtkTreePath *path;

  g_return_val_if_fail (iter->stamp == priv->stamp, NULL);
  g_return_val_if_fail (iter_is_valid(iter, q_tree_model), NULL);

  /* To get the path, we have to traverse from the child all the way up */
  path = gtk_tree_path_new();
  QIter *qiter = Q_ITER(iter);
  QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
  while( idx.isValid() ){
    gtk_tree_path_prepend_index(path, idx.row());
    idx = idx.parent();
  }
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
gtk_q_tree_model_get_value (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  // g_debug("getting model iter value");

  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;

  g_return_if_fail (column < priv->n_columns);
  g_return_if_fail (iter_is_valid(iter, q_tree_model));

  /* get the data */
  QIter *qiter = Q_ITER(iter);
  QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
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
gtk_q_tree_model_iter_next (GtkTreeModel  *tree_model,
			  GtkTreeIter   *iter)
{
  // g_debug("getting iter next");
  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;

  g_return_val_if_fail (iter_is_valid(iter, q_tree_model), FALSE);

  QIter *qiter = Q_ITER(iter);
  QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
  idx = idx.sibling(idx.row()+1, idx.column());

  /* validate */
  if (validate_index(priv->stamp, idx, iter) ) {
    GtkTreePath *path = gtk_q_tree_model_get_path(tree_model, iter);
    // g_debug("next iter path: %s", gtk_tree_path_to_string(path));
    gtk_tree_path_free(path);
    return TRUE;
  } else {
    // g_debug("next iter is invalid");
    return FALSE;
  }
}

/* Sets @iter to point to the previous node at the current level.
 *
 * If there is no previous @iter, %FALSE is returned and @iter is
 * set to be invalid.
 */
static gboolean
gtk_q_tree_model_iter_previous (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter)
{
  // g_debug("getting prev iter");
  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;

  g_return_val_if_fail (iter_is_valid(iter, q_tree_model), FALSE);

  QIter *qiter = Q_ITER(iter);
  QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
  idx = idx.sibling(idx.row()-1, idx.column());

  /* validate */
  return validate_index(priv->stamp, idx, iter);
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
gtk_q_tree_model_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
  // g_debug("getting model children");
  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;
  QModelIndex idx;

  /* make sure parent if valid node if its not NULL */
  if (parent)
    g_return_val_if_fail(iter_is_valid(parent, q_tree_model), FALSE);

  if (parent) {
    /* get first child */
    QIter *qparent = Q_ITER(parent);
    idx = priv->model->indexFromId(qparent->row.value, qparent->column.value, qparent->id);
    idx = idx.child(0, 0);
  } else {
    /* parent is NULL, get the first node */
    idx = priv->model->index(0, 0);
  }

  /* validate child */
  return validate_index(priv->stamp, idx, iter);
}

/* Returns %TRUE if @iter has children, %FALSE otherwise. */
static gboolean
gtk_q_tree_model_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  // g_debug("checking if iter has children");
  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;
  g_return_val_if_fail(iter_is_valid(iter, q_tree_model), FALSE);

  QIter *qiter = Q_ITER(iter);
  QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
  return priv->model->hasChildren(idx);
}

/* Returns the number of children that @iter has.
 *
 * As a special case, if @iter is %NULL, then the number
 * of toplevel nodes is returned.
 */
static gint
gtk_q_tree_model_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  // g_debug("getting number of children of iter");
  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;

  if (iter == NULL)
    return gtk_q_tree_model_length(q_tree_model);

  g_return_val_if_fail(iter_is_valid(iter, q_tree_model), -1);
  QIter *qiter = Q_ITER(iter);
  QModelIndex idx = priv->model->indexFromId(qiter->row.value, qiter->column.value, qiter->id);
  return priv->model->rowCount(idx);
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
gtk_q_tree_model_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
  // g_debug("getting nth child of itter");
  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;
  QModelIndex idx;

  if (parent) {
    g_return_val_if_fail(iter_is_valid(parent, q_tree_model), FALSE);

    /* get the nth child */
    QIter *qparent = Q_ITER(parent);
    idx = priv->model->indexFromId(qparent->row.value, qparent->column.value, qparent->id);
    idx = idx.child(n, 0);
  } else {
    idx = priv->model->index(n, 0);
  }

  /* validate */
  return validate_index(priv->stamp, idx, iter);
}

/* Sets @iter to be the parent of @child.
 *
 * If @child is at the toplevel, and doesn't have a parent, then
 * @iter is set to an invalid iterator and %FALSE is returned.
 * @child will remain a valid node after this function has been
 * called.
 */
static gboolean
gtk_q_tree_model_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  // g_debug("getting parent of iter of child iter");
  GtkQTreeModel *q_tree_model = GTK_Q_TREE_MODEL (tree_model);
  GtkQTreeModelPrivate *priv = q_tree_model->priv;
  QModelIndex idx;

  g_return_val_if_fail(iter_is_valid(child, q_tree_model), FALSE);

  QIter *qchild = Q_ITER(child);
  idx = priv->model->indexFromId(qchild->row.value, qchild->column.value, qchild->id);
  idx = idx.parent();

  /* validate */
  return validate_index(priv->stamp, idx, iter);
}

/* End Fulfill the GtkTreeModel requirements */