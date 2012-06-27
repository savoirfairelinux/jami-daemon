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

#include <glib/gi18n.h>
#include <string.h>
#include "videoconf.h"
#include "logger.h"
#include "utils.h"
#include "unused.h"
#include "eel-gconf-extensions.h"
#include "dbus.h"
#include "codeclist.h"

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

static GtkWidget *preview_button = NULL;

static GtkWidget *codecTreeView; // View used instead of store to get access to selection
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
select_codec(GtkTreeSelection *selection, GtkTreeModel *model)
{
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_widget_set_sensitive(GTK_WIDGET(codecMoveUpButton), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(codecMoveDownButton), FALSE);
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(codecMoveUpButton), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(codecMoveDownButton), TRUE);
    }
}

void active_is_always_recording()
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

static const gchar *const PREVIEW_START_STR = "_Start";
static const gchar *const PREVIEW_STOP_STR = "_Stop";

static void
preview_button_toggled(GtkButton *button, gpointer data UNUSED)
{
    preview_button = GTK_WIDGET(button);
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        dbus_start_video_preview();
    else
        dbus_stop_video_preview();

    update_preview_button_label();
}

void
set_preview_button_sensitivity(gboolean sensitive)
{
    if (!preview_button || !GTK_IS_WIDGET(preview_button))
        return;
    DEBUG("%ssetting preview button", sensitive ? "" : "Un");
    gtk_widget_set_sensitive(GTK_WIDGET(preview_button), sensitive);
}

void
update_preview_button_label()
{
    if (!preview_button || !GTK_IS_WIDGET(preview_button))
        return;

    GtkToggleButton *button = GTK_TOGGLE_BUTTON(preview_button);
    if (dbus_has_video_preview_started()) {
        /* We call g_object_set to avoid triggering the "toggled" signal */
        gtk_button_set_label(GTK_BUTTON(button), _(PREVIEW_STOP_STR));
        g_object_set(button, "active", TRUE, NULL);
    } else {
        gtk_button_set_label(GTK_BUTTON(button), _(PREVIEW_START_STR));
        g_object_set(button, "active", FALSE, NULL);
    }
}

/**
 * Fills the tree list with supported codecs
 */
static void preferences_dialog_fill_codec_list(account_t *a)
{
    // Get model of view and clear it
    GtkListStore *codecStore = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(codecTreeView)));
    gtk_list_store_clear(codecStore);

    GQueue *list = a ? a->vcodecs : get_video_codecs_list();

    // Add the codecs in the list
    for (size_t i = 0; i < list->length; i++) {
        codec_t *c = g_queue_peek_nth(list, i);

        if (c) {
            DEBUG("%s is %sactive", c->name, c->is_active ? "" : "not ");
            GtkTreeIter iter;
            gtk_list_store_append(codecStore, &iter);
            gchar *bitrate = g_strdup_printf("%s kbps", c->bitrate);

            gtk_list_store_set(codecStore, &iter, COLUMN_CODEC_ACTIVE,
                               c->is_active, COLUMN_CODEC_NAME, c->name,
                               COLUMN_CODEC_BITRATE, bitrate, -1);
            g_free(bitrate);
        }
    }
}

/**
 * Toggle active value of codec on click and update changes to the deamon
 * and in configuration files
 */

static void
codec_active_toggled(GtkCellRendererToggle *renderer UNUSED, gchar *path,
                     gpointer data)
{
    // Get path of clicked codec active toggle box
    GtkTreePath *treePath = gtk_tree_path_new_from_string(path);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW (codecTreeView));
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, treePath);

    // Retrieve userdata
    account_t *acc = (account_t*) data;

    if (!acc) {
        ERROR("No account selected");
        return;
    }

    // Get active value and name at iteration
    gboolean active = FALSE;
    gchar *name = NULL;
    gtk_tree_model_get(model, &iter, COLUMN_CODEC_ACTIVE, &active,
                       COLUMN_CODEC_NAME, &name, -1);

    DEBUG("%s", name);
    DEBUG("video codecs length %i", g_queue_get_length(acc->vcodecs));

    codec_t *codec = codec_list_get_by_name((gconstpointer) name, acc->vcodecs);

    // Toggle active value
    active = !active;

    // Store value
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_CODEC_ACTIVE,
                       active, -1);

    gtk_tree_path_free(treePath);

    // Modify codec queue to represent change
    codec->is_active = active;
}

