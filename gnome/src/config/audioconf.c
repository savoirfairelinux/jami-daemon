/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include "str_utils.h"
#include "codeclist.h"
#include "sflphone_const.h"
#include "audioconf.h"
#include "utils.h"
#include "logger.h"
#include "dbus/dbus.h"
#include "uimanager.h"
#include "mainwindow.h"

/* FIXME: these should be in a struct rather than at file scope */
static GtkWidget *codecMoveUpButton;
static GtkWidget *codecMoveDownButton;
static GtkWidget *codecTreeView; // View used instead of store to get access to selection
static GtkWidget *pulsebox;
static GtkWidget *pulse_conf;
static GtkWidget *alsabox;
static GtkWidget *alsa_conf;
static GtkWidget *alsa_output;
static GtkWidget *alsa_input;
static GtkWidget *alsa_ringtone;

// Codec properties ID
enum {
    COLUMN_CODEC_ACTIVE,
    COLUMN_CODEC_NAME,
    COLUMN_CODEC_FREQUENCY,
    COLUMN_CODEC_BITRATE,
    CODEC_COLUMN_COUNT
};

#define KBPS "kbps"
#define KHZ "kHz"

/**
 * Fills the tree list with supported codecs
 */
static void
fill_codec_list(const account_t *account)
{
    // Get model of view and clear it
    GtkListStore *codecStore = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(codecTreeView)));
    gtk_list_store_clear(codecStore);

    GQueue *list;
    if (!account) {
        DEBUG("Account is NULL, using global codec list");
        list = get_audio_codecs_list();
    } else {
        list = account->acodecs;
    }

    // Insert codecs
    for (size_t i = 0; i < list->length; ++i) {
        codec_t *c = g_queue_peek_nth(list, i);

        if (c) {
            DEBUG("%s is %sactive", c->name, c->is_active ? "" : "not ");
            GtkTreeIter iter;
            gtk_list_store_append(codecStore, &iter);
            gchar *samplerate = g_strdup_printf("%d " KHZ, (gint) (c->sample_rate * 0.001));
            gchar *bitrate = g_strdup_printf("%s " KBPS, c->bitrate);

            gtk_list_store_set(codecStore, &iter,
                               COLUMN_CODEC_ACTIVE, c->is_active,
                               COLUMN_CODEC_NAME, c->name,
                               COLUMN_CODEC_FREQUENCY, samplerate,
                               COLUMN_CODEC_BITRATE, bitrate,
                               -1);
            g_free(samplerate);
            g_free(bitrate);
        }
    }
}

static GtkListStore *
create_device_list_store(gchar **list)
{
    GtkListStore *list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);

    for (gchar **tmp = list; tmp && *tmp; ++tmp) {
        gint device_index = dbus_get_audio_device_index(*tmp);
        GtkTreeIter iter;
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, *tmp, 1, device_index, -1);
    }
    g_strfreev(list);
    return list_store;
}

/**
 * Fill store with output audio plugins
 */
static GtkListStore*
create_alsa_plugin_list_store()
{
    GtkListStore *list_store = gtk_list_store_new(1, G_TYPE_STRING);

    // Call dbus to retreive list
    gchar **list = dbus_get_audio_plugin_list();

    // For each plugin name included in list
    for (gchar **tmp = list; tmp && *tmp; ++tmp) {
        GtkTreeIter iter;
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, *tmp, -1);
    }
    g_strfreev(list);
    return list_store;
}

