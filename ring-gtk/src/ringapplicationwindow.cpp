#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "ringapplicationwindow.h"

#include "gtkqtreemodel.h"
#include "lib/callmodel.h"
#include "lib/accountlistmodel.h"
#include "lib/historymodel.h"
#include "gtkaccessproxymodel.h"

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
  GtkWidget *gears;
  /* TODO: remove after doing this with GAction */
  GtkWidget *toolbutton_pickup;
  GtkWidget *toolbutton_hangup;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingApplicationWindow, ring_application_window, GTK_TYPE_APPLICATION_WINDOW);

static Call *
get_call_from_selection(GtkTreeSelection *selection)
{
  GtkTreeIter iter;
  GtkTreeModel *model = NULL;
  Call *call = NULL;

  if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
    /* get the call */
    QIter qiter = *Q_ITER(&iter);
    GtkAccessProxyModel *proxy_model = gtk_q_tree_model_get_qmodel(GTK_Q_TREE_MODEL(model));
    QModelIndex proxy_idx = proxy_model->indexFromId(qiter.row.value, qiter.column.value, qiter.id);
    if (proxy_idx.isValid()) {
      /* we have the proxy model idx, now get the actual idx so we can get the call object */
      QModelIndex idx = proxy_model->mapToSource(proxy_idx);
      /* assume its valid and get the call from the CallModel instance */
      call = CallModel::instance()->getCall(idx);
    } else {
      g_debug("selected model index is not valid");
    }
  } else {
    /* none selected */
    g_debug("no selection");
  }
  return call;
}
static void
pickup_clicked(GtkToolButton *toolbutton,
               gpointer       user_data)
{
  RingApplicationWindow* win = RING_APPLICATION_WINDOW(user_data);
  RingApplicationWindowPrivate *priv;
  priv = (RingApplicationWindowPrivate*)ring_application_window_get_instance_private(win);

  g_debug("pickup call clicked");

  GtkTreeSelection* selection = NULL;
  GtkTreeIter iter;
  GtkTreeModel *model = NULL;

  /* get selected account,
   * if none selected, get the first one */

  /* get the selected call,
   * if none selected, create a new one with the selected account */
  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_calls));
  Call *call = get_call_from_selection(selection);
  if (call) {
    call->performAction(Call::Action::ACCEPT);
  } else {
    g_debug("could not get call from selection");
  }
}

static void
hangup_clicked(GtkToolButton *toolbutton,
               gpointer       user_data)
{
  RingApplicationWindow* win = RING_APPLICATION_WINDOW(user_data);
  RingApplicationWindowPrivate *priv;
  priv = (RingApplicationWindowPrivate*)ring_application_window_get_instance_private(win);

  g_debug("hangup call clicked");

  GtkTreeSelection* selection = NULL;
  GtkTreeIter iter;
  GtkTreeModel *model = NULL;

  /* get the selected call,
   * if none selected, do nothing */
  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_calls));
  Call *call = get_call_from_selection(selection);
  if (call) {
    call->performAction(Call::Action::REFUSE);
  } else {
    g_debug("could not get call from selection");
  }
}

static void
ring_application_window_init (RingApplicationWindow *win)
{
  RingApplicationWindowPrivate *priv;

  priv = (RingApplicationWindowPrivate*)ring_application_window_get_instance_private (win);
  gtk_widget_init_template (GTK_WIDGET (win));

  // gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (win), TRUE);

  /* gears menu */
  GtkBuilder *builder = gtk_builder_new_from_resource("/org/sfl/ring/ringgearsmenu.ui");
  GMenuModel *menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
  gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(priv->gears), menu);
  g_object_unref(builder);

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

  /* connect signals */
  g_signal_connect(priv->toolbutton_pickup, "clicked", G_CALLBACK(pickup_clicked), win);
  g_signal_connect(priv->toolbutton_hangup, "clicked", G_CALLBACK(hangup_clicked), win);
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
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), RingApplicationWindow, gears);

  /* TODO: remove after doing this with GAction */
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), RingApplicationWindow, toolbutton_pickup);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), RingApplicationWindow, toolbutton_hangup);

  /* bind handlers from template */
  // gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), search_text_changed);
  // gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), visible_child_changed);
}

GtkWidget *
ring_application_window_new (GtkApplication *app)
{
  return (GtkWidget *)g_object_new (RING_APPLICATION_WINDOW_TYPE, "application", app, NULL);
}