/**
 * Move codec in list depending on direction and selected codec and
 * update changes in the daemon list and the configuration files
 */
static void codec_move(gboolean moveUp, gpointer data)
{
    GtkTreeIter iter;
    GtkTreeIter *iter2;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreePath *treePath;
    gchar *path;

    // Get view, model and selection of codec store
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(codecTreeView));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(codecTreeView));

    // Find selected iteration and create a copy
    gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), &model, &iter);
    iter2 = gtk_tree_iter_copy(&iter);

    // Find path of iteration
    path = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL (model), &iter);
    treePath = gtk_tree_path_new_from_string(path);
    gint *indices = gtk_tree_path_get_indices(treePath);
    gint pos = indices[0];

    // Depending on button direction get new path
    if (moveUp)
        gtk_tree_path_prev(treePath);
    else
        gtk_tree_path_next(treePath);

    gtk_tree_model_get_iter(model, &iter, treePath);

    // Swap iterations if valid
    if (gtk_list_store_iter_is_valid (GTK_LIST_STORE (model), &iter))
        gtk_list_store_swap(GTK_LIST_STORE (model), &iter, iter2);

    // Scroll to new position
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(codecTreeView), treePath, NULL, FALSE, 0, 0);

    // Free resources
    gtk_tree_path_free(treePath);
    gtk_tree_iter_free(iter2);
    g_free(path);

    // Perpetuate changes in codec queue
    if (moveUp)
        codec_list_move_codec_up(pos, &((account_t*)data)->vcodecs);
    else
        codec_list_move_codec_down(pos, &((account_t*)data)->vcodecs);
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

GtkWidget* videocodecs_box(account_t *a)
{
    GtkWidget *ret = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    GtkWidget *scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_SHADOW_IN);

    gtk_box_pack_start(GTK_BOX(ret), scrolledWindow, TRUE, TRUE, 0);
    GtkListStore *codecStore = gtk_list_store_new(CODEC_COLUMN_COUNT,
                                                  G_TYPE_BOOLEAN, // Active
                                                  G_TYPE_STRING,  // Name
                                                  G_TYPE_STRING,  // Bit rate
                                                  G_TYPE_STRING   // Bandwith
                                                 );

    // Create codec tree view with list store
    codecTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(codecStore));

    // Get tree selection manager
    GtkTreeSelection *treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(codecTreeView));
    g_signal_connect(G_OBJECT(treeSelection), "changed",
            G_CALLBACK(select_codec),
            codecStore);

    // Active column
    GtkCellRenderer *renderer = gtk_cell_renderer_toggle_new();
    GtkTreeViewColumn *treeViewColumn = gtk_tree_view_column_new_with_attributes("", renderer, "active", COLUMN_CODEC_ACTIVE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);

    // Toggle codec active property on clicked
    g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(codec_active_toggled), (gpointer) a);

    // Name column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "markup", COLUMN_CODEC_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);

    // Bitrate column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes(_("Bitrate"), renderer, "text", COLUMN_CODEC_BITRATE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);

    g_object_unref(G_OBJECT(codecStore));
    gtk_container_add(GTK_CONTAINER(scrolledWindow), codecTreeView);

    // Create button box
    GtkWidget *buttonBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(buttonBox), 10);
    gtk_box_pack_start(GTK_BOX(ret), buttonBox, FALSE, FALSE, 0);

    codecMoveUpButton = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
    gtk_widget_set_sensitive(GTK_WIDGET(codecMoveUpButton), FALSE);
    gtk_box_pack_start(GTK_BOX(buttonBox), codecMoveUpButton, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(codecMoveUpButton), "clicked", G_CALLBACK(codec_move_up), a);

    codecMoveDownButton = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
    gtk_widget_set_sensitive(GTK_WIDGET(codecMoveDownButton), FALSE);
    gtk_box_pack_start(GTK_BOX(buttonBox), codecMoveDownButton, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(codecMoveDownButton), "clicked", G_CALLBACK(codec_move_down), a);

    preferences_dialog_fill_codec_list(a);

    return ret;
}

/* Gets a newly allocated string with the active text, the caller must
 * free this string */
