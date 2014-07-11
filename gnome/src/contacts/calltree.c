/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "calllist.h"
#include "calltree.h"
#include "str_utils.h"
#include "account_schema.h"
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <gtk/gtk.h>

#include "dbus.h"
#include "calltab.h"
#include "conferencelist.h"
#include "mainwindow.h"
#include "history.h"
#include "calltree.h"
#include "uimanager.h"
#include "actions.h"
#include "searchbar.h"
#include "sflphone_client.h"

#ifdef SFL_PRESENCE
#include "presencewindow.h"
#include "presence.h"
#endif

#if !GLIB_CHECK_VERSION(2, 30, 0)
#define G_VALUE_INIT  { 0, { { 0 } } }
#endif

typedef struct {
    gchar *source_ID;
    gchar *dest_ID;
    SFLPhoneClient *client;
} PopupData;

static PopupData *popup_data = NULL;

#define SFL_CREATE_CONFERENCE _("Create conference")
#define SFL_TRANSFER_CALL _("Transfer call to")

static GtkWidget *calltree_popupmenu = NULL;
static GtkWidget *calltree_menu_items = NULL;

static void drag_data_received_cb(GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *, guint, guint, SFLPhoneClient *client);
static void menuitem_response(gchar * string);

static GtkTargetEntry target_list[] = {
    { "MY_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, 0 }
};

static const guint n_targets = G_N_ELEMENTS(target_list);

/**
 * Show popup menu
 */
static gboolean
popup_menu(GtkWidget *widget,
           gpointer data)
{
    show_popup_menu(widget, NULL, data);
    return TRUE;
}

/* Returns TRUE if row contains a conference object pointer */
gboolean
is_conference(GtkTreeModel *model, GtkTreeIter *iter)
{
    gboolean result = FALSE;
    gtk_tree_model_get(model, iter, COLUMN_IS_CONFERENCE, &result, -1);
    return result;
}

static void
update_ringtone_seekslider_path(callable_obj_t *call)
{
    main_window_update_seekslider(call->_recordfile);
}

/* Call back when the user click on a call in the list */
static void
call_selected_cb(GtkTreeSelection *sel, SFLPhoneClient *client)
{
    GtkTreeModel *model = gtk_tree_view_get_model(gtk_tree_selection_get_tree_view(sel));

    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(sel, &model, &iter))
        return;

    if (calltab_has_name(active_calltree_tab, HISTORY))
        main_window_reset_playback_scale();

    /* Get ID of selected object, may be a call or a conference */
    gchar *id;
    gtk_tree_model_get(model, &iter, COLUMN_ID, &id, -1);

    if (is_conference(model, &iter)) {
        g_debug("Selected a conference");

        conference_obj_t *calltree_selected_conf = conferencelist_get(active_calltree_tab, id);
        g_free(id);

        if (calltree_selected_conf)
            calltab_select_conf(active_calltree_tab, calltree_selected_conf);
    } else {
        g_debug("Selected a call");

        callable_obj_t *selected_call = calllist_get_call(active_calltree_tab, id);
        g_free(id);

        if (selected_call) {
            calltab_select_call(active_calltree_tab, selected_call);
            if (calltab_has_name(active_calltree_tab, HISTORY))
                update_ringtone_seekslider_path(selected_call);
        }
    }

    update_actions(client);
}

/* A row is activated when it is double clicked */
static void
row_activated_cb(G_GNUC_UNUSED GtkTreeView *tree_view,
                 G_GNUC_UNUSED GtkTreePath *path,
                 G_GNUC_UNUSED GtkTreeViewColumn *column,
                 SFLPhoneClient *client)
{
    if (calltab_get_selected_type(active_calltree_tab) == A_CALL) {
        g_debug("Selected a call");
        callable_obj_t *selectedCall = calltab_get_selected_call(active_calltree_tab);

        if (selectedCall) {
            // Get the right event from the right calltree
            if (calltab_has_name(active_calltree_tab, CURRENT_CALLS)) {
                switch (selectedCall->_state) {
                    case CALL_STATE_INCOMING:
                        dbus_accept(selectedCall);
                        break;
                    case CALL_STATE_HOLD:
                        dbus_unhold(selectedCall);
                        break;
                    case CALL_STATE_RINGING:
                    case CALL_STATE_CURRENT:
                    case CALL_STATE_BUSY:
                    case CALL_STATE_FAILURE:
                        break;
                    case CALL_STATE_DIALING:
                        sflphone_place_call(selectedCall, client);
                        break;
                    default:
                        g_warning("Row activated - Should not happen!");
                        break;
                }
            } else {
                // If history or contact: double click action places a new call
                callable_obj_t* new_call = create_new_call(CALL, CALL_STATE_DIALING, "", selectedCall->_accountID, selectedCall->_display_name, selectedCall->_peer_number);

                calllist_add_call(current_calls_tab, new_call);
                calltree_add_call(current_calls_tab, new_call, NULL);
                // Function sflphone_place_call (new_call) is processed in process_dialing
                sflphone_place_call(new_call, client);
                calltree_display(current_calls_tab, client);
            }
        }
    } else if (calltab_get_selected_type(active_calltree_tab) == A_CONFERENCE) {
        g_debug("Selected a conference");

        if (calltab_has_name(active_calltree_tab, CURRENT_CALLS)) {
            conference_obj_t * selectedConf = calltab_get_selected_conf(current_calls_tab);

            if (selectedConf) {

                switch (selectedConf->_state) {
                    case CONFERENCE_STATE_ACTIVE_DETACHED:
                    case CONFERENCE_STATE_ACTIVE_DETACHED_RECORD:
                        sflphone_add_main_participant(selectedConf);
                        break;
                    case CONFERENCE_STATE_HOLD:
                    case CONFERENCE_STATE_HOLD_RECORD:
                        dbus_unhold_conference(selectedConf);
                        break;
                    case CONFERENCE_STATE_ACTIVE_ATTACHED:
                    case CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD:
                    default:
                        break;
                }
            }
        } else
            g_warning("Selected a conference in history, should not be possible");
    }
}

