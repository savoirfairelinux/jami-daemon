
#include <gtk/gtk.h>
#include "addressbook.h"

/**
 * Callback called after all book have been processed
 */

enum {
  COLUMN_NAME,
  COLUMN_PHONE,
  COLUMN_PIXBUF,
  N_COLUMNS
};

// Evil!
static GtkListStore *list_store;


static void
add_contact(const gchar *name, const char *phone, GdkPixbuf *photo)
{
    if (g_strcmp0(phone, "") == 0)
        return;

    g_print("name: %s, phone: %s, photo: %p\n", name, phone, photo);
    GtkTreeIter iter;
    // Add a new row to the model
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter,
                       COLUMN_NAME, name,
                       COLUMN_PHONE, phone,
                       COLUMN_PIXBUF, photo,
                       -1);
}

static void
handler_async_search(GList *hits, G_GNUC_UNUSED gpointer data)
{
    for (GList *i = hits; i != NULL; i = i->next) {
        Hit *hit = i->data;

        if (!hit)
            continue;

        const gchar *phone = hit->phone_business ? hit->phone_business :
                             hit->phone_mobile ? hit->phone_mobile :
                             hit->phone_home ? hit->phone_home : "";
        add_contact(hit->name, phone, hit->photo);

        g_free(hit->name);
        g_free(hit->phone_home);
        g_free(hit->phone_mobile);
        g_free(hit->phone_business);
        g_free(hit);
    }

    g_list_free(hits);
}

static void
text_changed_cb(GtkEntry *entry)
{
    g_print("Text changed to %s\n", gtk_entry_get_text(entry));
    gtk_list_store_clear(list_store);
    addressbook_search(handler_async_search, entry, NULL);
}

int
main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    list_store = gtk_list_store_new(N_COLUMNS,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    GDK_TYPE_PIXBUF);
    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));

    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    GtkCellRenderer *pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
    GtkCellRenderer *renderers[] = {text_renderer, text_renderer, pixbuf_renderer};
    const char *column_header[]= {"Name", "Phone", "Photo"};
    const char *column_type[]= {"text", "text", "pixbuf"};

    for (gint col = 0; col < N_COLUMNS; ++col) {
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(column_header[col],
                                                                             renderers[col], column_type[col],
                                                                             col,
                                                                             NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    }

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    addressbook_init();
    addressbook_set_current_book("Contacts");

    GtkWidget *entry = gtk_entry_new();
    gtk_container_add(GTK_CONTAINER(vbox), tree_view);
    gtk_container_add(GTK_CONTAINER(vbox), entry);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    addressbook_search(handler_async_search, GTK_ENTRY(entry), NULL);
    g_signal_connect(entry, "notify::text", G_CALLBACK(text_changed_cb), NULL);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}