static gchar *get_active_text(GtkComboBox *box)
{
    gchar *text = NULL;
    int comboBoxIndex = gtk_combo_box_get_active(box);
    if (comboBoxIndex >= 0) {
        GtkTreeIter iter;
        gtk_combo_box_get_active_iter(box, &iter);
        gtk_tree_model_get(gtk_combo_box_get_model(box), &iter, 0, &text, -1);
    }
    return text;
}

/* Return 0 if string was found in the combo box, != 0 if the string was not found */
static int set_combo_index_from_str(GtkComboBox *box, const gchar *str, size_t max)
{
    g_assert(str);

    GtkTreeModel *model = gtk_combo_box_get_model(box);
    GtkTreeIter iter;
    unsigned idx = 0;
    gtk_tree_model_get_iter_first(model, &iter);
    do {
        gchar *boxstr;
        gtk_tree_model_get(model, &iter, 0, &boxstr, -1);
        if (boxstr && !g_strcmp0(boxstr, str))
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

    if (v4l2RateList)
        gtk_list_store_clear(v4l2RateList);

    gchar *dev  = get_active_text(GTK_COMBO_BOX(v4l2Device));
    gchar *chan = get_active_text(GTK_COMBO_BOX(v4l2Channel));
    gchar *size = get_active_text(GTK_COMBO_BOX(v4l2Size));

    // Call dbus to retreive list
    if (dev && chan && size) {
      list = dbus_get_video_device_rate_list(dev, chan, size);
      g_free(size);
      g_free(chan);
      g_free(dev);
    }

    // For each device name included in list
    if (list && *list) {
        gint c = 0;
        for (gchar **tmp = list; *tmp; c++, tmp++) {
            gtk_list_store_append(v4l2RateList, &iter);
            gtk_list_store_set(v4l2RateList, &iter, 0, *tmp, 1, c, -1);
        }
        g_strfreev(list);

        gchar *rate = dbus_get_active_video_device_rate();
        if (!rate || !*rate || set_combo_index_from_str(GTK_COMBO_BOX(v4l2Rate), rate, c)) {
            // if setting is invalid, choose first entry
            gtk_combo_box_set_active(GTK_COMBO_BOX(v4l2Rate), 0);
            dbus_set_active_video_device_rate(get_active_text(GTK_COMBO_BOX(v4l2Rate)));
        }
        g_free(rate);
    } else
        ERROR("No video rate list found for device");
}


/**
 * Set the video input device rate on the server
 */
static void
select_video_input_device_rate_cb(GtkComboBox* comboBox, gpointer data UNUSED)
{
    gchar *str = get_active_text(comboBox);
    if (str)
        dbus_set_active_video_device_rate(str);
    g_free(str);
}

/**
 * Fill video input device size store
 */
static void
preferences_dialog_fill_video_input_device_size_list()
{
    if (v4l2SizeList)
        gtk_list_store_clear(v4l2SizeList);

    gchar *dev  = get_active_text(GTK_COMBO_BOX(v4l2Device));
    gchar *chan = get_active_text(GTK_COMBO_BOX(v4l2Channel));

    gchar** list = NULL;
    // Call dbus to retrieve list
    if (dev && chan) {
        list = dbus_get_video_device_size_list(dev, chan);
        g_free(chan);
        g_free(dev);
    }

    if (list && *list) {
        // For each device name included in list
        gint c = 0;
        for (gchar **tmp = list; *tmp; c++, tmp++) {
            GtkTreeIter iter;
            gtk_list_store_append(v4l2SizeList, &iter);
            gtk_list_store_set(v4l2SizeList, &iter, 0, *tmp, 1, c, -1);
        }
        g_strfreev(list);
        gchar *size = dbus_get_active_video_device_size();
        if (!size || !*size || set_combo_index_from_str(GTK_COMBO_BOX(v4l2Size), size, c)) {
            // if setting is invalid, choose first entry
            gtk_combo_box_set_active(GTK_COMBO_BOX(v4l2Size), 0);
            dbus_set_active_video_device_size(get_active_text(GTK_COMBO_BOX(v4l2Size)));
        }
        g_free(size);
    } else
        ERROR("No device size list found");
}

/**
 * Set the video input device size on the server
 */
static void
select_video_input_device_size_cb(GtkComboBox* comboBox, gpointer data UNUSED)
{
    gchar *str = get_active_text(comboBox);
    if (str) {
        dbus_set_active_video_device_size(str);
        preferences_dialog_fill_video_input_device_rate_list();
        g_free(str);
    }
}

/**
 * Fill video input device input store
 */
static void
preferences_dialog_fill_video_input_device_channel_list()
{
    if (v4l2ChannelList)
        gtk_list_store_clear(v4l2ChannelList);

    gchar *dev = get_active_text(GTK_COMBO_BOX(v4l2Device));

    gchar **list = NULL;
    // Call dbus to retrieve list
    if (dev) {
        list = dbus_get_video_device_channel_list(dev);
        g_free(dev);
    }

    if (list && *list) {
        // For each device name included in list
        int c = 0;
        for (gchar **tmp = list; *tmp; c++, tmp++) {
            GtkTreeIter iter;
            gtk_list_store_append(v4l2ChannelList, &iter);
            gtk_list_store_set(v4l2ChannelList, &iter, 0, *tmp, 1, c, -1);
        }
        g_strfreev(list);
        gchar *channel = dbus_get_active_video_device_channel();
        if (!channel || !*channel || set_combo_index_from_str(GTK_COMBO_BOX(v4l2Channel), channel, c)) {
            // if setting is invalid, choose first entry
            gtk_combo_box_set_active(GTK_COMBO_BOX(v4l2Channel), 0);
            dbus_set_active_video_device_channel(get_active_text(GTK_COMBO_BOX(v4l2Channel)));
        }
        g_free(channel);
    } else
        ERROR("No channel list found");
}

/**
 * Set the video input device input on the server
 */
static void
select_video_input_device_channel_cb(GtkComboBox* comboBox, gpointer data UNUSED)
{
    gchar *str = get_active_text(comboBox);
    if (str) {
        dbus_set_active_video_device_channel(str);
        preferences_dialog_fill_video_input_device_size_list();
        g_free(str);
    }
}

/**
 * Fill video input device store
 */
static gboolean
preferences_dialog_fill_video_input_device_list()
{
    gtk_list_store_clear(v4l2DeviceList);

    // Call dbus to retrieve list
    gchar **list = dbus_get_video_device_list();
    if (!list || !*list) {
        ERROR("No device list found");
        return FALSE;
    } else {
        // For each device name included in list
        gint c = 0;
        for (gchar **tmp = list; *tmp; c++, tmp++) {
            GtkTreeIter iter;
            gtk_list_store_append(v4l2DeviceList, &iter);
            gtk_list_store_set(v4l2DeviceList, &iter, 0, *tmp, 1, c, -1);
        }
        g_strfreev(list);
        gchar *dev = dbus_get_active_video_device();
        if (!dev || !*dev || set_combo_index_from_str(GTK_COMBO_BOX(v4l2Device), dev, c)) {
            // if setting is invalid, choose first entry
            gtk_combo_box_set_active(GTK_COMBO_BOX(v4l2Device), 0);
            dbus_set_active_video_device(get_active_text(GTK_COMBO_BOX(v4l2Device)));
        }
        g_free(dev);
        return TRUE;
    }
}

/**
 * Set the video input device on the server
 */
static void
select_video_input_device_cb(GtkComboBox* comboBox, gpointer data UNUSED)
{
    gchar *str = get_active_text(comboBox);
    if (str) {
        DEBUG("Setting video input device to %s", str);
        dbus_set_active_video_device(str);
        preferences_dialog_fill_video_input_device_channel_list();
        g_free(str);
    }
}

static void fill_devices(void)
{
    if (preferences_dialog_fill_video_input_device_list()) {
        gtk_widget_show_all(v4l2_hbox);
        gtk_widget_hide(v4l2_nodev);
        gtk_widget_set_sensitive(preview_button, TRUE);
    } else {
        gtk_widget_hide(v4l2_hbox);
        gtk_widget_show(v4l2_nodev);
        gtk_widget_set_sensitive(preview_button, FALSE);
    }
}

void video_device_event_cb(DBusGProxy *proxy UNUSED, void * foo UNUSED)
{
    fill_devices();
}


static GtkWidget* v4l2_box()
{
    DEBUG("%s", __PRETTY_FUNCTION__);
    GtkWidget *ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    v4l2_nodev = gtk_label_new(_("No devices found"));
    v4l2_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    gtk_box_pack_start(GTK_BOX(ret), v4l2_hbox , TRUE , TRUE , 0);
    gtk_box_pack_start(GTK_BOX(ret), v4l2_nodev, TRUE , TRUE , 0);

    GtkWidget *table = gtk_table_new(6, 3, FALSE);
    gtk_table_set_col_spacing(GTK_TABLE(table), 0, 40);
    gtk_box_pack_start(GTK_BOX(v4l2_hbox) , table , TRUE , TRUE , 1);

    // Set choices of input devices
    GtkWidget *item = gtk_label_new(_("Device"));
    v4l2DeviceList = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    v4l2Device = gtk_combo_box_new_with_model(GTK_TREE_MODEL(v4l2DeviceList));
    gtk_label_set_mnemonic_widget(GTK_LABEL(item), v4l2Device);

    g_signal_connect(G_OBJECT(v4l2Device), "changed", G_CALLBACK(select_video_input_device_cb), NULL);
    gtk_table_attach(GTK_TABLE(table), item, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set rendering
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(v4l2Device), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(v4l2Device), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), v4l2Device, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set choices of input
    item = gtk_label_new(_("Channel"));
    v4l2ChannelList = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    v4l2Channel = gtk_combo_box_new_with_model(GTK_TREE_MODEL(v4l2ChannelList));
    gtk_label_set_mnemonic_widget(GTK_LABEL(item), v4l2Channel);
    g_signal_connect(G_OBJECT(v4l2Channel), "changed", G_CALLBACK(select_video_input_device_channel_cb), NULL);
    gtk_table_attach(GTK_TABLE(table), item, 0, 1, 1, 2, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(v4l2Channel), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(v4l2Channel), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), v4l2Channel, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set choices of sizes
    item = gtk_label_new(_("Size"));
    v4l2SizeList = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    v4l2Size = gtk_combo_box_new_with_model(GTK_TREE_MODEL(v4l2SizeList));
    gtk_label_set_mnemonic_widget(GTK_LABEL(item), v4l2Size);
    g_signal_connect(G_OBJECT(v4l2Size), "changed", G_CALLBACK(select_video_input_device_size_cb), NULL);
    gtk_table_attach(GTK_TABLE(table), item, 0, 1, 2, 3, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(v4l2Size), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(v4l2Size), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), v4l2Size, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set choices of rates
    item = gtk_label_new(_("Rate"));
    v4l2RateList = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    v4l2Rate = gtk_combo_box_new_with_model(GTK_TREE_MODEL(v4l2RateList));
    gtk_label_set_mnemonic_widget(GTK_LABEL(item), v4l2Rate);
    g_signal_connect(G_OBJECT(v4l2Rate), "changed", G_CALLBACK(select_video_input_device_rate_cb), NULL);
    gtk_table_attach(GTK_TABLE(table), item, 0, 1, 3, 4, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(v4l2Rate), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(v4l2Rate), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), v4l2Rate, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);

    return ret;
}