static void
select_active_output_audio_device(GtkWidget *output)
{
    // Select active output device on server
    gchar **devices = dbus_get_current_audio_devices_index();
    if (devices && devices[0]) {

        int currentDeviceIndex = atoi(devices[0]);
        g_strfreev(devices);
        DEBUG("audio device index for output = %d", currentDeviceIndex);
        GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(output));

        // Find the currently set output device
        GtkTreeIter iter;
        gtk_tree_model_get_iter_first(model, &iter);

        do {
            int deviceIndex;
            gtk_tree_model_get(model, &iter, 1, &deviceIndex, -1);

            if (deviceIndex == currentDeviceIndex) {
                // Set current iteration the active one
                gtk_combo_box_set_active_iter(GTK_COMBO_BOX(output), &iter);
                return;
            }
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    // No index was found, select first one
    WARN("No active output device found");
    gtk_combo_box_set_active(GTK_COMBO_BOX(output), 0);
}


/**
 * Select active output audio device
 */
static void
select_active_ringtone_audio_device(GtkWidget *ringtone)
{
    // Select active ringtone device on server
    gchar **devices = dbus_get_current_audio_devices_index();
    if (devices && devices[2]) {
        const gint currentDeviceIndex = atoi(devices[2]);
        g_strfreev(devices);
        DEBUG("audio device index for ringtone = %d", currentDeviceIndex);
        GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(ringtone));

        // Find the currently set ringtone device
        GtkTreeIter iter;
        gtk_tree_model_get_iter_first(model, &iter);

        do {
            int deviceIndex;
            gtk_tree_model_get(model, &iter, 1, &deviceIndex, -1);

            if (deviceIndex == currentDeviceIndex) {
                // Set current iteration the active one
                gtk_combo_box_set_active_iter(GTK_COMBO_BOX(ringtone), &iter);
                return;
            }
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    // No index was found, select first one
    WARN("No active ringtone device found");
    gtk_combo_box_set_active(GTK_COMBO_BOX(ringtone), 0);
}

static GtkListStore*
create_output_list_store()
{
    gchar **list = dbus_get_audio_output_device_list();
    return create_device_list_store(list);
}

static GtkListStore*
create_input_list_store()
{
    gchar **list = dbus_get_audio_input_device_list();
    return create_device_list_store(list);
}

static void
select_active_input_audio_device(GtkWidget *input)
{
    // Select active input device on server
    gchar **devices = dbus_get_current_audio_devices_index();
    if (devices && devices[1]) {
        int currentDeviceIndex = atoi(devices[1]);
        g_strfreev(devices);
        GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(input));

        // Find the currently set input device
        GtkTreeIter iter;
        gtk_tree_model_get_iter_first(model, &iter);

        do {
            int deviceIndex;
            gtk_tree_model_get(model, &iter, 1, &deviceIndex, -1);

            if (deviceIndex == currentDeviceIndex) {
                // Set current iteration the active one
                gtk_combo_box_set_active_iter(GTK_COMBO_BOX(input), &iter);
                return;
            }
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    // No index was found, select first one
    WARN("No active input device found");
    gtk_combo_box_set_active(GTK_COMBO_BOX(input), 0);
}

static void
update_device_widget(const gchar *pluginName, GtkWidget *output, GtkWidget *input, GtkWidget *ringtone)
{
    const gboolean is_default = utf8_case_equal(pluginName, "default");
    gtk_widget_set_sensitive(output, !is_default);
    gtk_widget_set_sensitive(input, !is_default);
    gtk_widget_set_sensitive(ringtone, !is_default);
}

static void
select_output_alsa_plugin(GtkComboBox* widget, G_GNUC_UNUSED gpointer data)
{
    if (!must_show_alsa_conf())
        return;
    const gint comboBoxIndex = gtk_combo_box_get_active(widget);

    if (comboBoxIndex >= 0) {
        GtkTreeModel *model = gtk_combo_box_get_model(widget);
        GtkTreeIter iter;
        gtk_combo_box_get_active_iter(widget, &iter);
        gchar* pluginName;
        gtk_tree_model_get(model, &iter, 0, &pluginName, -1);
        dbus_set_audio_plugin(pluginName);
        update_device_widget(pluginName, alsa_output, alsa_input, alsa_ringtone);
    }
}

static void
select_active_output_alsa_plugin(GtkWidget *alsa_plugin)
{
    if (!must_show_alsa_conf())
        return;
    // Select active output device on server
    GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(alsa_plugin));

    // Find the current alsa plugin
    GtkTreeIter iter;
    gtk_tree_model_get_iter_first(model, &iter);

    gchar *alsa_pluginname = dbus_get_current_audio_output_plugin();
    gchar *tmp = alsa_pluginname;

    do {
        gtk_tree_model_get(model, &iter, 0, &alsa_pluginname, -1);

        if (utf8_case_equal(tmp, alsa_pluginname)) {
            g_free(alsa_pluginname);
            // Set current iteration the active one
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(alsa_plugin), &iter);
            return;
        }
    } while (gtk_tree_model_iter_next(model, &iter));
    g_free(alsa_pluginname);

    // No index was found, select first one
    WARN("No active output device found");
    gtk_combo_box_set_active(GTK_COMBO_BOX(alsa_plugin), 0);
}

static gint
get_device_index_from_combobox(GtkComboBox* comboBox)
{
    GtkTreeModel* model = gtk_combo_box_get_model(comboBox);
    GtkTreeIter iter;
    gtk_combo_box_get_active_iter(comboBox, &iter);
    gint deviceIndex;
    gtk_tree_model_get(model, &iter, 1, &deviceIndex, -1);
    return deviceIndex;
}

static void
select_audio_output_device(GtkComboBox* comboBox, G_GNUC_UNUSED gpointer data)
{
    if (gtk_combo_box_get_active(comboBox) >= 0)
        dbus_set_audio_output_device(get_device_index_from_combobox(comboBox));
}

static void
select_audio_input_device(GtkComboBox* comboBox, G_GNUC_UNUSED gpointer data)
{
    if (gtk_combo_box_get_active(comboBox) >= 0)
       dbus_set_audio_input_device(get_device_index_from_combobox(comboBox));
}

static void
select_audio_ringtone_device(GtkComboBox *comboBox, G_GNUC_UNUSED gpointer data)
{
    if (gtk_combo_box_get_active(comboBox) >= 0)
        dbus_set_audio_ringtone_device(get_device_index_from_combobox(comboBox));
}

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

/**
 * Toggle active value of codec on click and update changes to the deamon
 * and in configuration files
 */
static void
codec_active_toggled(G_GNUC_UNUSED GtkCellRendererToggle *renderer, gchar *path, gpointer data)
{
    // Get path of clicked codec active toggle box
    GtkTreePath *treePath = gtk_tree_path_new_from_string(path);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(codecTreeView));
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, treePath);

    // Retrieve userdata
    account_t *acc = (account_t*) data;

    if (!acc) {
        ERROR("no account selected");
        return;
    }

    // Get active value and name at iteration
    gboolean active;
    gchar* name;
    gchar* samplerate;
    gtk_tree_model_get(model, &iter, COLUMN_CODEC_ACTIVE, &active,
                       COLUMN_CODEC_NAME, &name, COLUMN_CODEC_FREQUENCY,
                       &samplerate, -1);

    DEBUG("Selected Codec: %s, %s", name, samplerate);

    codec_t* codec = NULL;

    const gboolean is_speex = utf8_case_equal(name, "speex");
    if (is_speex) {
        if (utf8_case_equal(samplerate, "8 " KHZ))
            codec = codec_list_get_by_payload(110, acc->acodecs);
        else if (utf8_case_equal(samplerate, "16 " KHZ))
            codec = codec_list_get_by_payload(111, acc->acodecs);
        else if (utf8_case_equal(samplerate, "32 " KHZ))
            codec = codec_list_get_by_payload(112, acc->acodecs);
        else
            codec = codec_list_get_by_name((gconstpointer) name, acc->acodecs);
    } else {
        codec = codec_list_get_by_name((gconstpointer) name, acc->acodecs);
    }

    // Toggle active value
    active = !active;

    // Store value
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COLUMN_CODEC_ACTIVE, active,
                       -1);

    gtk_tree_path_free(treePath);

    // Modify codec queue to represent change
    if (codec)
        codec_set_active(codec, active);
}