/* Catch cursor-activated signal. That is, when the entry is single clicked */
static void
row_single_click(G_GNUC_UNUSED GtkTreeView *tree_view, SFLPhoneClient *client)
{
    gchar * displaySasOnce = NULL;

    if (calllist_empty(active_calltree_tab))
        return;

    callable_obj_t *selectedCall = calltab_get_selected_call(active_calltree_tab);
    conference_obj_t *selectedConf = calltab_get_selected_conf(active_calltree_tab);

    if (calltab_has_name(active_calltree_tab, CURRENT_CALLS))
        g_debug("Active calltree is current_calls");
    else if (calltab_has_name(active_calltree_tab, HISTORY))
        g_debug("Active calltree is history");

    if (calltab_get_selected_type(active_calltree_tab) == A_CALL) {

        if (selectedCall) {
            account_t *account_details = account_list_get_by_id(selectedCall->_accountID);
            g_debug("AccountID %s", selectedCall->_accountID);

            if (account_details != NULL) {
                displaySasOnce = g_hash_table_lookup(account_details->properties, CONFIG_ZRTP_DISPLAY_SAS_ONCE);
                g_debug("Display SAS once %s", displaySasOnce);
            } else {
                GHashTable *properties = sflphone_get_ip2ip_properties();

                if (properties != NULL) {
                    displaySasOnce = g_hash_table_lookup(properties, CONFIG_ZRTP_DISPLAY_SAS_ONCE);
                    g_debug("IP2IP displaysasonce %s", displaySasOnce);
                }
            }

            /*  Make sure that we are not in the history tab since
             *  nothing is defined for it yet
             */
            if (calltab_has_name(active_calltree_tab, CURRENT_CALLS)) {
                switch (selectedCall->_srtp_state) {
                    case SRTP_STATE_ZRTP_SAS_UNCONFIRMED:
                        selectedCall->_srtp_state = SRTP_STATE_ZRTP_SAS_CONFIRMED;

                        if (utf8_case_equal(displaySasOnce, "true"))
                            selectedCall->_zrtp_confirmed = TRUE;

                        dbus_confirm_sas(selectedCall);
                        calltree_update_call(current_calls_tab, selectedCall, client);
                        break;
                    case SRTP_STATE_ZRTP_SAS_CONFIRMED:
                        selectedCall->_srtp_state = SRTP_STATE_ZRTP_SAS_UNCONFIRMED;
                        dbus_reset_sas(selectedCall);
                        calltree_update_call(current_calls_tab, selectedCall, client);
                        break;
                    default:
                        g_debug("Single click but no action");
                        break;
                }
            }
        }
    } else if (calltab_get_selected_type(active_calltree_tab) == A_CONFERENCE) {
        if (selectedConf)
            g_debug("There is actually a selected conf");
    } else
        g_warning("Unknown selection type");
}

static gboolean
button_pressed(GtkWidget* widget, GdkEventButton *event, SFLPhoneClient *client)
{
    if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    if (calltab_has_name(active_calltree_tab, CURRENT_CALLS))
        show_popup_menu(widget, event, client);
    else if (calltab_has_name(active_calltree_tab, HISTORY))
        show_popup_menu_history(widget, event, client);
    else
        show_popup_menu_contacts(widget, event, client);

    return TRUE;
}

static gchar *clean_display_number(gchar *name)
{
    const gchar SIP_PREFIX[] = "<sip:";
    const gchar SIPS_PREFIX[] = "<sips:";
    if (g_str_has_prefix(name, SIP_PREFIX))
        name += (sizeof(SIP_PREFIX) - 1);
    else if (g_str_has_prefix(name, SIPS_PREFIX))
        name += (sizeof(SIPS_PREFIX) - 1);

    gchar * pos = g_strrstr(name, ">");
    if (pos)
        *pos = '\0';
    return name;
}

static gchar *
calltree_display_call_info(callable_obj_t * call, CallDisplayType display_type)
{
    gchar display_number[strlen(call->_peer_number) + 1];
    strcpy(display_number, call->_peer_number);

    if (call->_type != CALL || !call_was_outgoing(call)) {
        // Get the hostname for this call (NULL if not existent)
        gchar * hostname = g_strrstr(call->_peer_number, "@");

        // Test if we are dialing a new number
        if (*call->_peer_number && hostname)
            display_number[hostname - call->_peer_number] = '\0';
    }

    // Different display depending on type
    gchar *name = NULL;
    gchar *details = NULL;

    if (*call->_display_name) {
        name = call->_display_name;
        details = display_number;
    } else {
        name = display_number;
        name = clean_display_number(name);
        details = "";
    }

    gchar *desc = g_markup_printf_escaped("<b>%s</b>   <i>%s</i>   ", name, details);
    gchar *suffix = NULL;

    switch (display_type) {
        case DISPLAY_TYPE_CALL:
            if (call->_state_code)
                suffix = g_markup_printf_escaped("\n<i>%s (%d)</i>", call->_state_code_description, call->_state_code);
            break;

            if (call->_state_code)
                suffix = g_markup_printf_escaped("\n<i>%s (%d)</i>",
                                                 call->_state_code_description, call->_state_code);
            break;
        case DISPLAY_TYPE_CALL_TRANSFER:
            suffix = g_markup_printf_escaped(_("\n<i>Transfer to:%s</i> "), call->_trsft_to);
            break;
        case DISPLAY_TYPE_SAS:
            suffix = g_markup_printf_escaped(_("\n<i>Confirm SAS <b>%s</b> ?</i>"), call->_sas);
            break;
        case DISPLAY_TYPE_HISTORY :
        default:
            break;
    }

    gchar *msg = g_strconcat(desc, suffix, NULL);
    g_free(desc);
    g_free(suffix);
    return msg;
}

#ifdef SFL_PRESENCE
void on_call_drag_data_get(GtkWidget *widget,
        G_GNUC_UNUSED GdkDragContext *drag_context,
        GtkSelectionData *sdata,
        G_GNUC_UNUSED guint info,
        G_GNUC_UNUSED guint time_,
        G_GNUC_UNUSED gpointer user_data)
{
    GtkTreeSelection *selector = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    if(!gtk_tree_selection_get_selected(selector, NULL, NULL))
        return;

    callable_obj_t * c = calltab_get_selected_call(active_calltree_tab);
    buddy_t * b = presence_buddy_create();

    presence_callable_to_buddy(c, b);

    g_debug("Drag src from calltree: b->uri : %s",b->uri);

    gtk_selection_data_set(sdata,
            gdk_atom_intern ("struct buddy_t pointer", FALSE),
            8,           // Tell GTK how to pack the data (bytes)
            (void *)&b,  // The actual pointer that we just made
            sizeof (b)); // The size of the pointer
}
#endif

