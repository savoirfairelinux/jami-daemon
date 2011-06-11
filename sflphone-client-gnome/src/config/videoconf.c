/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */


#include <string.h>
#include "videoconf.h"
#include "utils.h"
#include "eel-gconf-extensions.h"
#include "dbus.h"
#include "video/video_preview.h"

static GtkWidget *codecTreeView;		// View used instead of store to get access to selection
static GtkWidget *codecMoveUpButton;
static GtkWidget *codecMoveDownButton;

// Codec properties ID
enum {
    COLUMN_CODEC_ACTIVE,
    COLUMN_CODEC_NAME,
    COLUMN_CODEC_BITRATE,
    COLUMN_CODEC_BANDWIDTH,
    CODEC_COLUMN_COUNT
};

/**
 * Toggle move buttons on if a codec is selected, off elsewise
 */
static void
select_codec (GtkTreeSelection *selection, GtkTreeModel *model)
{
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
        gtk_widget_set_sensitive (GTK_WIDGET (codecMoveUpButton), FALSE);
        gtk_widget_set_sensitive (GTK_WIDGET (codecMoveDownButton), FALSE);
    } else {
        gtk_widget_set_sensitive (GTK_WIDGET (codecMoveUpButton), TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (codecMoveDownButton), TRUE);
    }
}
    
void active_is_always_recording ()
{
    gboolean enabled = FALSE;

    enabled = dbus_get_is_always_recording();

    if(enabled) {
        enabled = FALSE;
    }
    else {
        enabled = TRUE;
    }

    dbus_set_is_always_recording(enabled);
}

/* This gets called when the video preview window is closed */
static gboolean
preview_is_running_cb(GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    (void) pspec;
    gboolean running = FALSE;
    g_object_get(obj, "running", &running, NULL);
    if (!running) {
        GtkButton *button = GTK_BUTTON(user_data);
        gtk_button_set_label(button, "_Start preview");
    }
    return TRUE;
}

void preview_button_clicked(GtkButton *button, gpointer data UNUSED)
{
    static VideoPreview *preview = NULL;
    if (g_strcmp0(gtk_button_get_label(button), "_Start preview")  == 0) {
        gtk_button_set_label(button, "_Stop preview");
        if (preview == NULL) {
            preview = video_preview_new();
            g_signal_connect (preview, "notify::running",
                              G_CALLBACK (preview_is_running_cb),
                              button);
        }

        video_preview_run(preview);
    }
    else {
        /* user clicked stop */
        gtk_button_set_label(button, "_Start preview");
        if (preview) {
            video_preview_stop(preview);
            g_object_unref(preview);
            preview = NULL;
        }
    }
}


static void record_path_changed (GtkFileChooser *chooser , 
                                 GtkLabel *label UNUSED)
{
    DEBUG ("record_path_changed");

    gchar* path;
    path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
    DEBUG ("path2 %s", path);
    dbus_set_record_path (path);
}

GtkWidget* create_video_configuration()
{
    // Main widget
    GtkWidget *ret;
    // Sub boxes
    GtkWidget *frame;

    ret = gtk_vbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (ret), 10);

    GtkWidget *table;

    gnome_main_section_new_with_table (_ ("Video Manager"), &frame, &table, 1, 5);
    gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

    //int video_manager = dbus_get_video_manager();

    gnome_main_section_new_with_table (_ ("Preview"), &frame, &table, 1, 2);
    gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

    GtkWidget *previewButton = gtk_button_new_with_mnemonic(_("_Start preview"));
    gtk_table_attach(GTK_TABLE(table), previewButton, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 6);
    g_signal_connect(G_OBJECT(previewButton), "clicked", G_CALLBACK(preview_button_clicked), NULL);
    gtk_widget_show(GTK_WIDGET(previewButton));

    gtk_widget_show_all (ret);

    return ret;
}

/**
 * Fills the tree list with supported codecs
 */