/**
 * Move codec in list depending on direction and selected codec and
 * update changes in the daemon list and the configuration files
 */
static void codec_move(gboolean moveUp, gpointer data)
{
    // Get view, model and selection of codec store
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(codecTreeView));
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(codecTreeView));

    // Find selected iteration and create a copy
    GtkTreeIter iter;
    gtk_tree_selection_get_selected(selection, &model, &iter);
    GtkTreeIter *iter2 = gtk_tree_iter_copy(&iter);

    // Find path of iteration
    gchar *path = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(model), &iter);
    GtkTreePath *treePath = gtk_tree_path_new_from_string(path);
    gint *indices = gtk_tree_path_get_indices(treePath);
    gint indice = indices[0];

    // Depending on button direction get new path
    if (moveUp)
        gtk_tree_path_prev(treePath);
    else
        gtk_tree_path_next(treePath);

    gtk_tree_model_get_iter(model, &iter, treePath);

    // Swap iterations if valid
    if (gtk_list_store_iter_is_valid(GTK_LIST_STORE(model), &iter))
        gtk_list_store_swap(GTK_LIST_STORE(model), &iter, iter2);

    // Scroll to new position
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(codecTreeView), treePath, NULL, FALSE, 0, 0);

    // Free resources
    gtk_tree_path_free(treePath);
    gtk_tree_iter_free(iter2);
    g_free(path);

    // Retrieve the user data
    account_t *acc = (account_t*) data;

    if (acc) {
        // propagate changes in codec queue
        if (moveUp)
            codec_list_move_codec_up(indice, &acc->acodecs);
        else
            codec_list_move_codec_down(indice, &acc->acodecs);
    }
}