void
calltree_create(calltab_t* tab, gboolean has_searchbar, SFLPhoneClient *client)
{
    tab->tree = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    // Fix bug #708 (resize)
    gtk_widget_set_size_request(tab->tree,100,80);

    gtk_container_set_border_width(GTK_CONTAINER(tab->tree), 0);

    GtkWidget *calltree_sw = gtk_scrolled_window_new(NULL, NULL);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(calltree_sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(calltree_sw), GTK_SHADOW_IN);

    tab->store = gtk_tree_store_new(COLUMNS_IN_TREE_STORE,
                                    GDK_TYPE_PIXBUF, /* Icon */
                                    G_TYPE_STRING,   /* Description */
                                    GDK_TYPE_PIXBUF, /* Security Icon */
                                    G_TYPE_STRING,   /* ID of the object */
                                    G_TYPE_BOOLEAN   /* True if this is conference */
                                   );

    // For history, we want to associate the model to the view, after we've inserted
    // all the calls into the model, otherwise it will slow down application startup
    if (calltab_has_name(tab, HISTORY))
        tab->view = gtk_tree_view_new();
    else
        tab->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tab->store));

    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tab->view), FALSE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tab->view), FALSE);
    g_signal_connect(G_OBJECT(tab->view), "row-activated",
                     G_CALLBACK(row_activated_cb),
                     client);

    gtk_widget_set_can_focus(calltree_sw, TRUE);
    gtk_widget_grab_focus(calltree_sw);

    g_signal_connect(G_OBJECT(tab->view), "cursor-changed",
                     G_CALLBACK(row_single_click),
                     client);

    // Connect the popup menu
    g_signal_connect(G_OBJECT(tab->view), "popup-menu",
                     G_CALLBACK(popup_menu),
                     client);
    g_signal_connect(G_OBJECT(tab->view), "button-press-event",
                     G_CALLBACK(button_pressed),
                     client);

#ifdef SFL_PRESENCE
    gtk_drag_source_set(tab->view, GDK_BUTTON1_MASK,
         &presence_drag_targets, 1, GDK_ACTION_COPY|GDK_ACTION_MOVE);
    g_signal_connect(tab->view, "drag-data-get", G_CALLBACK(on_call_drag_data_get), NULL);
#endif

    if (calltab_has_name(tab, CURRENT_CALLS)) {

        gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(tab->view), GDK_BUTTON1_MASK, target_list, n_targets, GDK_ACTION_DEFAULT | GDK_ACTION_MOVE);
        gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(tab->view), target_list, n_targets, GDK_ACTION_DEFAULT);
        // destination widget drag n drop signals
        g_signal_connect(G_OBJECT(tab->view), "drag_data_received", G_CALLBACK(drag_data_received_cb), client);

        calltree_popupmenu = gtk_menu_new();

        calltree_menu_items = gtk_menu_item_new_with_label(SFL_TRANSFER_CALL);
        g_signal_connect_swapped(calltree_menu_items, "activate",
                                 G_CALLBACK(menuitem_response), g_strdup(SFL_TRANSFER_CALL));
        gtk_menu_shell_append(GTK_MENU_SHELL(calltree_popupmenu), calltree_menu_items);
        gtk_widget_show(calltree_menu_items);

        calltree_menu_items = gtk_menu_item_new_with_label(SFL_CREATE_CONFERENCE);
        g_signal_connect_swapped(calltree_menu_items, "activate",
                                 G_CALLBACK(menuitem_response), g_strdup(SFL_CREATE_CONFERENCE));
        gtk_menu_shell_append(GTK_MENU_SHELL(calltree_popupmenu), calltree_menu_items);
        gtk_widget_show(calltree_menu_items);
    }

    gtk_widget_grab_focus(GTK_WIDGET(tab->view));

    GtkCellRenderer *calltree_rend = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *calltree_col = gtk_tree_view_column_new_with_attributes("Icon", calltree_rend, "pixbuf", COLUMN_ACCOUNT_PIXBUF, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tab->view), calltree_col);

    calltree_rend = gtk_cell_renderer_text_new();
    calltree_col = gtk_tree_view_column_new_with_attributes("Description", calltree_rend,
                   "markup", COLUMN_ACCOUNT_DESC,
                   NULL);
    g_object_set(calltree_rend, "wrap-mode", (PangoWrapMode) PANGO_WRAP_WORD_CHAR, NULL);

    static const gint SFLPHONE_HIG_MARGIN = 10;
    static const gint CALLTREE_CALL_ICON_WIDTH = 24;
    static const gint CALLTREE_SECURITY_ICON_WIDTH = 24;
    gint CALLTREE_TEXT_WIDTH = (MAIN_WINDOW_WIDTH -
                                CALLTREE_SECURITY_ICON_WIDTH -
                                CALLTREE_CALL_ICON_WIDTH - (2 * SFLPHONE_HIG_MARGIN));
    g_object_set(calltree_rend, "wrap-width", CALLTREE_TEXT_WIDTH, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tab->view), calltree_col);

    /* Security icon */
    calltree_rend = gtk_cell_renderer_pixbuf_new();
    calltree_col = gtk_tree_view_column_new_with_attributes("Icon",
                   calltree_rend,
                   "pixbuf", COLUMN_ACCOUNT_SECURITY_PIXBUF,
                   NULL);
    g_object_set(calltree_rend, "xalign", (gfloat) 1.0, NULL);
    g_object_set(calltree_rend, "yalign", (gfloat) 0.0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tab->view), calltree_col);

    gtk_container_add(GTK_CONTAINER(calltree_sw), tab->view);

    GtkTreeSelection *calltree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tab->view));
    g_signal_connect(G_OBJECT(calltree_sel), "changed",
                     G_CALLBACK(call_selected_cb),
                     client);

    gtk_box_pack_start(GTK_BOX(tab->tree), calltree_sw, TRUE, TRUE, 0);

    // search bar if tab is either "history" or "addressbook"
    if (has_searchbar) {
        calltab_create_searchbar(tab, client);

        if (tab->searchbar != NULL) {
            GtkWidget *alignment =  gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
            gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 3, 6, 6);
            gtk_container_add(GTK_CONTAINER(alignment), tab->searchbar);
            gtk_box_pack_start(GTK_BOX(tab->tree), alignment, FALSE, TRUE, 0);
        }
    }


    if (!calltab_has_name(tab, HISTORY))
        gtk_widget_show(tab->tree);
}

