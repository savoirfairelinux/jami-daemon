
#include <gtk/gtk.h>
#include "addressbook.h"

/**
 * Callback called after all book have been processed
 */

static void
add_contact(const gchar *name, const char *phone, GdkPixbuf *photo)
{
}

static void
handler_async_search(GList *hits, gpointer user_data)
{
    AddressBook_Config *addressbook_config = user_data;

    for (GList *i = hits; i != NULL; i = i->next) {
        GdkPixbuf *photo = NULL;
        Hit *entry = i->data;

        if (!entry)
            continue;

        add_contact(entry->name, entry->phone_home, photo);

        g_free(entry->name);
        g_free(entry->phone_home);
        g_free(entry);
    }

    g_list_free(hits);
}

int
main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show(window);
    addressbook_init();

    gtk_main();

    return 0;
}