/**
 * Called from move up codec button signal
 */
static void codec_move_up(G_GNUC_UNUSED GtkButton *button, gpointer data)
{
    // Change tree view ordering and get indice changed
    codec_move(TRUE, data);
}

/**
 * Called from move down codec button signal
 */
static void codec_move_down(G_GNUC_UNUSED GtkButton *button, gpointer data)
{
    // Change tree view ordering and get indice changed
    codec_move(FALSE, data);
}

GtkWidget*
audiocodecs_box(const account_t *account)
{
    GtkWidget *audiocodecs_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(audiocodecs_hbox), 10);

    GtkWidget *scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_SHADOW_IN);

    gtk_box_pack_start(GTK_BOX(audiocodecs_hbox), scrolledWindow, TRUE, TRUE, 0);
    GtkListStore *codecStore = gtk_list_store_new(CODEC_COLUMN_COUNT,
            G_TYPE_BOOLEAN, /* Active */
            G_TYPE_STRING, /* Name */
            G_TYPE_STRING, /* Frequency */
            G_TYPE_STRING, /* Bitrate */
            G_TYPE_STRING  /* Bandwidth */);

    // Create codec tree view with list store
    codecTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(codecStore));

    /* The list store model will be destroyed automatically with the view */
    g_object_unref(G_OBJECT(codecStore));

    // Get tree selection manager
    GtkTreeSelection *treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(codecTreeView));
    g_signal_connect(G_OBJECT(treeSelection), "changed",
                     G_CALLBACK(select_codec), codecStore);

    // Active column
    GtkCellRenderer *renderer = gtk_cell_renderer_toggle_new();
    GtkTreeViewColumn *treeViewColumn = gtk_tree_view_column_new_with_attributes("", renderer, "active", COLUMN_CODEC_ACTIVE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);

    // Toggle codec active property on clicked
    g_signal_connect(G_OBJECT(renderer), "toggled",
                     G_CALLBACK(codec_active_toggled), (gpointer) account);

    // Name column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "markup", COLUMN_CODEC_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);

    // Bit rate column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes(_("Frequency"), renderer, "text", COLUMN_CODEC_FREQUENCY, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);

    // Bandwith column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes(_("Bitrate"), renderer, "text", COLUMN_CODEC_BITRATE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);

    gtk_container_add(GTK_CONTAINER(scrolledWindow), codecTreeView);

    // Create button box
    GtkWidget *buttonBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(buttonBox), 10);
    gtk_box_pack_start(GTK_BOX(audiocodecs_hbox), buttonBox, FALSE, FALSE, 0);

    codecMoveUpButton = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
    gtk_widget_set_sensitive(GTK_WIDGET(codecMoveUpButton), FALSE);
    gtk_box_pack_start(GTK_BOX(buttonBox), codecMoveUpButton, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(codecMoveUpButton), "clicked",
                     G_CALLBACK(codec_move_up), (gpointer) account);

    codecMoveDownButton = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
    gtk_widget_set_sensitive(GTK_WIDGET(codecMoveDownButton), FALSE);
    gtk_box_pack_start(GTK_BOX(buttonBox), codecMoveDownButton, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(codecMoveDownButton), "clicked",
                     G_CALLBACK(codec_move_down), (gpointer) account);

    fill_codec_list(account);

    return audiocodecs_hbox;
}