static void preferences_dialog_fill_codec_list (account_t **a UNUSED)
{
    GtkListStore *codecStore;
    gchar **video_codecs = NULL, **specs = NULL;
    GtkTreeIter iter;
    glong payload;

    // Get model of view and clear it
    codecStore = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (codecTreeView)));
    gtk_list_store_clear (codecStore);

    // This is a global list inherited by all accounts
    video_codecs = (gchar**) dbus_video_codec_list ();
    if (video_codecs != NULL) {
        // Add the codecs in the list
        for (; *video_codecs; video_codecs++) {
            payload = atol (*video_codecs);
            specs = (gchar **) dbus_video_codec_details (payload);
            DEBUG("%s\n", *video_codecs);
            DEBUG("%s\n", specs[0]);
            DEBUG("%d\n", payload);

            // Insert codecs
            gtk_list_store_append (codecStore, &iter);
            gtk_list_store_set (codecStore, &iter,
                    COLUMN_CODEC_ACTIVE,	TRUE,									// Active
                    COLUMN_CODEC_NAME,		specs[0],								// Name
                    COLUMN_CODEC_BITRATE,	g_strdup_printf ("%.1f kbps", 1000.0),	// Bitrate (kbps)
                    COLUMN_CODEC_BANDWIDTH,	g_strdup_printf ("%.1f kbps", 1000.0),	// Bandwidth (kpbs)
                    -1);
        }
    }

}


/**
 * Toggle active value of codec on click and update changes to the deamon
 * and in configuration files
 */
static void
codec_active_toggled (GtkCellRendererToggle *renderer UNUSED, gchar *path UNUSED, gpointer data UNUSED)
{
#if 0
    GtkTreeIter iter;
    GtkTreePath *treePath;
    GtkTreeModel *model;
    gboolean active;
    char* name;
    codec_t* codec;
    account_t *acc;

    // Get path of clicked codec active toggle box
    treePath = gtk_tree_path_new_from_string (path);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (codecTreeView));
    gtk_tree_model_get_iter (model, &iter, treePath);

    // Retrieve userdata
    acc = (account_t*) data;

    if (!acc)
        ERROR ("Aie, no account selected");

    // Get active value and name at iteration
    gtk_tree_model_get (model, &iter,
            COLUMN_CODEC_ACTIVE, &active,
            COLUMN_CODEC_NAME, &name,
            -1);

    printf ("%s\n", name);
    printf ("%i\n", g_queue_get_length (acc->codecs));

    codec = codec_list_get_by_name ( (gconstpointer) name, acc->codecs);

    // Toggle active value
    active = !active;

    // Store value
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
            COLUMN_CODEC_ACTIVE, active,
            -1);

    gtk_tree_path_free (treePath);

    // Modify codec queue to represent change
    if (active) {
        codec_set_active (&codec);
    } else {
        codec_set_inactive (&codec);
    }
#endif
}

/**
 * Move codec in list depending on direction and selected codec and
 * update changes in the daemon list and the configuration files
 */
static void codec_move (gboolean moveUp, gpointer data UNUSED)
{

    GtkTreeIter iter;
    GtkTreeIter *iter2;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreePath *treePath;
    gchar *path;
    //account_t *acc;
    //GQueue *acc_q;

    // Get view, model and selection of codec store
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (codecTreeView));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (codecTreeView));

    // Retrieve the user data
    //acc = (account_t*) data;

    //if (acc)
    //   acc_q = acc->codecs;

    // Find selected iteration and create a copy
    gtk_tree_selection_get_selected (GTK_TREE_SELECTION (selection), &model, &iter);
    iter2 = gtk_tree_iter_copy (&iter);

    // Find path of iteration
    path = gtk_tree_model_get_string_from_iter (GTK_TREE_MODEL (model), &iter);
    treePath = gtk_tree_path_new_from_string (path);
    //gint *indices = gtk_tree_path_get_indices (treePath);
    //gint indice = indices[0];

    // Depending on button direction get new path
    if (moveUp)
        gtk_tree_path_prev (treePath);
    else
        gtk_tree_path_next (treePath);

    gtk_tree_model_get_iter (model, &iter, treePath);

    // Swap iterations if valid
    if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (model), &iter))
        gtk_list_store_swap (GTK_LIST_STORE (model), &iter, iter2);

    // Scroll to new position
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (codecTreeView), treePath, NULL, FALSE, 0, 0);

    // Free resources
    gtk_tree_path_free (treePath);
    gtk_tree_iter_free (iter2);
    g_free (path);

