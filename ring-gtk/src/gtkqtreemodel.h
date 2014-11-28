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

#ifndef __GTK_Q_MODEL_H__
#define __GTK_Q_MODEL_H__

#include <gtk/gtk.h>
#include <QtCore/QAbstractItemModel>

G_BEGIN_DECLS

#define GTK_TYPE_Q_MODEL	       (gtk_q_model_get_type ())
#define GTK_Q_MODEL(obj)	       (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_Q_MODEL, GtkQModel))
#define GTK_Q_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_Q_MODEL, GtkQModelClass))
#define GTK_IS_Q_MODEL(obj)	       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_Q_MODEL))
#define GTK_IS_Q_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_Q_MODEL))
#define GTK_Q_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_Q_MODEL, GtkQModelClass))

typedef struct _GtkQModel              GtkQModel;
typedef struct _GtkQModelPrivate       GtkQModelPrivate;
typedef struct _GtkQModelClass         GtkQModelClass;

struct _GtkQModel
{
  GObject parent;

  /*< private >*/
  GtkQModelPrivate *priv;
};

struct _GtkQModelClass
{
  GObjectClass parent_class;
};


GType         gtk_q_model_get_type         (void) G_GNUC_CONST;
GtkQModel *gtk_q_model_new              (QAbstractItemModel *, gint, ...);

G_END_DECLS


#endif /* __GTK_Q_MODEL_H__ */
