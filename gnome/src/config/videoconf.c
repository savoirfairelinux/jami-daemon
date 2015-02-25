/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
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

#include <glib/gi18n.h>
#include <string.h>
#include "videoconf.h"
#include "utils.h"
#include "dbus.h"
#include "codeclist.h"
#include "video/video_capabilities.h"
#include "video/video_widget.h"

#define KBPS "kbps"

typedef struct {
    VideoCapabilities *cap;

    /* Video choices */
    gchar *name;
    gchar *chan;
    gchar *size;
    gchar *rate;
} VideoDevice;

static GtkComboBoxText *v4l2Device;
static GtkComboBoxText *v4l2Channel;
static GtkComboBoxText *v4l2Size;
static GtkComboBoxText *v4l2Rate;

static GtkWidget *v4l2_hbox;
static GtkWidget *v4l2_nodev;

static GtkWidget *camera_button;

static GtkWidget *codecTreeView; // View used instead of store to get access to selection
static GtkWidget *codecMoveUpButton;
static GtkWidget *codecMoveDownButton;

// Codec properties ID
enum {
    COLUMN_CODEC_ACTIVE,
    COLUMN_CODEC_NAME,
    COLUMN_CODEC_BITRATE,
    COLUMN_CODEC_PARAMETERS,
    CODEC_COLUMN_COUNT
};


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

static const gchar *const CAMERA_START_STR = "_Start";
static const gchar *const CAMERA_STOP_STR = "_Stop";

static void
camera_button_toggled(GtkButton *button, G_GNUC_UNUSED gpointer data)
{
    gchar ** str = dbus_get_call_list();

    /* we can toggle only if there is no call */
    if (str == NULL || *str == NULL) {

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
            dbus_start_video_camera();
        } else {
            dbus_stop_video_camera();
        }

    }

    g_strfreev(str);

    update_camera_button_label();
}

void
set_camera_button_sensitivity(gboolean sensitive)
{
    if (!camera_button || !GTK_IS_WIDGET(camera_button))
        return;
    g_debug("%ssetting camera button", sensitive ? "" : "Un");
    gtk_widget_set_sensitive(GTK_WIDGET(camera_button), sensitive);
}

void
update_camera_button_label()
{
    if (!camera_button || !GTK_IS_WIDGET(camera_button))
        return;

    GtkToggleButton *button = GTK_TOGGLE_BUTTON(camera_button);
    if (dbus_has_video_camera_started()) {
        /* We call g_object_set to avoid triggering the "toggled" signal */
        gtk_button_set_label(GTK_BUTTON(button), _(CAMERA_STOP_STR));
        g_object_set(button, "active", TRUE, NULL);
    } else {
        gtk_button_set_label(GTK_BUTTON(button), _(CAMERA_START_STR));
        g_object_set(button, "active", FALSE, NULL);
    }
}

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
        g_debug("Account is NULL, using global codec list");
    } else {
        list = account->allCodecs;
    }

    // Insert codecs
    if ( ! list )
        return;

    for (size_t i = 0; i < list->length; ++i) {
        codec_t *c = g_queue_peek_nth(list, i);

        if ( c && g_strcmp0(c->type, "VIDEO") == 0) {
            g_debug("%s is %sactive", c->name, c->is_active ? "" : "not ");
            GtkTreeIter iter;
            gtk_list_store_append(codecStore, &iter);
            gchar *bitrate = g_strdup_printf("%s " KBPS, c->bitrate);

            gtk_list_store_set(codecStore, &iter,
                               COLUMN_CODEC_ACTIVE, c->is_active,
                               COLUMN_CODEC_NAME, c->name,
                               COLUMN_CODEC_BITRATE, bitrate,
                               -1);
            g_free(bitrate);
        }
    }
}


/* ebail: disable */
#if 0
static void
bitrate_edited_cb(G_GNUC_UNUSED GtkCellRenderer *renderer, gchar *path, gchar *new_text, gpointer data)
{
    // Retrieve userdata
    account_t *acc = (account_t*) data;

    if (!acc) {
        g_warning("No account selected");
        return;
    }
    g_debug("updating bitrate for %s", acc->accountID);
    // Get active value and name at iteration
    const gint base = 10;
    gchar *endptr;
    const long long val = strtoll(new_text, &endptr, base);
    /* Ignore if it's not a number */
    if (*endptr != '\0') {
        g_warning("Ignoring %lld characters %s\n", val, endptr);
    } else if (val < 0) {
        g_warning("Ignoring negative bitrate value");
    } else {
        // Get path of edited codec
        GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(codecTreeView));
        GtkTreeIter iter;
        gtk_tree_model_get_iter(model, &iter, tree_path);
        gtk_tree_path_free(tree_path);
        gchar *name = NULL;
        gtk_tree_model_get(model, &iter, COLUMN_CODEC_NAME, &name, -1);

        GArray *vcodecs = dbus_video_codec_list(acc->accountID);
        if (!vcodecs)
            return;

        gchar *bitrate = g_strdup_printf("%llu", val);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_CODEC_BITRATE, bitrate, -1);
        /* ebail : disable for the moment - we do not use GHashTable anymore
        GHashTable *codec = video_codec_list_get_by_name(vcodecs, name);
        if (codec) {
            g_debug("Setting new bitrate %s for %s", bitrate, name);
            video_codec_set_bitrate(codec, bitrate);
            dbus_set_video_codecs(acc->accountID, vcodecs);
        } else {
            g_warning("Could not find codec %s", name);
        }
        */
        g_free(bitrate);
        g_ptr_array_free(vcodecs, TRUE);
    }
}