GtkWidget* create_video_configuration()
{
    // Main widget
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    // Sub boxes
    GtkWidget *frame, *table;
    gnome_main_section_new_with_table(_("Video Manager"), &frame, &table, 1, 5);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    gnome_main_section_new_with_table(_("Video4Linux2"), &frame, &table, 1, 4);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    GtkWidget *v4l2box = v4l2_box();
    gtk_table_attach(GTK_TABLE(table), v4l2box, 0, 1, 1, 2,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 6);

    gnome_main_section_new_with_table(_("Preview"), &frame, &table, 1, 2);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    const gboolean started = dbus_has_video_preview_started();

    preview_button = gtk_toggle_button_new_with_mnemonic(started ? _(PREVIEW_STOP_STR) : _(PREVIEW_START_STR));
    gtk_widget_set_size_request(preview_button, 80, 30);
    gtk_table_attach(GTK_TABLE(table), preview_button, 0, 1, 0, 1, 0, 0, 0, 6);
    gtk_widget_show(GTK_WIDGET(preview_button));
    if (started)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(preview_button), TRUE);
    g_signal_connect(G_OBJECT(preview_button), "toggled",
                     G_CALLBACK(preview_button_toggled), NULL);

    gchar **list = dbus_get_call_list();
    gboolean active_call;
    if (list && *list) {
        active_call = TRUE;
        g_strfreev(list);
    }

    if (active_call)
        gtk_widget_set_sensitive(GTK_WIDGET(preview_button), FALSE);

    gtk_widget_show_all(vbox);

    // get devices list from daemon *after* showing all widgets
    // that way we can show either the list, either the "no devices found" label
    fill_devices();

    return vbox;
}