static gboolean
remove_element_if_match(GtkTreeModel *model, G_GNUC_UNUSED GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    const gchar *target_id = (const gchar *) data;
    gchar *id;
    gtk_tree_model_get(model, iter, COLUMN_ID, &id, -1);
    gboolean result = FALSE;
    if (g_strcmp0(id, target_id) == 0) {
        gtk_tree_store_remove(GTK_TREE_STORE(model), iter);
        result = TRUE;  // stop iterating, we found it
    }
    g_free(id);

    return result;
}

void
calltree_remove_call(calltab_t* tab, const gchar *target_id)
{
    GtkTreeStore *store = tab->store;
    GtkTreeModel *model = GTK_TREE_MODEL(store);
    gtk_tree_model_foreach(model, remove_element_if_match, (gpointer) target_id);

    /* invalidate selected call if it was our target */
    if (!calllist_empty(tab)) {
        callable_obj_t *sel = calltab_get_selected_call(tab);
        if (sel && g_strcmp0(sel->_callID, target_id) == 0)
            calltab_select_call(tab, NULL);
    }

    statusbar_update_clock("");
}

static GdkPixbuf *history_state_to_pixbuf(callable_obj_t *call)
{
    if(call == NULL) {
        g_warning("Not a valid call in history state to pixbuf");
        return NULL;
    }

    gboolean has_rec_file = FALSE;
    gboolean is_incoming = FALSE;
    gboolean is_outgoing = FALSE;

    if(call->_recordfile && strlen(call->_recordfile) > 0)
        has_rec_file = TRUE;

    if(g_strcmp0(call->_history_state, OUTGOING_STRING) == 0)
        is_outgoing = TRUE;
    else if(g_strcmp0(call->_history_state, INCOMING_STRING) == 0)
        is_incoming = TRUE;

    gchar *svg_filename = NULL;

    if(!has_rec_file)
        svg_filename = g_strconcat(ICONS_DIR, "/", call->_history_state, ".svg", NULL);
    else {
        if(is_incoming)
            svg_filename = g_strconcat(ICONS_DIR, "/", "incoming_rec", ".svg", NULL);
        else if(is_outgoing)
            svg_filename = g_strconcat(ICONS_DIR, "/", "outgoing_rec", ".svg", NULL);
        else
            svg_filename = g_strconcat(ICONS_DIR, "/", call->_history_state, ".svg", NULL);
    }

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(svg_filename, NULL);

    g_free(svg_filename);

    return pixbuf;
}

typedef struct {
    calltab_t *tab;
    callable_obj_t *call;
} CallUpdateCtx;

typedef struct {
    calltab_t *tab;
    const conference_obj_t *conf;
} ConferenceRemoveCtx;

static gboolean
update_call(GtkTreeModel *model, G_GNUC_UNUSED GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    GdkPixbuf *pixbuf = NULL;
    GdkPixbuf *pixbuf_security = NULL;
    CallUpdateCtx *ctx = (CallUpdateCtx*) data;
    calltab_t *tab = ctx->tab;
    callable_obj_t *call = ctx->call;
    GtkTreeStore* store = tab->store;

    gchar* srtp_enabled = NULL;
    gboolean display_sas = TRUE;
    account_t* account = NULL;

    account = account_list_get_by_id(call->_accountID);

    if (account != NULL) {
        srtp_enabled = account_lookup(account, CONFIG_SRTP_ENABLE);
        display_sas = utf8_case_equal(account_lookup(account, CONFIG_ZRTP_DISPLAY_SAS), "true");
    } else {
        GHashTable * properties = sflphone_get_ip2ip_properties();
        if (properties != NULL) {
            srtp_enabled = g_hash_table_lookup(properties, CONFIG_SRTP_ENABLE);
            display_sas = utf8_case_equal(g_hash_table_lookup(properties, CONFIG_ZRTP_DISPLAY_SAS), "true");
        }
    }

    gchar *id;
    gtk_tree_model_get(model, iter, COLUMN_ID, &id, -1);

    callable_obj_t * iterCall = calllist_get_call(tab, id);
    g_free(id);

    if (iterCall != call)
        return FALSE;

    /* Update text */
    gchar *description = NULL;

    if (call->_state == CALL_STATE_TRANSFER)
        description = calltree_display_call_info(call, DISPLAY_TYPE_CALL_TRANSFER);
    else
        if (call->_sas && display_sas && call->_srtp_state == SRTP_STATE_ZRTP_SAS_UNCONFIRMED && !call->_zrtp_confirmed)
            description = calltree_display_call_info(call, DISPLAY_TYPE_SAS);
        else
            description = calltree_display_call_info(call, DISPLAY_TYPE_STATE_CODE);

    /* Update icons */
    if (calltab_has_name(tab, CURRENT_CALLS)) {
        g_debug("Receiving in state %d", call->_state);

        switch (call->_state) {
            case CALL_STATE_HOLD:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/hold.svg", NULL);
                break;
            case CALL_STATE_INCOMING:
            case CALL_STATE_RINGING:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/ring.svg", NULL);
                break;
            case CALL_STATE_CURRENT:
                if (dbus_get_is_recording(call))
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/icon_rec.svg", NULL);
                else
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/current.svg", NULL);
                break;
            case CALL_STATE_DIALING:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/dial.svg", NULL);
                break;
            case CALL_STATE_FAILURE:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/fail.svg", NULL);
                break;
            case CALL_STATE_BUSY:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/busy.svg", NULL);
                break;
            case CALL_STATE_TRANSFER:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/transfer.svg", NULL);
                break;
            default:
                g_warning("Update calltree - Should not happen!");
        }

        switch (call->_srtp_state) {
            case SRTP_STATE_SDES_SUCCESS:
                pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_confirmed.svg", NULL);
                break;
            case SRTP_STATE_ZRTP_SAS_UNCONFIRMED:
                pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_unconfirmed.svg", NULL);
                if (call->_sas != NULL)
                    g_debug("SAS is ready with value %s", call->_sas);
                break;
            case SRTP_STATE_ZRTP_SAS_CONFIRMED:
                pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_confirmed.svg", NULL);
                break;
            case SRTP_STATE_ZRTP_SAS_SIGNED:
                pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_certified.svg", NULL);
                break;
            case SRTP_STATE_UNLOCKED:
                if (utf8_case_equal(srtp_enabled, "true"))
                    pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_off.svg", NULL);
                break;
            default:
                g_warning("Update calltree srtp state #%d- Should not happen!", call->_srtp_state);
                if (utf8_case_equal(srtp_enabled, "true"))
                    pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_off.svg", NULL);
        }

    } else if (calltab_has_name(tab, HISTORY)) {
        pixbuf = history_state_to_pixbuf(call);

        g_free(description);
        description = calltree_display_call_info(call, DISPLAY_TYPE_HISTORY);
        gchar *date = get_formatted_start_timestamp(call->_time_start);
        gchar *duration = get_call_duration(call);
        gchar *full_duration = g_strconcat(date , duration , NULL);
        g_free(date);
        g_free(duration);

        gchar *old_description = description;
        description = g_strconcat(old_description, full_duration, NULL);
        g_free(full_duration);
        g_free(old_description);
    }

    gtk_tree_store_set(store, iter,
            COLUMN_ACCOUNT_PIXBUF, pixbuf,
            COLUMN_ACCOUNT_DESC, description,
            COLUMN_ACCOUNT_SECURITY_PIXBUF, pixbuf_security,
            COLUMN_ID, call->_callID,
            COLUMN_IS_CONFERENCE, FALSE,
            -1);

    g_free(description);

    if (pixbuf != NULL)
        g_object_unref(G_OBJECT(pixbuf));
    if (pixbuf_security != NULL)
        g_object_unref(G_OBJECT(pixbuf_security));
    return TRUE;
}