#if 0
    // Perpetuate changes in codec queue
    if (moveUp)
        codec_list_move_codec_up (indice, &acc_q);
    else
        codec_list_move_codec_down (indice, &acc_q);
#endif
}

/**
 * Called from move up codec button signal
 */
static void codec_move_up (GtkButton *button UNUSED, gpointer data)
{
    // Change tree view ordering and get indice changed
    codec_move (TRUE, data);
}

/**
 * Called from move down codec button signal
 */
static void codec_move_down (GtkButton *button UNUSED, gpointer data)
{
    // Change tree view ordering and get indice changed
    codec_move (FALSE, data);
}

GtkWidget* videocodecs_box (account_t **a)
{
    GtkWidget *ret;
    GtkWidget *scrolledWindow;
    GtkWidget *buttonBox;

    GtkListStore *codecStore;
    GtkCellRenderer *renderer;
    GtkTreeSelection *treeSelection;
    GtkTreeViewColumn *treeViewColumn;

    ret = gtk_hbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (ret), 10);

    scrolledWindow = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledWindow), GTK_SHADOW_IN);

    gtk_box_pack_start (GTK_BOX (ret), scrolledWindow, TRUE, TRUE, 0);
    codecStore = gtk_list_store_new (CODEC_COLUMN_COUNT,
            G_TYPE_BOOLEAN,	// Active
            G_TYPE_STRING,		// Name
            G_TYPE_STRING,		// Bit rate
            G_TYPE_STRING		// Bandwith
            );

    // Create codec tree view with list store
    codecTreeView = gtk_tree_view_new_with_model (GTK_TREE_MODEL (codecStore));

    // Get tree selection manager
    treeSelection = gtk_tree_view_get_selection (GTK_TREE_VIEW (codecTreeView));
    g_signal_connect (G_OBJECT (treeSelection), "changed",
            G_CALLBACK (select_codec),
            codecStore);

    // Active column
    renderer = gtk_cell_renderer_toggle_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes ("", renderer, "active", COLUMN_CODEC_ACTIVE, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (codecTreeView), treeViewColumn);

    // Toggle codec active property on clicked
    g_signal_connect (G_OBJECT (renderer), "toggled", G_CALLBACK (codec_active_toggled), (gpointer) *a);

    // Name column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes (_ ("Name"), renderer, "markup", COLUMN_CODEC_NAME, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (codecTreeView), treeViewColumn);

    // Bitrate column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes (_ ("Bitrate"), renderer, "text", COLUMN_CODEC_BITRATE, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (codecTreeView), treeViewColumn);

    // Bandwidth column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes (_ ("Bandwidth"), renderer, "text", COLUMN_CODEC_BANDWIDTH, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (codecTreeView), treeViewColumn);

    g_object_unref (G_OBJECT (codecStore));
    gtk_container_add (GTK_CONTAINER (scrolledWindow), codecTreeView);

    // Create button box
    buttonBox = gtk_vbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (buttonBox), 10);
    gtk_box_pack_start (GTK_BOX (ret), buttonBox, FALSE, FALSE, 0);

    codecMoveUpButton = gtk_button_new_from_stock (GTK_STOCK_GO_UP);
    gtk_widget_set_sensitive (GTK_WIDGET (codecMoveUpButton), FALSE);
    gtk_box_pack_start (GTK_BOX (buttonBox), codecMoveUpButton, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (codecMoveUpButton), "clicked", G_CALLBACK (codec_move_up), *a);

    codecMoveDownButton = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
    gtk_widget_set_sensitive (GTK_WIDGET (codecMoveDownButton), FALSE);
    gtk_box_pack_start (GTK_BOX (buttonBox), codecMoveDownButton, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (codecMoveDownButton), "clicked", G_CALLBACK (codec_move_down), *a);

    preferences_dialog_fill_codec_list (a);

    return ret;
}
