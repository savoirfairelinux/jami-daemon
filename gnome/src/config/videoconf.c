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
#include "video/video_renderer.h"
#include "actions.h"
#include "codeclist.h"

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

static GtkWidget *v4l2Device;
static GtkWidget *v4l2Channel;
static GtkWidget *v4l2Size;
static GtkWidget *v4l2Rate;

static GtkListStore *v4l2DeviceList;
static GtkListStore *v4l2ChannelList;
static GtkListStore *v4l2SizeList;
static GtkListStore *v4l2RateList;

static GtkWidget *v4l2_hbox;
static GtkWidget *v4l2_nodev;

static GtkWidget *receivingVideoWindow;
static GtkWidget *receivingVideoArea;
static VideoRenderer *video_renderer = NULL;

static GtkWidget *preview_button = NULL;

static GtkWidget *drawarea = NULL;
static int using_clutter;
static int drawWidth  = 352; // FIXME: should come from dbus signals
static int drawHeight = 288;
static const char *drawFormat;
static VideoRenderer *preview = NULL;

static GtkWidget *codecTreeView;		// View used instead of store to get access to selection
static GtkWidget *codecMoveUpButton;
static GtkWidget *codecMoveDownButton;

// Codec properties ID
enum {
    COLUMN_CODEC_ACTIVE,
    COLUMN_CODEC_NAME,
    COLUMN_CODEC_BITRATE,
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

/* This gets called when the video preview is stopped */
static gboolean
preview_is_running_cb(GObject *obj, GParamSpec *pspec, gpointer user_data)
{
    (void) pspec;
    gboolean running = FALSE;
    g_object_get(obj, "running", &running, NULL);
    GtkButton *button = GTK_BUTTON(user_data);
    if (running) {
        gtk_button_set_label(button, _("_Stop"));
    } else {
        gtk_button_set_label(button, _("_Start"));
        preview = NULL;
    }
    return TRUE;
}

void
video_started_cb(DBusGProxy *proxy, gint OUT_shmId, gint OUT_semId, gint OUT_videoBufferSize, GError *error, gpointer userdata)
{
    (void)proxy;
    (void)error;
    (void)userdata;

    if (OUT_shmId == -1 || OUT_semId == -1 || OUT_videoBufferSize == -1) {
        return;
    }

    DEBUG("Preview started shm:%d sem:%d size:%d", OUT_shmId, OUT_semId, OUT_videoBufferSize);
    preview = video_renderer_new(drawarea, drawWidth, drawHeight, drawFormat, OUT_shmId, OUT_semId, OUT_videoBufferSize);
    g_signal_connect (preview, "notify::running", G_CALLBACK (preview_is_running_cb), preview_button);
    if (video_renderer_run(preview)) {
        ERROR("Video preview run returned an error, unreffing\n");
        g_object_unref(preview);
    }
}

static void
preview_button_clicked(GtkButton *button, gpointer data UNUSED)
{
    preview_button = GTK_WIDGET(button);
    if (g_strcmp0(gtk_button_get_label(button), _("_Start")) == 0) {

        static const char *formats[2] = { "rgb24", "bgra" };

        drawFormat = using_clutter ? formats[0] : formats[1];
        dbus_start_video_preview(drawWidth, drawHeight, drawFormat);
    }
    else { /* user clicked stop */
        if (!preview) /* preview was not created yet on the server */
            return ;
        video_renderer_stop(preview);
	dbus_stop_video_preview();
        preview = NULL;
    }
}

/**
 * Fills the tree list with supported codecs
 */
static void preferences_dialog_fill_codec_list (account_t *a)
{
    GtkListStore *codecStore;
    GtkTreeIter iter;

    // Get model of view and clear it
    codecStore = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (codecTreeView)));
    gtk_list_store_clear (codecStore);

    GQueue *list = a ? a->vcodecs : get_video_codecs_list ();

    // Add the codecs in the list
    unsigned i;
    for (i = 0 ; i < list->length; i++) {
      codec_t *c = g_queue_peek_nth (list, i);

      if (c) {
          printf("%s", c->name);
          gtk_list_store_append (codecStore, &iter);
          gchar *bitrate = g_strdup_printf ("%s kbps", c->bitrate);

          gtk_list_store_set (codecStore, &iter,
                              COLUMN_CODEC_ACTIVE,    c->is_active,
                              COLUMN_CODEC_NAME,      c->name,
                              COLUMN_CODEC_BITRATE,   bitrate,
                              -1);
          g_free(bitrate);
      }
    }
}