static GtkWidget* alsa_box()
{
    GtkWidget *alsa_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_show(alsa_hbox);

    GtkWidget *table = gtk_table_new(6, 3, FALSE);
    gtk_table_set_col_spacing(GTK_TABLE(table), 0, 40);
    gtk_box_pack_start(GTK_BOX(alsa_hbox) , table , TRUE , TRUE , 1);
    gtk_widget_show(table);

    gchar *message = "<small><i>default</i> plugin always uses internal sound card. Select <i>dmix/dsnoop</i> to use an alternate soundcard.</small>";
    GtkWidget *info_bar = gnome_info_bar(message, GTK_MESSAGE_INFO);
    gtk_table_attach(GTK_TABLE(table), info_bar, 1, 3, 1, 2, GTK_FILL, GTK_SHRINK, 10, 10);

    DEBUG("Configuration plugin");
    GtkWidget *label = gtk_label_new(_("ALSA plugin"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(label);
    // Set choices of audio managers
    GtkListStore *alsa_pluginlist = create_alsa_plugin_list_store();
    GtkWidget *alsa_plugin = gtk_combo_box_new_with_model(GTK_TREE_MODEL(alsa_pluginlist));
    select_active_output_alsa_plugin(alsa_plugin);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), alsa_plugin);
    g_signal_connect(G_OBJECT(alsa_plugin), "changed", G_CALLBACK(select_output_alsa_plugin), NULL);

    // Set rendering
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(alsa_plugin), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(alsa_plugin), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), alsa_plugin, 2, 3, 2, 3, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(alsa_plugin);

    // Device : Output device
    // Create title label
    DEBUG("Configuration output");
    label = gtk_label_new(_("Output"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(label);
    // Set choices of output devices
    GtkListStore *outputlist = create_output_list_store();
    alsa_output = gtk_combo_box_new_with_model(GTK_TREE_MODEL(outputlist));
    select_active_output_audio_device(alsa_output);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), alsa_output);
    g_signal_connect(G_OBJECT(alsa_output), "changed", G_CALLBACK(select_audio_output_device), NULL);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(alsa_output), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(alsa_output), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), alsa_output, 2, 3, 3, 4, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(alsa_output);

    // Device : Input device
    // Create title label
    DEBUG("Configuration input");
    label = gtk_label_new(_("Input"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(label);

    // Set choices of input devices
    GtkListStore *inputlist = create_input_list_store();
    alsa_input = gtk_combo_box_new_with_model(GTK_TREE_MODEL(inputlist));
    select_active_input_audio_device(alsa_input);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), alsa_input);
    g_signal_connect(G_OBJECT(alsa_input), "changed", G_CALLBACK(select_audio_input_device), NULL);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(alsa_input), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(alsa_input), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), alsa_input, 2, 3, 4, 5, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(alsa_input);

    DEBUG("Configuration rintgtone");
    label = gtk_label_new(_("Ringtone"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(label);
    // set choices of ringtone devices
    GtkListStore *ringtonelist = create_output_list_store();
    alsa_ringtone = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ringtonelist));
    select_active_ringtone_audio_device(alsa_ringtone);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), alsa_output);
    g_signal_connect(G_OBJECT(alsa_ringtone), "changed", G_CALLBACK(select_audio_ringtone_device), NULL);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(alsa_ringtone), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(alsa_ringtone), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), alsa_ringtone, 2, 3, 5, 6, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(alsa_ringtone);

    gtk_widget_show_all(alsa_hbox);

    // Update the combo box
    gchar *alsa_pluginname = dbus_get_current_audio_output_plugin();
    update_device_widget(alsa_pluginname, alsa_output, alsa_input, alsa_ringtone);
    g_free(alsa_pluginname);
    return alsa_hbox;
}