static void
parameters_edited_cb(G_GNUC_UNUSED GtkCellRenderer *renderer, gchar *path, gchar *new_text, gpointer data)
{
    account_t *acc = (account_t*) data;

    if (!acc) {
        g_warning("No account selected");
        return;
    }

    if (strlen(new_text) == 0)
        return;

    // Get path of edited codec
    GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(codecTreeView));
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, tree_path);
    gtk_tree_path_free(tree_path);
    gchar *name = NULL;
    gtk_tree_model_get(model, &iter, COLUMN_CODEC_NAME, &name, -1);

    GArray *vcodecs = dbus_video_codec_list(acc->accountID);
    if (!vcodecs)
        return;
    /* ebail : disable for the moment - we do not use GHashTable anymore
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_CODEC_PARAMETERS, new_text, -1);

    GHashTable *codec = video_codec_list_get_by_name(vcodecs, name);
    if (codec) {
        g_debug("Setting new parameters \"%s\" for %s", new_text, name);
        video_codec_set_parameters(codec, new_text);
        dbus_set_video_codecs(acc->accountID, vcodecs);
    } else {
        g_warning("Could not find codec %s", name);
    }
    g_ptr_array_free(vcodecs, TRUE);
    */
}
#endif

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
        g_warning("no account selected");
        return;
    }

    // Get active value and name at iteration
    gboolean active;
    gchar* name;
    gtk_tree_model_get(model, &iter, COLUMN_CODEC_ACTIVE, &active,
                       COLUMN_CODEC_NAME, &name, -1);

    g_debug("Selected Codec: %s", name);
    printf("Selected Codec: %s", name);

    codec_t* codec = NULL;

    codec = codec_list_get_by_name((gconstpointer) name, acc->allCodecs);

    // Toggle active value
    active = !active;

    // Store value
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COLUMN_CODEC_ACTIVE, active,
                       -1);

    gtk_tree_path_free(treePath);

    // Modify codec queue to represent change
    if (codec) {
        printf("ELOI: call to video_codec_set_active \n");
        codec_set_active(codec, active);
    }
    printf("ELOI < codec_active_toggled \n");
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
            codec_list_move_codec_up(indice, &acc->allCodecs);
        else
            codec_list_move_codec_down(indice, &acc->allCodecs);
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
videocodecs_box(const account_t *account)
{
    GtkWidget *videocodecs_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(videocodecs_hbox), 10);

    GtkWidget *scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_SHADOW_IN);

    gtk_box_pack_start(GTK_BOX(videocodecs_hbox), scrolledWindow, TRUE, TRUE, 0);
    GtkListStore *codecStore = gtk_list_store_new(CODEC_COLUMN_COUNT,
                                                  G_TYPE_BOOLEAN, // Active
                                                  G_TYPE_STRING,  // Name
                                                  G_TYPE_STRING,  // Bit rate
                                                  G_TYPE_STRING   // Bandwith
                                                  );

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

    // Bandwith column
    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes(_("Bitrate"), renderer, "text", COLUMN_CODEC_BITRATE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);

    gtk_container_add(GTK_CONTAINER(scrolledWindow), codecTreeView);

    // Create button box
    GtkWidget *buttonBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(buttonBox), 10);
    gtk_box_pack_start(GTK_BOX(videocodecs_hbox), buttonBox, FALSE, FALSE, 0);

    codecMoveUpButton = gtk_button_new_with_label(_("Up"));
    gtk_widget_set_sensitive(GTK_WIDGET(codecMoveUpButton), FALSE);
    gtk_box_pack_start(GTK_BOX(buttonBox), codecMoveUpButton, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(codecMoveUpButton), "clicked",
                     G_CALLBACK(codec_move_up), (gpointer) account);

    codecMoveDownButton = gtk_button_new_with_label(_("Down"));
    gtk_widget_set_sensitive(GTK_WIDGET(codecMoveDownButton), FALSE);
    gtk_box_pack_start(GTK_BOX(buttonBox), codecMoveDownButton, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(codecMoveDownButton), "clicked",
                     G_CALLBACK(codec_move_down), (gpointer) account);

    fill_codec_list(account);

    return videocodecs_hbox;
}