/**
 * Toggle active value of codec on click and update changes to the deamon
 * and in configuration files
 */
    static void
codec_active_toggled (GtkCellRendererToggle *renderer UNUSED, gchar *path, gpointer data)
{
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
    printf ("%i\n", g_queue_get_length (acc->vcodecs));

    codec = codec_list_get_by_name ( (gconstpointer) name, acc->vcodecs);

    // Toggle active value
    active = !active;

    // Store value
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
            COLUMN_CODEC_ACTIVE, active,
            -1);

    gtk_tree_path_free (treePath);

    // Modify codec queue to represent change
    codec->is_active = active;
}

/**
 * Move codec in list depending on direction and selected codec and
 * update changes in the daemon list and the configuration files
 */
static void codec_move (gboolean moveUp, gpointer data)
{

    GtkTreeIter iter;
    GtkTreeIter *iter2;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreePath *treePath;
    gchar *path;

    // Get view, model and selection of codec store
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (codecTreeView));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (codecTreeView));

    // Find selected iteration and create a copy
    gtk_tree_selection_get_selected (GTK_TREE_SELECTION (selection), &model, &iter);
    iter2 = gtk_tree_iter_copy (&iter);

    // Find path of iteration
    path = gtk_tree_model_get_string_from_iter (GTK_TREE_MODEL (model), &iter);
    treePath = gtk_tree_path_new_from_string (path);
    gint *indices = gtk_tree_path_get_indices (treePath);
    gint indice = indices[0];

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

    // Perpetuate changes in codec queue
    codec_list_move (indice, ((account_t*)data)->vcodecs, moveUp);
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

GtkWidget* videocodecs_box (account_t *a)
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
    g_signal_connect (G_OBJECT (renderer), "toggled", G_CALLBACK (codec_active_toggled), (gpointer) a);

    // Name column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes (_ ("Name"), renderer, "markup", COLUMN_CODEC_NAME, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (codecTreeView), treeViewColumn);

    // Bitrate column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes (_ ("Bitrate"), renderer, "text", COLUMN_CODEC_BITRATE, NULL);
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
    g_signal_connect (G_OBJECT (codecMoveUpButton), "clicked", G_CALLBACK (codec_move_up), a);

    codecMoveDownButton = gtk_button_new_from_stock (GTK_STOCK_GO_DOWN);
    gtk_widget_set_sensitive (GTK_WIDGET (codecMoveDownButton), FALSE);
    gtk_box_pack_start (GTK_BOX (buttonBox), codecMoveDownButton, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (codecMoveDownButton), "clicked", G_CALLBACK (codec_move_down), a);

    preferences_dialog_fill_codec_list (a);

    return ret;
}

static char *get_active_text(GtkComboBox *box)
{
    char *text = NULL;
    int comboBoxIndex = gtk_combo_box_get_active (box);
    if (comboBoxIndex >= 0) {
        GtkTreeIter iter;
        gtk_combo_box_get_active_iter (box, &iter);
        gtk_tree_model_get (gtk_combo_box_get_model (box), &iter, 0, &text, -1);
    }
    return text;
}

/* Return 0 if string was found in the combo box, != 0 if the string was not found */
static int set_combo_index_from_str(GtkComboBox *box, const char *str, size_t max)
{
    g_assert(str);

    GtkTreeModel *model = gtk_combo_box_get_model (box);
    GtkTreeIter iter;
    unsigned idx = 0;
    gtk_tree_model_get_iter_first(model, &iter);
    do {
        char *boxstr;
        gtk_tree_model_get(model, &iter, 0, &boxstr, -1);
        if (boxstr && !strcmp(boxstr, str))
            break;
    } while (idx++ < max && gtk_tree_model_iter_next(model, &iter));

    if (idx >= max)
        return 1;

    gtk_combo_box_set_active(box, idx);
    return 0;
}