void
calltree_update_call(calltab_t* tab, callable_obj_t * call, SFLPhoneClient *client)
{
    if (!call) {
        g_warning("Call is NULL, ignoring");
        return;
    }
    CallUpdateCtx ctx = {tab, call};
    GtkTreeStore *store = tab->store;
    GtkTreeModel *model = GTK_TREE_MODEL(store);
    gtk_tree_model_foreach(model, update_call, (gpointer) &ctx);
    update_actions(client);
}

void calltree_add_call(calltab_t* tab, callable_obj_t * call, GtkTreeIter *parent)
{
    g_assert(tab != history_tab);
    g_return_if_fail(call != NULL);

    account_t* account_details = NULL;

    GdkPixbuf *pixbuf = NULL;
    GdkPixbuf *pixbuf_security = NULL;
    GtkTreeIter iter;
    gchar* key_exchange = NULL;
    gchar* srtp_enabled = NULL;

    // New call in the list

    gchar *description = calltree_display_call_info(call, DISPLAY_TYPE_CALL);

    gtk_tree_store_prepend(tab->store, &iter, parent);

    account_details = account_list_get_by_id(call->_accountID);

    if (account_details) {
        srtp_enabled = g_hash_table_lookup(account_details->properties, CONFIG_SRTP_ENABLE);
        key_exchange = g_hash_table_lookup(account_details->properties, CONFIG_SRTP_KEY_EXCHANGE);
    }

    g_debug("Added call key exchange is %s", key_exchange);

    if (calltab_has_name(tab, CURRENT_CALLS)) {
        switch (call->_state) {
            case CALL_STATE_INCOMING:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/ring.svg", NULL);
                break;
            case CALL_STATE_DIALING:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/dial.svg", NULL);
                break;
            case CALL_STATE_RINGING:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/ring.svg", NULL);
                break;
            case CALL_STATE_CURRENT:
                // If the call has been initiated by a another client and, when we start, it is already current
                if (dbus_get_is_recording(call))
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/icon_rec.svg", NULL);
                else
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/current.svg", NULL);
                break;
            case CALL_STATE_HOLD:
                // If the call has been initiated by a another client and, when we start, it is already current
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/hold.svg", NULL);
                break;
            case CALL_STATE_FAILURE:
                // If the call has been initiated by a another client and, when we start, it is already current
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/fail.svg", NULL);
                break;
            default:
                g_warning("Update calltree add - Should not happen!");
        }

        if (srtp_enabled && utf8_case_equal(srtp_enabled, "true"))
            pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/secure_off.svg", NULL);

    } else if (calltab_has_name(tab, CONTACTS))
        pixbuf = call->_contact_thumbnail;
    else
        g_warning("This widget doesn't exist - This is a bug in the application.");

    //Resize it
    if (pixbuf && (gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32)) {
        GdkPixbuf *new = gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
        g_object_unref(pixbuf);
        pixbuf = new;
    }

    if (pixbuf_security && (gdk_pixbuf_get_width(pixbuf_security) > 32 || gdk_pixbuf_get_height(pixbuf_security) > 32)) {
        GdkPixbuf *new = gdk_pixbuf_scale_simple(pixbuf_security, 32, 32, GDK_INTERP_BILINEAR);
        g_object_unref(pixbuf_security);
        pixbuf_security = new;
    }

    gtk_tree_store_set(tab->store, &iter,
                       COLUMN_ACCOUNT_PIXBUF, pixbuf,
                       COLUMN_ACCOUNT_DESC, description,
                       COLUMN_ACCOUNT_SECURITY_PIXBUF, pixbuf_security,
                       COLUMN_ID, call->_callID,
                       COLUMN_IS_CONFERENCE, FALSE,
                       -1);

    g_free(description);

    if (pixbuf != NULL)
        g_object_unref(G_OBJECT(pixbuf));

    if (pixbuf_security != NULL)
        g_object_unref(G_OBJECT(pixbuf));

    if (tab == active_calltree_tab)
        gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(tab->view)), &iter);
}