static void
reset_combo_box(GtkComboBoxText *combo, gchar **entries, const gchar *preferred)
{
    g_assert(GTK_IS_WIDGET(combo));

    guint index = 0; /* first one if not found */

    /* Temporarily deactivate the "changed" signal to clear the list */
    const guint signal_id = g_signal_lookup("changed", G_OBJECT_TYPE(combo));
    const gulong handler_id = g_signal_handler_find(combo, G_SIGNAL_MATCH_ID, signal_id, 0, NULL, NULL, NULL);
    g_signal_handler_block(combo, handler_id);
    gtk_combo_box_text_remove_all(combo);
    g_signal_handler_unblock(combo, handler_id);

    for (guint i = 0; entries[i]; ++i) {
        gtk_combo_box_text_insert_text(combo, i, entries[i]);
        if (g_strcmp0(entries[i], preferred) == 0)
            index = i;
    }

    /* NOTE This will invoke the combo box's "changed" callback */
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), index);
}

static void
fill_devices(void)
{
    gchar **devices = dbus_video_get_device_list();
    g_assert(devices);

    if (*devices) {
        gchar *default_device = dbus_video_get_default_device();
        reset_combo_box(v4l2Device, devices, default_device);
        g_free(default_device);

        gtk_widget_show_all(v4l2_hbox);
        gtk_widget_hide(v4l2_nodev);
        gtk_widget_set_sensitive(camera_button, TRUE);
    } else if (GTK_IS_WIDGET(v4l2_hbox)) {
        g_warning("No video devices");
        gtk_widget_hide(v4l2_hbox);
        gtk_widget_show(v4l2_nodev);
        gtk_widget_set_sensitive(camera_button, FALSE);
    }

    g_strfreev(devices);
}

static void
combo_box_device_changed_cb(G_GNUC_UNUSED GtkComboBoxText* combo, gpointer data)
{
    VideoDevice *dev = (VideoDevice *) data;
    gchar *name = gtk_combo_box_text_get_active_text(v4l2Device);
    g_assert(name);

    /* Clear the video device */
    if (dev->cap)
        video_capabilities_free(dev->cap);
    g_free(dev->name);
    g_free(dev->chan);
    g_free(dev->size);
    g_free(dev->rate);

    dev->name = name;
    dev->cap = video_capabilities_new(name);

    /* Fetch user preferences for this device */
    GHashTable *hash = dbus_video_get_settings(name);
    dev->chan = g_strdup(g_hash_table_lookup(hash, "channel"));
    dev->size = g_strdup(g_hash_table_lookup(hash, "size"));
    dev->rate = g_strdup(g_hash_table_lookup(hash, "rate"));
    g_hash_table_destroy(hash);

    /* Start the cascade into the channel combo box */
    gchar **channels = video_capabilities_get_channels(dev->cap);
    reset_combo_box(v4l2Channel, channels, dev->chan);
    g_strfreev(channels);
}

static void
combo_box_channel_changed_cb(G_GNUC_UNUSED GtkComboBoxText *combo, gpointer data)
{
    VideoDevice *dev = (VideoDevice *) data;
    gchar *chan = gtk_combo_box_text_get_active_text(v4l2Channel);
    g_assert(chan);
    g_free(dev->chan);
    dev->chan = chan;

    /* Cascade into the size combo box */
    gchar** sizes = video_capabilities_get_sizes(dev->cap, dev->chan);
    reset_combo_box(v4l2Size, sizes, dev->size);
    g_strfreev(sizes);
}

static void
combo_box_size_changed_cb(G_GNUC_UNUSED GtkComboBoxText *combo, gpointer data)
{
    VideoDevice *dev = (VideoDevice *) data;
    gchar *size = gtk_combo_box_text_get_active_text(v4l2Size);
    g_assert(size);
    g_free(dev->size);
    dev->size = size;

    /* Cascade into the rate combo box */
    gchar **rates = video_capabilities_get_rates(dev->cap, dev->chan, dev->size);
    reset_combo_box(v4l2Rate, rates, dev->rate);
    g_strfreev(rates);
}

static void
combo_box_rate_changed_cb(G_GNUC_UNUSED GtkComboBoxText *combo, gpointer data)
{
    VideoDevice *dev = (VideoDevice *) data;
    gchar *rate = gtk_combo_box_text_get_active_text(v4l2Rate);
    g_assert(rate);
    g_free(dev->rate);
    dev->rate = rate;

    /* End of the cascade, save the default device and its settings */
    dbus_video_set_default_device(dev->name);
    GHashTable *hash = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(hash, "channel", dev->chan);
    g_hash_table_insert(hash, "size", dev->size);
    g_hash_table_insert(hash, "rate", dev->rate);
    dbus_video_apply_settings(dev->name, hash);
    g_hash_table_destroy(hash);
}