/**
 * Fill video input device rate store
 */
static void
preferences_dialog_fill_video_input_device_rate_list()
{

    GtkTreeIter iter;
    gchar** list = NULL;

    gtk_list_store_clear (v4l2RateList);

    gchar *dev  = get_active_text(GTK_COMBO_BOX(v4l2Device));
    gchar *chan = get_active_text(GTK_COMBO_BOX(v4l2Channel));
    gchar *size = get_active_text(GTK_COMBO_BOX(v4l2Size));

    // Call dbus to retreive list
    if (dev && chan && size)
      list = dbus_get_video_input_device_rate_list(dev, chan, size);

    // For each device name included in list
    int c;

    if (list && *list) {
        for (c=0; *list ; c++, list++) {
            gtk_list_store_append (v4l2RateList, &iter);
            gtk_list_store_set (v4l2RateList, &iter, 0, *list, 1, c, -1);
        }

        char *rate = dbus_get_video_input_device_rate();
        if (!rate || !*rate || set_combo_index_from_str(GTK_COMBO_BOX(v4l2Rate), rate, c)) {
            // if setting is invalid, choose first entry
            gtk_combo_box_set_active(GTK_COMBO_BOX(v4l2Rate), 0);
            dbus_set_video_input_rate(get_active_text(GTK_COMBO_BOX(v4l2Rate)));
        }
        free(rate);
    }
    else {
        ERROR("No video rate list found for device");
        free(list);
    }
}


/**
 * Set the video input device rate on the server
 */
static void
select_video_input_device_rate (GtkComboBox* comboBox, gpointer data UNUSED)
{
    char *str = get_active_text(comboBox);
    if (str)
        dbus_set_video_input_rate(str);
}

/**
 * Fill video input device size store
 */
static void
preferences_dialog_fill_video_input_device_size_list()
{

    GtkTreeIter iter;
    gchar** list = NULL;

    gtk_list_store_clear (v4l2SizeList);

    gchar *dev  = get_active_text(GTK_COMBO_BOX(v4l2Device));
    gchar *chan = get_active_text(GTK_COMBO_BOX(v4l2Channel));

    // Call dbus to retreive list
    if (dev && chan)
        list = dbus_get_video_input_device_size_list(dev, chan);

    if (list && *list) {
        // For each device name included in list
        int c;

        for (c=0; *list ; c++, list++) {
            gtk_list_store_append (v4l2SizeList, &iter);
            gtk_list_store_set (v4l2SizeList, &iter, 0, *list, 1, c, -1);
        }
        char *size = dbus_get_video_input_device_size();
        if (!size || !*size || set_combo_index_from_str(GTK_COMBO_BOX(v4l2Size), size, c)) {
            // if setting is invalid, choose first entry
            gtk_combo_box_set_active(GTK_COMBO_BOX(v4l2Size), 0);
            dbus_set_video_input_size(get_active_text(GTK_COMBO_BOX(v4l2Size)));
        }
        free(size);
    }
    else {
        free(list);
        ERROR("No device size list found");
    }
}

/**
 * Set the video input device size on the server
 */
static void
select_video_input_device_size (GtkComboBox* comboBox, gpointer data UNUSED)
{
    char *str = get_active_text(comboBox);
    if (str) {
        dbus_set_video_input_size(str);
        preferences_dialog_fill_video_input_device_rate_list();
    }
}

/**
 * Fill video input device input store
 */