void calltree_add_history_entry(callable_obj_t *call)
{
    // New call in the list
    gchar * description = calltree_display_call_info(call, DISPLAY_TYPE_HISTORY);

    GtkTreeIter iter;
    gtk_tree_store_prepend(history_tab->store, &iter, NULL);

    GdkPixbuf *pixbuf = history_state_to_pixbuf(call);

    gchar *date = get_formatted_start_timestamp(call->_time_start);
    gchar *duration = get_call_duration(call);
    gchar * full_duration = g_strconcat(date, duration, NULL);
    g_free(date);
    g_free(duration);
    gchar * full_description = g_strconcat(description, full_duration, NULL);
    g_free(description);
    g_free(full_duration);

    //Resize it
    if (pixbuf && (gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32)) {
        GdkPixbuf *new = gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
        g_object_unref(pixbuf);
        pixbuf = new;
    }

    gtk_tree_store_set(history_tab->store, &iter,
                       COLUMN_ACCOUNT_PIXBUF, pixbuf,
                       COLUMN_ACCOUNT_DESC, full_description,
                       COLUMN_ACCOUNT_SECURITY_PIXBUF, NULL,
                       COLUMN_ID, call->_callID,
                       COLUMN_IS_CONFERENCE, FALSE,
                       -1);

    g_free(full_description);

    if (pixbuf != NULL)
        g_object_unref(G_OBJECT(pixbuf));
}

void calltree_add_conference_to_current_calls(conference_obj_t* conf, SFLPhoneClient *client)
{
    account_t *account_details = NULL;

    if (!conf) {
        g_warning("Conference is null");
        return;
    } else if (!conf->_confID) {
        g_warning("Conference ID is null");
        return;
    }

    g_debug("Add conference %s", conf->_confID);

    GtkTreeIter iter;
    gtk_tree_store_append(current_calls_tab->store, &iter, NULL);

    GdkPixbuf *pixbuf = NULL;

    switch (conf->_state) {
        case CONFERENCE_STATE_ACTIVE_ATTACHED:
            pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/usersAttached.svg", NULL);
            break;
        case CONFERENCE_STATE_ACTIVE_DETACHED:
        case CONFERENCE_STATE_HOLD:
        case CONFERENCE_STATE_HOLD_RECORD:
            pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/usersDetached.svg", NULL);
            break;
        case CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD:
            pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/usersAttachedRec.svg", NULL);
            break;
        case CONFERENCE_STATE_ACTIVE_DETACHED_RECORD:
            pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/usersDetachedRec.svg", NULL);
            break;
        default:
            g_warning("Update conference add - Should not happen!");
    }

    //Resize it
    if (pixbuf) {
        if (gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32) {
            GdkPixbuf *new = gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
            g_object_unref(pixbuf);
            pixbuf = new;
        }
    } else
        g_debug("Error no pixbuff for conference from %s", ICONS_DIR);

    GdkPixbuf *pixbuf_security = NULL;

    // Used to determine if at least one participant use a security feature
    // If true (at least on call use a security feature) we need to display security icons
    conf->_conf_srtp_enabled = FALSE;

    // Used to determine if the conference is secured
    // Every participant to a conference must be secured, the conference is not secured elsewhere
    conf->_conference_secured = TRUE;

    if (conf->participant_list) {
        g_debug("Determine if at least one participant uses SRTP");

        for (GSList *part = conf->participant_list; part; part = g_slist_next(part)) {
            const gchar * const call_id = (const gchar *) part->data;
            callable_obj_t *call = calllist_get_call(current_calls_tab, call_id);

            if (call == NULL)
                g_warning("Could not find call %s in call list", call_id);
            else {
                account_details = account_list_get_by_id(call->_accountID);
                gchar *srtp_enabled = "";

                if (!account_details)
                    g_warning("Could not find account %s in account list", call->_accountID);
                else
                    srtp_enabled = g_hash_table_lookup(account_details->properties, CONFIG_SRTP_ENABLE);

                if (utf8_case_equal(srtp_enabled, "true")) {
                    g_debug("SRTP enabled for participant %s", call_id);
                    conf->_conf_srtp_enabled = TRUE;
                    break;
                } else
                    g_debug("SRTP is not enabled for participant %s", call_id);
            }
        }

        g_debug("Determine if all conference participants are secured");

        if (conf->_conf_srtp_enabled) {
            for (GSList *part = conf->participant_list; part; part = g_slist_next(part)) {
                const gchar * const call_id = (gchar *) part->data;
                callable_obj_t *call = calllist_get_call(current_calls_tab, call_id);

                if (call) {
                    if (call->_srtp_state == SRTP_STATE_UNLOCKED) {
                        g_debug("Participant %s is not secured", call_id);
                        conf->_conference_secured = FALSE;
                        break;
                    } else
                        g_debug("Participant %s is secured", call_id);
                }
            }
        }
    }

    if (conf->_conf_srtp_enabled) {
        if (conf->_conference_secured) {
            g_debug("Conference is secured");
            pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_confirmed.svg", NULL);
        } else {
            g_debug("Conference is not secured");
            pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_off.svg", NULL);
        }
    }

    gchar *description = g_markup_printf_escaped("<b>%s</b>", "");
    gtk_tree_store_set(current_calls_tab->store, &iter,
                       COLUMN_ACCOUNT_PIXBUF, pixbuf,
                       COLUMN_ACCOUNT_DESC, description,
                       COLUMN_ACCOUNT_SECURITY_PIXBUF, pixbuf_security,
                       COLUMN_ID, conf->_confID,
                       COLUMN_IS_CONFERENCE, TRUE,
                       -1);
    g_free(description);

    if (pixbuf)
        g_object_unref(pixbuf);

    if (pixbuf_security)
        g_object_unref(pixbuf_security);

    for (GSList *part = conf->participant_list; part; part = g_slist_next(part)) {
        const gchar * const call_id = (gchar *) part->data;
        callable_obj_t *call = calllist_get_call(current_calls_tab, call_id);

        calltree_remove_call(current_calls_tab, call->_callID);
        calltree_add_call(current_calls_tab, call, &iter);
    }

    gtk_tree_view_set_model(GTK_TREE_VIEW(current_calls_tab->view),
                            GTK_TREE_MODEL(current_calls_tab->store));

    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(current_calls_tab->store), &iter);

    gtk_tree_view_expand_row(GTK_TREE_VIEW(current_calls_tab->view), path, FALSE);

    update_actions(client);
}