static GtkWidget* pulse_box()
{
    GtkWidget *pulse_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_show(pulse_hbox);

    GtkWidget *table = gtk_table_new(6, 3, FALSE);
    gtk_table_set_col_spacing(GTK_TABLE(table), 0, 40);
    gtk_box_pack_start(GTK_BOX(pulse_hbox), table, TRUE, TRUE, 1);
    gtk_widget_show(table);

    // Device : Output device
    // Create title label
    DEBUG("Configuration output");
    GtkWidget *label = gtk_label_new(_("Output"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(label);

    // Set choices of output devices
    GtkListStore *outputlist = create_output_list_store();
    GtkWidget *pulse_output = gtk_combo_box_new_with_model(GTK_TREE_MODEL(outputlist));
    select_active_output_audio_device(pulse_output);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), pulse_output);
    g_signal_connect(G_OBJECT(pulse_output), "changed", G_CALLBACK(select_audio_output_device), NULL);

    // Set rendering
    GtkCellRenderer * renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(pulse_output), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(pulse_output), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), pulse_output, 2, 3, 3, 4, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(pulse_output);

    // Device : Input device
    // Create title label
    DEBUG("Configuration input");
    label = gtk_label_new(_("Input"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(label);

    // Set choices of output devices
    GtkListStore *inputlist = create_input_list_store();
    GtkWidget *pulse_input = gtk_combo_box_new_with_model(GTK_TREE_MODEL(inputlist));
    select_active_input_audio_device(pulse_input);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), pulse_input);
    g_signal_connect(G_OBJECT(pulse_input), "changed", G_CALLBACK(select_audio_input_device), NULL);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(pulse_input), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(pulse_input), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), pulse_input, 2, 3, 4, 5, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(pulse_input);

    DEBUG("Configuration rintgtone");
    label = gtk_label_new(_("Ringtone"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(label);
    // set choices of ringtone devices
    GtkListStore *ringtonelist = create_output_list_store();
    GtkWidget *ringtone = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ringtonelist));
    select_active_ringtone_audio_device(ringtone);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), pulse_output);
    g_signal_connect(G_OBJECT(ringtone), "changed", G_CALLBACK(select_audio_ringtone_device), NULL);

    // Set rendering
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(ringtone), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(ringtone), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(table), ringtone, 2, 3, 5, 6, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(ringtone);

    gtk_widget_show_all(pulse_hbox);

    return pulse_hbox;
}


static void
select_audio_manager(GtkWidget *alsa_button, SFLPhoneClient *client)
{
    if (!must_show_alsa_conf() && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alsa_button))) {
        dbus_set_audio_manager(ALSA_API_STR);
        gtk_container_remove(GTK_CONTAINER(pulse_conf), pulsebox);
        gtk_widget_hide(pulse_conf);

        alsabox = alsa_box();
        gtk_container_add(GTK_CONTAINER(alsa_conf), alsabox);
        gtk_widget_show(alsa_conf);
        gtk_widget_set_sensitive(alsa_conf, TRUE);
        gtk_action_set_sensitive(volumeToggle_, TRUE);
    } else if (must_show_alsa_conf() && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alsa_button))) {
        dbus_set_audio_manager(PULSEAUDIO_API_STR);
        gtk_container_remove(GTK_CONTAINER(alsa_conf), alsabox);
        gtk_widget_hide(alsa_conf);

        pulsebox = pulse_box();
        gtk_container_add(GTK_CONTAINER(pulse_conf), pulsebox);
        gtk_widget_show(pulse_conf);
        gtk_widget_set_sensitive(pulse_conf, TRUE);
        gtk_action_set_sensitive(volumeToggle_, TRUE);

        if (gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(volumeToggle_))) {
            main_window_volume_controls(FALSE);
            g_settings_set_boolean(client->settings, "show-volume-controls", FALSE);
            gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(volumeToggle_), FALSE);
        }

        gtk_action_set_sensitive(volumeToggle_, FALSE);
    } else
        ERROR("Unexpected audio API state");
}