static void
preferences_dialog_fill_video_input_device_channel_list()
{

    GtkTreeIter iter;
    gchar** list = NULL;

    gtk_list_store_clear (v4l2ChannelList);

    gchar *dev = get_active_text(GTK_COMBO_BOX(v4l2Device));

    // Call dbus to retreive list
    if (dev)
        list = dbus_get_video_input_device_channel_list(dev);

    if (list && *list) {
        // For each device name included in list
        int c;

        for (c=0; *list ; c++, list++) {
            gtk_list_store_append (v4l2ChannelList, &iter);
            gtk_list_store_set (v4l2ChannelList, &iter, 0, *list, 1, c, -1);
        }
        char *channel = dbus_get_video_input_device_channel();
        if (!channel || !*channel || set_combo_index_from_str(GTK_COMBO_BOX(v4l2Channel), channel, c)) {
            // if setting is invalid, choose first entry
            gtk_combo_box_set_active(GTK_COMBO_BOX(v4l2Channel), 0);
            dbus_set_video_input_device_channel(get_active_text(GTK_COMBO_BOX(v4l2Channel)));
        }
        free(channel);
    }
    else {
        free(list);
        ERROR("No channel list found");
    }
}

/**
 * Set the video input device input on the server
 */
static void
select_video_input_device_channel (GtkComboBox* comboBox, gpointer data UNUSED)
{
    char *str = get_active_text(comboBox);
    if (str) {
        dbus_set_video_input_device_channel (str);
        preferences_dialog_fill_video_input_device_size_list();
    }
}

/**
 * Fill video input device store
 */
static int
preferences_dialog_fill_video_input_device_list()
{

    GtkTreeIter iter;
    gchar** list = NULL;

    gtk_list_store_clear (v4l2DeviceList);

    // Call dbus to retreive list
    list = dbus_get_video_input_device_list();
    if (list && *list) {
        // For each device name included in list
        int c;
        for (c=0; *list ; c++, list++) {
            gtk_list_store_append (v4l2DeviceList, &iter);
            gtk_list_store_set (v4l2DeviceList, &iter, 0, *list, 1, c, -1);
        }
        char *dev = dbus_get_video_input_device();
        if (!dev || !*dev || set_combo_index_from_str(GTK_COMBO_BOX(v4l2Device), dev, c)) {
            // if setting is invalid, choose first entry
            gtk_combo_box_set_active(GTK_COMBO_BOX(v4l2Device), 0);
            dbus_set_video_input_device(get_active_text(GTK_COMBO_BOX(v4l2Device)));
        }
        free(dev);
        return 0;
    }
    else {
        ERROR("No device list found");
        free(list);
        return 1;
    }
}

/**
 * Set the video input device on the server
 */
static void
select_video_input_device (GtkComboBox* comboBox, gpointer data UNUSED)
{
    char *str = get_active_text(comboBox);
    if (str) {
        dbus_set_video_input_device (str);
        preferences_dialog_fill_video_input_device_channel_list();
    }
}

static void fill_devices(void)
{
    if (!preferences_dialog_fill_video_input_device_list()) {
        gtk_widget_show_all(v4l2_hbox);
        gtk_widget_hide(v4l2_nodev);
    } else {
        gtk_widget_hide_all(v4l2_hbox);
        gtk_widget_show(v4l2_nodev);
    }
}

void video_device_event_cb(DBusGProxy *proxy UNUSED, void * foo  UNUSED)
{
    fill_devices();
}

static void receiving_video_window_deleted_cb(GtkWidget *widget UNUSED, gpointer data UNUSED)
{
    sflphone_hang_up();
}