void
video_device_event_cb(G_GNUC_UNUSED DBusGProxy *proxy, G_GNUC_UNUSED gpointer foo)
{
    /* FIXME: get rid of these global widgets */
    g_return_if_fail(GTK_IS_WIDGET(v4l2Device));
    fill_devices();
}

static GtkComboBoxText *
attach_combo_box(GtkGrid *grid, const guint row, const gchar *label, GCallback callback, gpointer data)
{
    /* Attach the label on left */
    GtkWidget *item = gtk_label_new(_(label));
    gtk_grid_attach(grid, item, 0, row, 1, 1);
    gtk_widget_set_halign(item, GTK_ALIGN_START);

    /* Attach the combo combo on right */
    GtkWidget *combo = gtk_combo_box_text_new();
    g_signal_connect(G_OBJECT(combo), "changed", callback, data);
    gtk_grid_attach(grid, combo, 1, row, 1, 1);

    return GTK_COMBO_BOX_TEXT(combo);
}

static GtkWidget *
v4l2_box()
{
    GtkWidget *ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    v4l2_nodev = gtk_label_new(_("No devices found"));
    v4l2_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    gtk_box_pack_start(GTK_BOX(ret), v4l2_hbox , TRUE , TRUE , 0);
    gtk_box_pack_start(GTK_BOX(ret), v4l2_nodev, TRUE , TRUE , 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 40);
    gtk_box_pack_start(GTK_BOX(v4l2_hbox), grid, TRUE, TRUE, 1);

    VideoDevice *dev = g_new0(VideoDevice, 1);

    v4l2Device = attach_combo_box(GTK_GRID(grid), 0, "Device", G_CALLBACK(combo_box_device_changed_cb), dev);
    v4l2Channel = attach_combo_box(GTK_GRID(grid), 1, "Channel", G_CALLBACK(combo_box_channel_changed_cb), dev);
    v4l2Size = attach_combo_box(GTK_GRID(grid), 2, "Size", G_CALLBACK(combo_box_size_changed_cb), dev);
    v4l2Rate = attach_combo_box(GTK_GRID(grid), 3, "Rate", G_CALLBACK(combo_box_rate_changed_cb), dev);

    return ret;
}

GtkWidget *
create_video_configuration(SFLPhoneClient *client)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget *frame, *grid;
    gnome_main_section_new_with_grid(_("Video4Linux2"), &frame, &grid);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    GtkWidget *v4l2box = v4l2_box();
    gtk_grid_attach(GTK_GRID(grid), v4l2box, 0, 1, 1, 1);

    frame = gnome_main_section_new(_("Camera"));
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
    GtkWidget *vbox_camera = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_camera), 10);
    gtk_container_add(GTK_CONTAINER(frame), vbox_camera);

    const gboolean started = dbus_has_video_camera_started();

    camera_button = gtk_toggle_button_new_with_mnemonic(started ? _(CAMERA_STOP_STR) : _(CAMERA_START_STR));
    gtk_widget_set_size_request(camera_button, 80, 30);
    gtk_box_pack_start(GTK_BOX(vbox_camera), camera_button, FALSE, FALSE, 0);
    gtk_widget_set_halign(camera_button, GTK_ALIGN_START);

    GtkWidget* preview_frame = gtk_frame_new(_("Preview"));
    gtk_frame_set_shadow_type(GTK_FRAME(preview_frame), GTK_SHADOW_IN);
    /* alight label to the right */
    gtk_frame_set_label_align(GTK_FRAME(preview_frame), 1.0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox_camera), preview_frame, TRUE, TRUE, 0);

    if (started)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(camera_button), TRUE);
    g_signal_connect(G_OBJECT(camera_button), "toggled",
                     G_CALLBACK(camera_button_toggled), NULL);

    gboolean active_call = FALSE;
    gchar **list = dbus_get_call_list();

    if (list && *list) {
        active_call = TRUE;
        g_strfreev(list);
    }

    if (active_call) {
        /* active call, so don't allow preview */
        gtk_widget_set_sensitive(GTK_WIDGET(camera_button), FALSE);
    } else {
        /* allow preview, and put videw widget in preview container
         * when the container is deleted, make sure to move the video widget
         */
        g_signal_connect_swapped(G_OBJECT(preview_frame), "destroy", G_CALLBACK(video_widget_move_to_window), client->video);
        video_widget_move_to_preview(client->video, GTK_CONTAINER(preview_frame));
    }

    gtk_widget_show_all(vbox);

    /* get devices list from daemon *after* showing all widgets
     * that way we can show either the list, either the "no devices found" label
     */
    fill_devices();

    return vbox;
}