static const gchar *reverse_state(const gchar *state)
{
    if (g_strcmp0(state, "enabled") == 0)
        return "disabled";
    else
        return "enabled";
}

static void
active_noise_suppress(void)
{
    gchar *state = dbus_get_noise_suppress_state();
    dbus_set_noise_suppress_state(reverse_state(state));
    g_free(state);
}

static void
active_echo_cancel(void)
{
    gchar *state = dbus_get_echo_cancel_state();
    dbus_set_echo_cancel_state(reverse_state(state));
    g_free(state);
}

static void
active_is_always_recording(void)
{
    dbus_set_is_always_recording(!dbus_get_is_always_recording());
}

static void restore_recording_path(GtkFileChooser *chooser)
{
    gchar *recording_path = dbus_get_record_path();
    if (recording_path && strlen(recording_path) > 0)
        gtk_file_chooser_set_current_folder(chooser, recording_path);
    g_free(recording_path);
}

static void record_path_changed(GtkFileChooser *chooser, G_GNUC_UNUSED gpointer data)
{
    gchar* path = gtk_file_chooser_get_filename(chooser);
    if (!g_access(path, W_OK)) {
        dbus_set_record_path(path);
    } else {
        ERROR("Directory %s is not writable", path);
        restore_recording_path(chooser);
    }
    g_free(path);
}