// FIXME: Should not be in config, also only handling clutter case for now
void receiving_video_event_cb(DBusGProxy *proxy, gint shmKey, gint semKey,
                              gint videoBufferSize, gint destWidth,
                              gint destHeight, GError *error, gpointer userdata)
{
    if (!receivingVideoWindow) {
        receivingVideoWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        g_signal_connect (receivingVideoWindow, "delete-event", G_CALLBACK (receiving_video_window_deleted_cb), NULL);
    }

    (void)proxy;
    (void)error;
    (void)userdata;
    gboolean using_clutter = clutter_init(NULL, NULL) == CLUTTER_INIT_SUCCESS;
    g_assert(using_clutter);

    if (!receivingVideoArea) {
        receivingVideoArea = gtk_clutter_embed_new();
        gtk_container_add(GTK_CONTAINER(receivingVideoWindow), receivingVideoArea);
    }
    g_assert(receivingVideoArea);
    g_assert(gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(receivingVideoArea)));

    if (shmKey == -1 || semKey == -1 || videoBufferSize == -1)
        return;

    gtk_widget_set_size_request (receivingVideoArea, destWidth, destHeight);
    gtk_widget_show_all(receivingVideoWindow);

    drawFormat = "rgb24";
    DEBUG("Video started for shm:%d sem:%d bufferSz:%d width:%d height:%d",
           shmKey, semKey, videoBufferSize, destWidth, destHeight);

    video_renderer = video_renderer_new(receivingVideoArea, destWidth, destHeight, drawFormat, shmKey, semKey, videoBufferSize);
    g_assert(video_renderer);
    if (video_renderer_run(video_renderer)) {
        g_object_unref(video_renderer);
        video_renderer = NULL;
        DEBUG("Could not run video renderer");
    }
    else
        DEBUG("Running video renderer");
}

// FIXME: Should not be in config, only doing clutter case for now
void stopped_receiving_video_event_cb(DBusGProxy *proxy, gint shmKey, gint semKey, GError *error, gpointer userdata)
{
    (void)proxy;
    (void)error;
    (void)userdata;

    DEBUG("Video stopped for shm:%d sem:%d", shmKey, semKey);

    if (video_renderer) {
        if (receivingVideoWindow) {
            if (GTK_IS_WIDGET(receivingVideoWindow))
                    gtk_widget_destroy(receivingVideoWindow);
            receivingVideoArea = receivingVideoWindow = NULL;
        }
        video_renderer = NULL;
    }
}


