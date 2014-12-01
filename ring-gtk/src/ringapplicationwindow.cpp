#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "ringapplicationwindow.h"

#include "gtkqtreemodel.h"
#include "lib/callmodel.h"
#include "lib/accountlistmodel.h"
#include "lib/historymodel.h"

struct _RingApplicationWindow
{
  GtkApplicationWindow parent;
};

struct _RingApplicationWindowClass
{
  GtkApplicationWindowClass parent_class;
};

typedef struct _RingApplicationWindowPrivate RingApplicationWindowPrivate;

struct _RingApplicationWindowPrivate
{
  // GSettings *settings;
  GtkWidget *treeview_history;
  GtkWidget *treeview_calls;
  GtkWidget *treeview_accounts;
  GtkWidget *entry_call;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingApplicationWindow, ring_application_window, GTK_TYPE_APPLICATION_WINDOW);

static void
ring_application_window_init (RingApplicationWindow *win)
{
  RingApplicationWindowPrivate *priv;

  priv = (RingApplicationWindowPrivate*)ring_application_window_get_instance_private (win);
  gtk_widget_init_template (GTK_WIDGET (win));

  // gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (win), TRUE);

  /* create the tree models */
  GtkQTreeModel *model;

  // account model
  model = gtk_q_tree_model_new(AccountListModel::instance(), 3,
      Account::Role::Alias, G_TYPE_STRING,
      Account::Role::Id, G_TYPE_STRING,
      Account::Role::Enabled, G_TYPE_BOOLEAN);

  // put in treeview
  gtk_tree_view_set_model( GTK_TREE_VIEW(priv->treeview_accounts), GTK_TREE_MODEL(model) );

  GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes ("Alias", renderer, "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_accounts), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("ID", renderer, "text", 1, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_accounts), column);

  renderer = gtk_cell_renderer_toggle_new ();
  column = gtk_tree_view_column_new_with_attributes ("Enabled", renderer, "active", 2, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_accounts), column);

  // call model
  model = gtk_q_tree_model_new(CallModel::instance(), 4,
      Call::Role::Name, G_TYPE_STRING,
      Call::Role::Number, G_TYPE_STRING,
      Call::Role::Length, G_TYPE_STRING,
      Call::Role::CallState, G_TYPE_STRING);
  gtk_tree_view_set_model( GTK_TREE_VIEW(priv->treeview_calls), GTK_TREE_MODEL(model) );

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Name", renderer, "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_calls), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Number", renderer, "text", 1, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_calls), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Duration", renderer, "text", 2, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_calls), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("State", renderer, "text", 3, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_calls), column);

  // history model
  model = gtk_q_tree_model_new(HistoryModel::instance(), 4,
      Call::Role::Name, G_TYPE_STRING,
      Call::Role::Number, G_TYPE_STRING,
      Call::Role::Date, G_TYPE_STRING,
      Call::Role::Direction2, G_TYPE_INT);
  gtk_tree_view_set_model( GTK_TREE_VIEW(priv->treeview_history), GTK_TREE_MODEL(model) );

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Name", renderer, "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_history), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Number", renderer, "text", 1, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_history), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Date", renderer, "text", 2, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_history), column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Direction", renderer, "text", 3, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview_history), column);
}

static void
ring_application_window_dispose (GObject *object)
{
  RingApplicationWindow *win;
  RingApplicationWindowPrivate *priv;

  win = RING_APPLICATION_WINDOW (object);
  priv = (RingApplicationWindowPrivate *)ring_application_window_get_instance_private (win);

  // g_clear_object (&priv->settings);

  G_OBJECT_CLASS (ring_application_window_parent_class)->dispose (object);
}

static void
ring_application_window_class_init (RingApplicationWindowClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = ring_application_window_dispose;

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                               "/org/sfl/ring/ringapplicationwindow.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), RingApplicationWindow, treeview_history);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), RingApplicationWindow, treeview_calls);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), RingApplicationWindow, treeview_accounts);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), RingApplicationWindow, entry_call);

  /* bind handlers from template */
  // gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), search_text_changed);
  // gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), visible_child_changed);
}

GtkWidget *
ring_application_window_new (GtkApplication *app)
{
  return (GtkWidget *)g_object_new (RING_APPLICATION_WINDOW_TYPE, "application", app, NULL);
}