GtkWidget* create_audio_configuration(SFLPhoneClient *client)
{
    /* Main widget */
    GtkWidget *audio_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(audio_vbox), 10);

    GtkWidget *frame;
    GtkWidget *table;
    gnome_main_section_new_with_table(_("Sound Manager"), &frame, &table, 1, 4);
    gtk_box_pack_start(GTK_BOX(audio_vbox), frame, FALSE, FALSE, 0);

    gchar *audio_manager = dbus_get_audio_manager();
    const gboolean using_pulse = g_strcmp0(audio_manager, PULSEAUDIO_API_STR) == 0;
    g_free(audio_manager);

    GtkWidget *pulse_button = gtk_radio_button_new_with_mnemonic(NULL , _("_Pulseaudio"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pulse_button), using_pulse);
    gtk_table_attach(GTK_TABLE(table), pulse_button, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL,
                     GTK_EXPAND | GTK_FILL, 0, 0);

    // Box for the Pulse configuration
    pulse_conf = gnome_main_section_new(_("Pulseaudio settings"));
    gtk_box_pack_start(GTK_BOX(audio_vbox), pulse_conf, FALSE, FALSE, 0);
    gtk_widget_show(pulse_conf);

    GtkWidget *alsa_button = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(pulse_button), _("_ALSA"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alsa_button), !using_pulse);
    g_signal_connect(G_OBJECT(alsa_button), "clicked", G_CALLBACK(select_audio_manager), client->settings);
    gtk_table_attach(GTK_TABLE(table), alsa_button, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL,
                     GTK_EXPAND | GTK_FILL, 0, 0);

    // Box for the ALSA configuration
    alsa_conf = gnome_main_section_new(_("ALSA settings"));
    gtk_box_pack_start(GTK_BOX(audio_vbox), alsa_conf, FALSE, FALSE, 0);
    gtk_widget_show(alsa_conf);

    if (must_show_alsa_conf()) {
        // Box for the ALSA configuration
        alsabox = alsa_box();
        gtk_container_add(GTK_CONTAINER(alsa_conf), alsabox);
        gtk_widget_hide(alsa_conf);
    } else {
        pulsebox = pulse_box();
        gtk_container_add(GTK_CONTAINER(pulse_conf), pulsebox);
        gtk_widget_hide(pulse_conf);
    }

    gnome_main_section_new_with_table(_("Recordings"), &frame, &table, 2, 3);
    gtk_box_pack_start(GTK_BOX(audio_vbox), frame, FALSE, FALSE, 0);

    // label
    GtkWidget *label = gtk_label_new(_("Destination folder"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
                     0, 5);

    // folder chooser button
    GtkWidget *folderChooser = gtk_file_chooser_button_new(_("Select a folder"),
                                                           GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    /* Get the path where to save audio files */
    restore_recording_path(GTK_FILE_CHOOSER(folderChooser));

    g_signal_connect(G_OBJECT(folderChooser) , "selection-changed", G_CALLBACK(record_path_changed),
                     NULL);
    gtk_table_attach(GTK_TABLE(table), folderChooser, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL,
                     GTK_EXPAND | GTK_FILL, 0, 5);

    // isAlwaysRecording functionality checkbox
    gboolean isAlwaysRecording = dbus_get_is_always_recording();
    GtkWidget *enableIsAlwaysRecording = gtk_check_button_new_with_mnemonic(_("_Always recording"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableIsAlwaysRecording), isAlwaysRecording);
    g_signal_connect(G_OBJECT(enableIsAlwaysRecording), "clicked", active_is_always_recording, NULL);
    gtk_table_attach(GTK_TABLE(table), enableIsAlwaysRecording, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL,
                     GTK_EXPAND | GTK_FILL, 0, 5);
    gtk_widget_show(GTK_WIDGET(enableIsAlwaysRecording));

    // Box for the voice enhancement configuration
    gnome_main_section_new_with_table(_("Voice enhancement settings"), &frame, &table, 2, 1);
    gtk_box_pack_start(GTK_BOX(audio_vbox), frame, FALSE, FALSE, 0);

    GtkWidget *enableNoiseReduction = gtk_check_button_new_with_mnemonic(_("_Noise Reduction"));
    gchar *state = dbus_get_noise_suppress_state();

    if (g_strcmp0(state, "enabled") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableNoiseReduction), TRUE);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableNoiseReduction), FALSE);

    g_free(state);
    state = NULL;

    g_signal_connect(G_OBJECT(enableNoiseReduction), "clicked", active_noise_suppress, NULL);
    gtk_table_attach(GTK_TABLE(table), enableNoiseReduction, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL,
                     GTK_EXPAND | GTK_FILL, 0, 0);

    GtkWidget *enableEchoCancel = gtk_check_button_new_with_mnemonic(_("_Echo Cancellation"));
    state = dbus_get_echo_cancel_state();

    if (g_strcmp0(state, "enabled") == 0)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableEchoCancel), TRUE);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableEchoCancel), FALSE);

    g_free(state);

    g_signal_connect(G_OBJECT(enableEchoCancel), "clicked", active_echo_cancel, NULL);
    gtk_table_attach(GTK_TABLE(table), enableEchoCancel, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL,
                     GTK_EXPAND | GTK_FILL, 0, 0);

    gtk_widget_show_all(audio_vbox);

    if (!using_pulse)
        gtk_widget_show(alsa_conf);
    else
        gtk_widget_hide(alsa_conf);

    return audio_vbox;
}

/** Show/Hide the alsa configuration panel */
gboolean must_show_alsa_conf()
{
    gchar *api = dbus_get_audio_manager();
    int ret = g_strcmp0(api, ALSA_API_STR);
    g_free(api);
    return ret == 0;
}

gboolean
must_show_volume(SFLPhoneClient *client)
{
    return g_settings_get_boolean(client->settings, "show-volume-controls") && must_show_alsa_conf();
}