static GtkWidget* v4l2_box ()
{
    GtkWidget *item;
    GtkWidget *table;
    GtkCellRenderer *renderer;

    GtkWidget *ret = gtk_vbox_new(FALSE, 0);

    v4l2_nodev = gtk_label_new (_ ("No devices found"));
    v4l2_hbox = gtk_hbox_new (FALSE, 4);

    gtk_box_pack_start (GTK_BOX (ret) , v4l2_hbox , TRUE , TRUE , 0);
    gtk_box_pack_start (GTK_BOX (ret) , v4l2_nodev, TRUE , TRUE , 0);

    table = gtk_table_new (6, 3, FALSE);
    gtk_table_set_col_spacing (GTK_TABLE (table), 0, 40);
    gtk_box_pack_start (GTK_BOX (v4l2_hbox) , table , TRUE , TRUE , 1);

    // Set choices of input devices
    item = gtk_label_new (_ ("Device"));
    v4l2DeviceList = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    v4l2Device = gtk_combo_box_new_with_model (GTK_TREE_MODEL (v4l2DeviceList));
    gtk_label_set_mnemonic_widget (GTK_LABEL (item), v4l2Device);

    g_signal_connect (G_OBJECT (v4l2Device), "changed", G_CALLBACK (select_video_input_device), v4l2Device);
    gtk_table_attach (GTK_TABLE (table), item, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (v4l2Device), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (v4l2Device), renderer, "text", 0, NULL);
    gtk_table_attach (GTK_TABLE (table), v4l2Device, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);


    // Set choices of input
    item = gtk_label_new (_ ("Channel"));
    v4l2ChannelList = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    v4l2Channel = gtk_combo_box_new_with_model (GTK_TREE_MODEL (v4l2ChannelList));
    gtk_label_set_mnemonic_widget (GTK_LABEL (item), v4l2Channel);
    g_signal_connect (G_OBJECT (v4l2Channel), "changed", G_CALLBACK (select_video_input_device_channel), v4l2Channel);
    gtk_table_attach (GTK_TABLE (table), item, 0, 1, 1, 2, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (v4l2Channel), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (v4l2Channel), renderer, "text", 0, NULL);
    gtk_table_attach (GTK_TABLE (table), v4l2Channel, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set choices of sizes
    item = gtk_label_new (_ ("Size"));
    v4l2SizeList = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    v4l2Size = gtk_combo_box_new_with_model (GTK_TREE_MODEL (v4l2SizeList));
    gtk_label_set_mnemonic_widget (GTK_LABEL (item), v4l2Size);
    g_signal_connect (G_OBJECT (v4l2Size), "changed", G_CALLBACK (select_video_input_device_size), v4l2Size);
    gtk_table_attach (GTK_TABLE (table), item, 0, 1, 2, 3, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (v4l2Size), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (v4l2Size), renderer, "text", 0, NULL);
    gtk_table_attach (GTK_TABLE (table), v4l2Size, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set choices of rates
    item = gtk_label_new (_ ("Rate"));
    v4l2RateList = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    v4l2Rate = gtk_combo_box_new_with_model (GTK_TREE_MODEL (v4l2RateList));
    gtk_label_set_mnemonic_widget (GTK_LABEL (item), v4l2Rate);
    g_signal_connect (G_OBJECT (v4l2Rate), "changed", G_CALLBACK (select_video_input_device_rate), v4l2Rate);
    gtk_table_attach (GTK_TABLE (table), item, 0, 1, 3, 4, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (v4l2Rate), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (v4l2Rate), renderer, "text", 0, NULL);
    gtk_table_attach (GTK_TABLE (table), v4l2Rate, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    return ret;
}


static gint
on_drawarea_unrealize(GtkWidget *drawarea, gpointer data)
{
    (void)drawarea;(void)data;
    if (preview) {
        gboolean running = FALSE;
        g_object_get(preview, "running", &running, NULL);
        if (running) {
            video_renderer_stop(preview);
	    dbus_stop_video_preview();
	}
    }

    return FALSE; // call other handlers
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

    gnome_main_section_new_with_table (_ ("Video4Linux2"), &frame, &table, 1, 4);
    gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

    GtkWidget *v4l2box = v4l2_box();
    gtk_table_attach(GTK_TABLE(table), v4l2box, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 6);

    gnome_main_section_new_with_table (_ ("Preview"), &frame, &table, 1, 2);
    gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

    preview_button = gtk_button_new_with_mnemonic(_("_Start"));
    gtk_widget_set_size_request(preview_button, 80, 30);
    gtk_table_attach(GTK_TABLE(table), preview_button, 0, 1, 0, 1, 0, 0, 0, 6);
    g_signal_connect(G_OBJECT(preview_button), "clicked", G_CALLBACK(preview_button_clicked), NULL);
    gtk_widget_show(GTK_WIDGET(preview_button));

    gchar **list = dbus_get_call_list();
    gchar **orig = list;
    int active_call = 0;
    while (list && *list) {
        active_call = 1;
        g_free(*list++);
    }
    g_free(orig);

    if (active_call)
        gtk_widget_set_sensitive(GTK_WIDGET(preview_button), FALSE);

    using_clutter = clutter_init(NULL, NULL) == CLUTTER_INIT_SUCCESS;
    if (using_clutter) {
        drawarea = gtk_clutter_embed_new();
        if (!gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(drawarea))) {
            gtk_widget_destroy(drawarea);
            using_clutter = 0;
        }
    }
    if (!using_clutter) 
        drawarea = gtk_drawing_area_new();

    GdkWindow *win = gtk_widget_get_window(drawarea);
    if (win && GDK_IS_WINDOW(win))
        gdk_window_clear(win);
    g_signal_connect(drawarea, "unrealize", G_CALLBACK(on_drawarea_unrealize),
            NULL);
    gtk_widget_set_size_request (drawarea, drawWidth, drawHeight);
    gtk_table_attach(GTK_TABLE(table), drawarea, 0, 1, 1, 2, 0, 0, 0, 6);

    gtk_widget_show_all (ret);

    // get devices list from daemon *after* showing all widgets
    // that way we can show either the list, either the "no devices found" label
    fill_devices();

    return ret;
}