static
gboolean
remove_conference(GtkTreeModel *model, G_GNUC_UNUSED GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    if (!is_conference(model, iter))
        return FALSE;

    gchar *conf_id;
    gtk_tree_model_get(model, iter, COLUMN_ID, &conf_id, -1);

    ConferenceRemoveCtx * ctx = (ConferenceRemoveCtx *) data;
    calltab_t *tab = ctx->tab;
    conference_obj_t *tempconf = conferencelist_get(tab, conf_id);
    g_free(conf_id);

    const conference_obj_t *conf = ctx->conf;
    /* if this is not the conference we want to remove */
    if (tempconf != conf)
        return FALSE;

    int nbParticipants = gtk_tree_model_iter_n_children(model, iter);
    g_debug("nbParticipants: %d", nbParticipants);

    for (int j = 0; j < nbParticipants; j++) {
        GtkTreeIter iter_child;

        if (gtk_tree_model_iter_nth_child(model, &iter_child, iter, j)) {
            gchar *call_id;
            gtk_tree_model_get(model, &iter_child, COLUMN_ID, &call_id, -1);

            callable_obj_t *call = calllist_get_call(tab, call_id);
            g_free(call_id);

            // do not add back call in history calltree when cleaning it
            if (call && tab != history_tab)
                calltree_add_call(tab, call, NULL);
        }
    }

    gtk_tree_store_remove(GTK_TREE_STORE(model), iter);

    if (calltab_get_selected_conf(tab) == conf)
        calltab_select_conf(tab, NULL);
    return TRUE;
}

void calltree_remove_conference(calltab_t* tab, const conference_obj_t* conf, SFLPhoneClient *client)
{
    if(conf == NULL) {
        g_warning("Could not remove conference, conference pointer is NULL");
        return;
    }

    ConferenceRemoveCtx context = {tab, conf};
    GtkTreeStore *store = tab->store;
    GtkTreeModel *model = GTK_TREE_MODEL(store);
    gtk_tree_model_foreach(model, remove_conference, (gpointer) &context);

    update_actions(client);
    g_debug("Finished removing conference %s", conf->_confID);
}

void calltree_display(calltab_t *tab, SFLPhoneClient *client)
{
    /* If we already are displaying the specified calltree */
    if (calltab_has_name(active_calltree_tab, tab->name))
        return;

    if (calltab_has_name(tab, CURRENT_CALLS)) {
        if (calltab_has_name(active_calltree_tab, CONTACTS))
            gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(contactButton_), FALSE);
        else
            gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(historyButton_), FALSE);
    } else if (calltab_has_name(tab, HISTORY)) {
        if (calltab_has_name(active_calltree_tab, CONTACTS))
            gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(contactButton_), FALSE);

        gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(historyButton_), TRUE);
    } else if (calltab_has_name(tab, CONTACTS)) {
        if (calltab_has_name(active_calltree_tab, HISTORY))
            gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(historyButton_), FALSE);

        gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(contactButton_), TRUE);
        set_focus_on_addressbook_searchbar();
    } else
        g_warning("Not a valid call tab  (%d, %s)", __LINE__, __FILE__);

    if (active_calltree_tab->mainwidget)
        gtk_widget_hide(active_calltree_tab->mainwidget);
    else
        gtk_widget_hide(active_calltree_tab->tree);
    active_calltree_tab = tab;
    if (active_calltree_tab->mainwidget)
        gtk_widget_show(active_calltree_tab->mainwidget);
    else
        gtk_widget_show(active_calltree_tab->tree);

    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(active_calltree_tab->view));
    g_signal_emit_by_name(sel, "changed");
    update_actions(client);
}

gboolean calltree_update_clock(G_GNUC_UNUSED gpointer data)
{
    if (calllist_empty(current_calls_tab))
        return TRUE;

    char timestr[32];
    const gchar *msg = "";
    callable_obj_t *call = calltab_get_selected_call(current_calls_tab);

    if (call) {
        switch (call->_state) {
            case CALL_STATE_INVALID:
            case CALL_STATE_INCOMING:
            case CALL_STATE_RINGING:
            case CALL_STATE_FAILURE:
            case CALL_STATE_DIALING:
            case CALL_STATE_BUSY:
                break;
            default:
                format_duration(call, time(NULL), timestr, sizeof(timestr));
                msg = timestr;
                break;
        }
    }

    statusbar_update_clock(msg);
    return TRUE;
}

static void cleanup_popup_data(PopupData **data)
{
    if (data && *data) {
        g_free((*data)->source_ID);
        g_free((*data)->dest_ID);
        g_free(*data);
        *data = 0;
    }
}

static gboolean
has_parent(GtkTreeModel *model, GtkTreeIter *child)
{
    GtkTreeIter parent;
    return gtk_tree_model_iter_parent(model, &parent, child);
}

static gboolean try_detach(GtkTreeModel *model, GtkTreeIter *source_iter, GtkTreeIter *dest_iter)
{
    gboolean result = FALSE;
    if (has_parent(model, source_iter) && !has_parent(model, dest_iter)) {
        GValue source_val = G_VALUE_INIT;
        gtk_tree_model_get_value(model, source_iter, COLUMN_ID, &source_val);
        const gchar *source_ID = g_value_get_string(&source_val);
        sflphone_detach_participant(source_ID);
        result = TRUE;
        g_value_unset(&source_val);
    }
    return result;
}

static gboolean
handle_drop_into(GtkTreeModel *model, GtkTreeIter *source_iter, GtkTreeIter *dest_iter, SFLPhoneClient *client)
{
    GValue source_val = G_VALUE_INIT;
    gtk_tree_model_get_value(model, source_iter, COLUMN_ID, &source_val);
    const gchar *source_ID = g_value_get_string(&source_val);

    GValue dest_val = G_VALUE_INIT;
    gtk_tree_model_get_value(model, dest_iter, COLUMN_ID, &dest_val);
    const gchar *dest_ID = g_value_get_string(&dest_val);

    gboolean result = FALSE;

    if (has_parent(model, source_iter)) {
        g_debug("Source is participant, should only be detached");
        result = FALSE;
    } else if (!has_parent(model, dest_iter)) {
        if (is_conference(model, dest_iter)) {
            if (is_conference(model, source_iter)) {
                g_debug("dropped conference on conference, merging conferences");
                dbus_join_conference(source_ID, dest_ID);
                result = TRUE;
            } else {
                g_debug("dropped call on conference, adding a call to a conference");
                sflphone_add_participant(source_ID, dest_ID);
                result = TRUE;
            }
        } else if (is_conference(model, source_iter)) {
            g_debug("dropped conference on call, merging call into conference");
            sflphone_add_participant(dest_ID, source_ID);
            result = TRUE;
        } else {
            g_debug("dropped call on call, creating new conference or transferring");
            calltree_remove_call(current_calls_tab, source_ID);
            callable_obj_t *source_call = calllist_get_call(current_calls_tab, source_ID);
            calltree_add_call(current_calls_tab, source_call, NULL);
            cleanup_popup_data(&popup_data);
            popup_data = g_new0(PopupData, 1);
            popup_data->source_ID = g_strdup(source_ID);
            popup_data->dest_ID = g_strdup(dest_ID);
            popup_data->client = client;
            gtk_menu_popup(GTK_MENU(calltree_popupmenu), NULL, NULL, NULL, NULL, 0, 0);
            result = TRUE;
        }
    } else {
        // Happens when we drag a call on anther call which participate to a conference
        callable_obj_t *dest_call = calllist_get_call(current_calls_tab, dest_ID);
        if (dest_call) {
            gchar *conf_ID = dbus_get_conference_id(dest_call->_callID);
            if (g_strcmp0(conf_ID, "") != 0) {
                sflphone_add_participant(source_ID, conf_ID);
                result = TRUE;
            }
            g_free(conf_ID);
        }
    }
    g_value_unset(&source_val);
    g_value_unset(&dest_val);
    return result;
}

static gboolean valid_drop(GtkTreeModel *model, GtkTreeIter *source_iter, GtkTreePath *dest_path)
{
    gboolean result = TRUE;
    GtkTreePath *source_path = gtk_tree_model_get_path(model, source_iter);
    if (!gtk_tree_path_compare(source_path, dest_path)) {
        g_warning("invalid drop: source and destination are the same");
        result = FALSE;
    } else if (gtk_tree_path_is_ancestor(source_path, dest_path)) {
        g_warning("invalid drop: source is ancestor of destination");
        result = FALSE;
    } else if (gtk_tree_path_is_descendant(source_path, dest_path)) {
        g_warning("invalid drop: source is descendant of destination");
        result = FALSE;
    }
    gtk_tree_path_free(source_path);
    return result;
}

static gboolean
render_drop(GtkTreeModel *model, GtkTreePath *dest_path, GtkTreeViewDropPosition dest_pos,
            GtkTreeIter *source_iter, SFLPhoneClient *client)
{
    GtkTreeIter dest_iter;
    if (!gtk_tree_model_get_iter(model, &dest_iter, dest_path)) {
        g_warning("Could not get destination iterator");
        return FALSE;
    }

    gboolean result = FALSE;
    switch (dest_pos) {
        case GTK_TREE_VIEW_DROP_BEFORE:
        case GTK_TREE_VIEW_DROP_AFTER:
            g_debug("dropped at position %d, detaching if appropriate", dest_pos);
            result = try_detach(model, source_iter, &dest_iter);
            break;

        case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
        case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
            g_debug("DROP_INTO");
            if (valid_drop(model, source_iter, dest_path))
                result = handle_drop_into(model, source_iter, &dest_iter, client);
            break;
    }
    return result;
}

void drag_data_received_cb(GtkWidget *widget, GdkDragContext *context,
                           G_GNUC_UNUSED gint x, G_GNUC_UNUSED gint y,
                           G_GNUC_UNUSED GtkSelectionData *selection_data,
                           G_GNUC_UNUSED guint target_type, guint etime,
                           SFLPhoneClient *client)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW(widget);
    GtkTreeModel *model = GTK_TREE_MODEL(gtk_tree_view_get_model(tree_view));
    GtkTreeSelection *tree_selection = gtk_tree_view_get_selection(tree_view);
    GtkTreeIter source_iter;
    if (!gtk_tree_selection_get_selected(tree_selection, NULL, &source_iter)) {
        g_warning("No tree element selected");
        return;
    }
    GtkTreePath *dest_path;
    GtkTreeViewDropPosition dest_pos;
    if (!gtk_tree_view_get_dest_row_at_pos(tree_view, x, y, &dest_path, &dest_pos)) {
        g_warning("No row at given position");
        return;
    }

    gboolean success = render_drop(model, dest_path, dest_pos, &source_iter, client);
    if (gdk_drag_context_get_selected_action(context) == GDK_ACTION_MOVE)
        gtk_drag_finish(context, success, TRUE, etime);
}

/* Print a string when a menu item is selected */

static void
menuitem_response(gchar * string)
{
    if (g_strcmp0(string, SFL_CREATE_CONFERENCE) == 0) {
        dbus_join_participant(popup_data->source_ID,
                              popup_data->dest_ID);
        calltree_remove_call(current_calls_tab, popup_data->source_ID);
        calltree_remove_call(current_calls_tab, popup_data->dest_ID);
        update_actions(popup_data->client);
    } else if (g_strcmp0(string, SFL_TRANSFER_CALL) == 0) {
        callable_obj_t * source_call = calllist_get_call(current_calls_tab, popup_data->source_ID);
        callable_obj_t * dest_call = calllist_get_call(current_calls_tab, popup_data->dest_ID);
        g_debug("Transferring call %s, to %s",
              source_call->_peer_number,
              dest_call->_peer_number);
        dbus_attended_transfer(source_call, dest_call);
        calltree_remove_call(current_calls_tab, popup_data->source_ID);
    } else
        g_warning("Unknown option in menu %s", string);

    // Make sure the create conference option will appear next time the menu pops
    // The create conference option will hide if tow call from the same conference are draged on each other
    gtk_widget_show(calltree_menu_items);

    cleanup_popup_data(&popup_data);

    g_debug("%s", string);
}

