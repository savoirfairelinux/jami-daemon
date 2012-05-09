/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "calllist.h"
#include "calltree.h"
#include "str_utils.h"
#include <string.h>
#include <stdlib.h>

#include "gtk2_wrappers.h"
#include "eel-gconf-extensions.h"
#include "unused.h"
#include "dbus.h"
#include "calltab.h"
#include "logger.h"
#include "conferencelist.h"
#include "mainwindow.h"
#include "history.h"
#include "calltree.h"
#include "uimanager.h"
#include "actions.h"
#include "imwindow.h"
#include "searchbar.h"

// Messages used in menu item
static const gchar * const SFL_CREATE_CONFERENCE = "Create conference";
static const gchar * const SFL_TRANSFER_CALL = "Transfer call to";

static GtkWidget *calltree_sw = NULL;
static GtkCellRenderer *calltree_rend = NULL;
static GtkTreeSelection *calltree_sel = NULL;

static GtkWidget *calltree_popupmenu = NULL;
static GtkWidget *calltree_menu_items = NULL;

static CallType calltree_dragged_type = A_INVALID;
static CallType calltree_selected_type = A_INVALID;

static const gchar *calltree_dragged_call_id = NULL;
static const gchar *calltree_selected_call_id = NULL;
static const gchar *calltree_selected_call_id_for_drag = NULL;
static const gchar *calltree_dragged_path = NULL;
static const gchar *calltree_selected_path = NULL;
static const gchar *calltree_selected_path_for_drag = NULL;

static gint calltree_dragged_path_depth = -1;
static gint calltree_selected_path_depth = -1;
static gint calltree_selected_path_depth_for_drag = -1;

static callable_obj_t *calltree_dragged_call = NULL;
static callable_obj_t *calltree_selected_call = NULL;
static callable_obj_t *calltree_selected_call_for_drag = NULL;

static conference_obj_t *calltree_dragged_conf = NULL;
static conference_obj_t *calltree_selected_conf = NULL;

static void drag_data_get_cb(GtkTreeDragSource *drag_source, GtkTreePath *path, GtkSelectionData *selection_data, gpointer data);
static void drag_end_cb(GtkWidget *, GdkDragContext *, gpointer);
static void drag_begin_cb(GtkWidget *widget, GdkDragContext *context, gpointer data UNUSED);
static void drag_data_received_cb(GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *, guint, guint, gpointer);
static void drag_history_received_cb(GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *, guint, guint, gpointer);
static void menuitem_response(gchar *);

enum {
    COLUMN_ACCOUNT_PIXBUF = 0,
    COLUMN_ACCOUNT_DESC,
    COLUMN_ACCOUNT_SECURITY_PIXBUF,
    COLUMN_ACCOUNT_PTR,
    COLUMN_IS_CONFERENCE,
    COLUMNS_IN_TREE_STORE
};

enum {
    DRAG_ACTION_CALL_ON_CALL = 0,
    DRAG_ACTION_CALL_ON_CONF,
    DRAG_ACTION_CALL_OUT_CONf
};

/**
 * Show popup menu
 */
static gboolean
popup_menu(GtkWidget *widget,
           gpointer   user_data UNUSED)
{
    show_popup_menu(widget, NULL);
    return TRUE;
}

/* Returns TRUE if row contains a conference object pointer */
static gboolean
is_conference(GtkTreeModel *model, GtkTreeIter *iter)
{
    gboolean result = FALSE;
    gtk_tree_model_get(model, iter, COLUMN_IS_CONFERENCE, &result, -1);
    return result;
}

/* Call back when the user click on a call in the list */
static void
call_selected_cb(GtkTreeSelection *sel, void* data UNUSED)
{
    DEBUG("CallTree: Selection callback");
    GtkTreeModel *model = gtk_tree_view_get_model(gtk_tree_selection_get_tree_view(sel));

    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) {
        DEBUG("gtk_tree_selection_get_selected return non zero!!!!!!!!!!!!!!!!!!!!!!!!! stop selected callback\n");
        return;
    }

    if (active_calltree_tab == history_tab) {
        DEBUG("CallTree: Current call tree is history");
    }
    else if (active_calltree_tab == current_calls_tab) {
        DEBUG("CallTree: Current call tree is current calls");
    }

    // store info for dragndrop
    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
    gchar *string_path = gtk_tree_path_to_string(path);
    calltree_selected_path_depth = gtk_tree_path_get_depth(path);
    DEBUG("Selected path: %s", string_path);

    GValue val = G_VALUE_INIT;
    /* Get pointer to selected object, may be a call or a conference */
    gtk_tree_model_get_value(model, &iter, COLUMN_ACCOUNT_PTR, &val);

    if (is_conference(model, &iter)) {
        DEBUG("CallTree: Selected a conference");
        calltree_selected_type = A_CONFERENCE;

        calltree_selected_conf = (conference_obj_t*) g_value_get_pointer(&val);
        g_value_unset(&val);

        if (calltree_selected_conf) {
            calltab_select_conf(active_calltree_tab, calltree_selected_conf);
            calltree_selected_call_id = calltree_selected_conf->_confID;
            calltree_selected_path = string_path;
            calltree_selected_call = NULL;

            if (calltree_selected_conf->_im_widget)
                im_window_show_tab(calltree_selected_conf->_im_widget);

            DEBUG("CallTree: selected_path %s, selected_conf_id %s, selected_path_depth %d",
                  calltree_selected_path, calltree_selected_call_id, calltree_selected_path_depth);
        }
    } else {
        DEBUG("CallTree: Selected a call");
        calltree_selected_type = A_CALL;

        calltree_selected_call = g_value_get_pointer(&val);
        g_value_unset(&val);

        if (calltree_selected_call) {
            calltab_select_call(active_calltree_tab, calltree_selected_call);
            calltree_selected_call_id = calltree_selected_call->_callID;
            calltree_selected_path = string_path;
            calltree_selected_conf = NULL;

            if (calltree_selected_call->_im_widget)
                im_window_show_tab(calltree_selected_call->_im_widget);

            DEBUG("CallTree: selected_path %s, selected_call_id %s, selected_path_depth %d",
                  calltree_selected_path, calltree_selected_call_id, calltree_selected_path_depth);
        }
    }

    update_actions();
}

/* A row is activated when it is double clicked */
void
row_activated(GtkTreeView *tree_view UNUSED,
              GtkTreePath *path UNUSED,
              GtkTreeViewColumn *column UNUSED,
              void * data UNUSED)
{
    DEBUG("CallTree: Double click action");

    if (calltab_get_selected_type(active_calltree_tab) == A_CALL) {
        DEBUG("CallTree: Selected a call");
        callable_obj_t *selectedCall = calltab_get_selected_call(active_calltree_tab);

        if (selectedCall) {
            // Get the right event from the right calltree
            if (active_calltree_tab == current_calls_tab) {
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
                        sflphone_place_call(selectedCall);
                        break;
                    default:
                        WARN("Row activated - Should not happen!");
                        break;
                }
            } else {
                // If history or contact: double click action places a new call
                callable_obj_t* new_call = create_new_call(CALL, CALL_STATE_DIALING, "", selectedCall->_accountID, selectedCall->_display_name, selectedCall->_peer_number);

                calllist_add_call(current_calls_tab, new_call);
                calltree_add_call(current_calls_tab, new_call, NULL);
                // Function sflphone_place_call (new_call) is processed in process_dialing
                sflphone_place_call(new_call);
                calltree_display(current_calls_tab);
            }
        }
    } else if (calltab_get_selected_type(active_calltree_tab) == A_CONFERENCE) {
        DEBUG("CallTree: Selected a conference");

        if (active_calltree_tab == current_calls_tab) {
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
            WARN("CallTree: Selected a conference in history, should not be possible");
    }
}

/* Catch cursor-activated signal. That is, when the entry is single clicked */
static void
row_single_click(GtkTreeView *tree_view UNUSED, void * data UNUSED)
{
    gchar * displaySasOnce = NULL;

    DEBUG("CallTree: Single click action");

    callable_obj_t *selectedCall = calltab_get_selected_call(active_calltree_tab);
    conference_obj_t *selectedConf = calltab_get_selected_conf(active_calltree_tab);

    if (active_calltree_tab == current_calls_tab)
        DEBUG("CallTree: Active calltree is current_calls");
    else if (active_calltree_tab == history_tab)
        DEBUG("CallTree: Active calltree is history");

    if (calltab_get_selected_type(active_calltree_tab) == A_CALL) {
        DEBUG("CallTree: Selected a call");

        if (selectedCall) {
            account_t *account_details = account_list_get_by_id(selectedCall->_accountID);
            DEBUG("AccountID %s", selectedCall->_accountID);

            if (account_details != NULL) {
                displaySasOnce = g_hash_table_lookup(account_details->properties, ACCOUNT_DISPLAY_SAS_ONCE);
                DEBUG("Display SAS once %s", displaySasOnce);
            } else {
                GHashTable *properties = sflphone_get_ip2ip_properties();

                if (properties != NULL) {
                    displaySasOnce = g_hash_table_lookup(properties, ACCOUNT_DISPLAY_SAS_ONCE);
                    DEBUG("IP2IP displaysasonce %s", displaySasOnce);
                }
            }

            /*  Make sure that we are not in the history tab since
             *  nothing is defined for it yet
             */
            if (active_calltree_tab == current_calls_tab) {
                switch (selectedCall->_srtp_state) {
                    case SRTP_STATE_ZRTP_SAS_UNCONFIRMED:
                        selectedCall->_srtp_state = SRTP_STATE_ZRTP_SAS_CONFIRMED;

                        if (utf8_case_equal(displaySasOnce, "true"))
                            selectedCall->_zrtp_confirmed = TRUE;

                        dbus_confirm_sas(selectedCall);
                        calltree_update_call(current_calls_tab, selectedCall);
                        break;
                    case SRTP_STATE_ZRTP_SAS_CONFIRMED:
                        selectedCall->_srtp_state = SRTP_STATE_ZRTP_SAS_UNCONFIRMED;
                        dbus_reset_sas(selectedCall);
                        calltree_update_call(current_calls_tab, selectedCall);
                        break;
                    default:
                        DEBUG("Single click but no action");
                        break;
                }
            }
        }
    } else if (calltab_get_selected_type(active_calltree_tab) == A_CONFERENCE) {
        DEBUG("CallTree: Selected a conference");
        if (selectedConf)
            DEBUG("CallTree: There is actually a selected conf");
    } else
        WARN("CallTree: Warning: Unknown selection type");
}

static gboolean
button_pressed(GtkWidget* widget, GdkEventButton *event, gpointer user_data UNUSED)
{
    if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    if (active_calltree_tab == current_calls_tab)
        show_popup_menu(widget, event);
    else if (active_calltree_tab == history_tab)
        show_popup_menu_history(widget, event);
    else
        show_popup_menu_contacts(widget, event);

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
    return name;
}

static gchar *
calltree_display_call_info(callable_obj_t * c, CallDisplayType display_type, const gchar *const audio_codec)
{
    gchar display_number[strlen(c->_peer_number) + 1];
    strcpy(display_number, c->_peer_number);

    if (c->_type != CALL || !call_was_outgoing(c)) {
        // Get the hostname for this call (NULL if not existent)
        gchar * hostname = g_strrstr(c->_peer_number, "@");

        // Test if we are dialing a new number
        if (*c->_peer_number && hostname)
            display_number[hostname - c->_peer_number] = '\0';
    }

    // Different display depending on type
    gchar *name, *details = NULL;

    if (*c->_display_name) {
        name = c->_display_name;
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
            if (c->_state_code)
                suffix = g_markup_printf_escaped("\n<i>%s (%d)</i>", c->_state_code_description, c->_state_code);
            break;
        case DISPLAY_TYPE_STATE_CODE :

            if (c->_state_code)
                suffix = g_markup_printf_escaped("\n<i>%s (%d)</i>  <i>%s</i>",
                                                 c->_state_code_description, c->_state_code,
                                                 audio_codec);
            else
                suffix = g_markup_printf_escaped("\n<i>%s</i>", audio_codec);

            break;
        case DISPLAY_TYPE_CALL_TRANSFER:
            suffix = g_markup_printf_escaped("\n<i>Transfer to:%s</i> ", c->_trsft_to);
            break;
        case DISPLAY_TYPE_SAS:
            suffix = g_markup_printf_escaped("\n<i>Confirm SAS <b>%s</b> ?</i>", c->_sas);
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

void
calltree_create(calltab_t* tab, int searchbar_type)
{
    tab->tree = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    // Fix bug #708 (resize)
    gtk_widget_set_size_request(tab->tree,100,80);

    gtk_container_set_border_width(GTK_CONTAINER(tab->tree), 0);

    calltree_sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(calltree_sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(calltree_sw), GTK_SHADOW_IN);

    tab->store = gtk_tree_store_new(COLUMNS_IN_TREE_STORE,
                                    GDK_TYPE_PIXBUF, /* Icon */
                                    G_TYPE_STRING,   /* Description */
                                    GDK_TYPE_PIXBUF, /* Security Icon */
                                    G_TYPE_POINTER,  /* Pointer to the Object */
                                    G_TYPE_BOOLEAN   /* True if this is conference */
                                   );

    tab->view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tab->store));
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tab->view), FALSE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tab->view), FALSE);
    g_signal_connect(G_OBJECT(tab->view), "row-activated",
                     G_CALLBACK(row_activated),
                     NULL);

    gtk_widget_set_can_focus(calltree_sw, TRUE);
    gtk_widget_grab_focus(calltree_sw);

    g_signal_connect(G_OBJECT(tab->view), "cursor-changed",
                     G_CALLBACK(row_single_click),
                     NULL);

    // Connect the popup menu
    g_signal_connect(G_OBJECT(tab->view), "popup-menu",
                     G_CALLBACK(popup_menu),
                     NULL);
    g_signal_connect(G_OBJECT(tab->view), "button-press-event",
                     G_CALLBACK(button_pressed),
                     NULL);

    if (g_strcmp0(tab->_name, CURRENT_CALLS) == 0) {
        // Make calltree reordable for drag n drop
        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(tab->view), TRUE);

        // source widget drag n drop signals
        g_signal_connect(G_OBJECT(tab->view), "drag_end", G_CALLBACK(drag_end_cb), NULL);
        g_signal_connect(G_OBJECT(tab->view), "drag_begin", G_CALLBACK(drag_begin_cb), NULL);

        // destination widget drag n drop signals
        g_signal_connect(G_OBJECT(tab->view), "drag_data_received", G_CALLBACK(drag_data_received_cb), NULL);

        g_signal_connect(G_OBJECT(tab->view), "drag_data_get", G_CALLBACK(drag_data_get_cb), NULL);

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
    } else if (tab == history_tab) {
        gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(tab->view), TRUE);
        g_signal_connect(G_OBJECT(tab->view), "drag_data_received", G_CALLBACK(drag_history_received_cb), NULL);
    }

    gtk_widget_grab_focus(GTK_WIDGET(tab->view));

    calltree_rend = gtk_cell_renderer_pixbuf_new();
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

    g_object_unref(G_OBJECT(tab->store));
    gtk_container_add(GTK_CONTAINER(calltree_sw), tab->view);

    calltree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tab->view));
    g_signal_connect(G_OBJECT(calltree_sel), "changed",
                     G_CALLBACK(call_selected_cb),
                     NULL);

    gtk_box_pack_start(GTK_BOX(tab->tree), calltree_sw, TRUE, TRUE, 0);

    // search bar if tab is either "history" or "addressbook"
    if (searchbar_type) {
        calltab_create_searchbar(tab);

        if (tab->searchbar != NULL)
            gtk_box_pack_start(GTK_BOX(tab->tree), tab->searchbar, FALSE, TRUE, 0);
    }

    gtk_widget_show(tab->tree);
}


static void
calltree_remove_call_recursive(calltab_t* tab, gconstpointer callable, GtkTreeIter *parent)
{
    GtkTreeStore *store = tab->store;
    GtkTreeModel *model = GTK_TREE_MODEL(store);

    if (!callable)
        ERROR("CallTree: Error: Not a valid call");

    int nbChild = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), parent);

    DEBUG("Removing call");

    for (int i = 0; i < nbChild; i++) {
        GtkTreeIter child;

        if (gtk_tree_model_iter_nth_child(model, &child, parent, i)) {
            if (gtk_tree_model_iter_has_child(model, &child))
                calltree_remove_call_recursive(tab, callable, &child);

            GValue val = G_VALUE_INIT;
            gtk_tree_model_get_value(model, &child, COLUMN_ACCOUNT_PTR, &val);

            gconstpointer iterCall = g_value_get_pointer(&val);
            g_value_unset(&val);

            if (iterCall == callable)
                gtk_tree_store_remove(store, &child);
        }
    }

    if (calltab_get_selected_call(tab) == callable)
        calltab_select_call(tab, NULL);

    update_actions();

    statusbar_update_clock("");
}

void
calltree_remove_call(calltab_t* tab, callable_obj_t * c)
{
    calltree_remove_call_recursive(tab, c, NULL);
}

GdkPixbuf *history_state_to_pixbuf(const gchar *history_state)
{
    gchar *svg_filename = g_strconcat(ICONS_DIR, "/", history_state, ".svg", NULL);
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(svg_filename, NULL);
    g_free(svg_filename);
    return pixbuf;
}

static void
calltree_update_call_recursive(calltab_t* tab, callable_obj_t * c, GtkTreeIter *parent)
{
    GdkPixbuf *pixbuf = NULL;
    GdkPixbuf *pixbuf_security = NULL;
    GtkTreeIter iter;
    GtkTreeStore* store = tab->store;

    gchar* srtp_enabled = NULL;
    gboolean display_sas = TRUE;
    account_t* account = NULL;

    int nbChild = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), parent);

    if (c) {
        account = account_list_get_by_id(c->_accountID);

        if (account != NULL) {
            srtp_enabled = account_lookup(account, ACCOUNT_SRTP_ENABLED);
            display_sas = utf8_case_equal(account_lookup(account, ACCOUNT_ZRTP_DISPLAY_SAS), "true");
        } else {
            GHashTable * properties = sflphone_get_ip2ip_properties();
            if (properties != NULL) {
                srtp_enabled = g_hash_table_lookup(properties, ACCOUNT_SRTP_ENABLED);
                display_sas = utf8_case_equal(g_hash_table_lookup(properties, ACCOUNT_ZRTP_DISPLAY_SAS), "true");
            }
        }
    }

    for (gint i = 0; i < nbChild; i++) {

        if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, parent, i)) {

            if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter))
                calltree_update_call_recursive(tab, c, &iter);

            GValue val = G_VALUE_INIT;
            gtk_tree_model_get_value(GTK_TREE_MODEL(store), &iter, COLUMN_ACCOUNT_PTR, &val);

            callable_obj_t * iterCall = (callable_obj_t*) g_value_get_pointer(&val);
            g_value_unset(&val);

            if (iterCall != c)
                continue;

            /* Update text */
            gchar * description = NULL;
            gchar * audio_codec = call_get_audio_codec(c);

            if (c->_state == CALL_STATE_TRANSFER)
                description = calltree_display_call_info(c, DISPLAY_TYPE_CALL_TRANSFER, "");
            else
                if (c->_sas && display_sas && c->_srtp_state == SRTP_STATE_ZRTP_SAS_UNCONFIRMED && !c->_zrtp_confirmed)
                    description = calltree_display_call_info(c, DISPLAY_TYPE_SAS, "");
                else
                    description = calltree_display_call_info(c, DISPLAY_TYPE_STATE_CODE, audio_codec);

            g_free(audio_codec);

            /* Update icons */
            if (tab == current_calls_tab) {
                DEBUG("Receiving in state %d", c->_state);

                switch (c->_state) {
                    case CALL_STATE_HOLD:
                        pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/hold.svg", NULL);
                        break;
                    case CALL_STATE_INCOMING:
                    case CALL_STATE_RINGING:
                        pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/ring.svg", NULL);
                        break;
                    case CALL_STATE_CURRENT:
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
                    case CALL_STATE_RECORD:
                        pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/icon_rec.svg", NULL);
                        break;
                    default:
                        WARN("Update calltree - Should not happen!");
                }

                switch (c->_srtp_state) {
                    case SRTP_STATE_SDES_SUCCESS:
                        pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_confirmed.svg", NULL);
                        break;
                    case SRTP_STATE_ZRTP_SAS_UNCONFIRMED:
                        pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_unconfirmed.svg", NULL);
                        if (c->_sas != NULL)
                            DEBUG("SAS is ready with value %s", c->_sas);
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
                        WARN("Update calltree srtp state #%d- Should not happen!", c->_srtp_state);
                        if (utf8_case_equal(srtp_enabled, "true"))
                            pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_off.svg", NULL);
                }

            } else if (tab == history_tab) {
                // parent is NULL this is not a conference participant
                if (parent == NULL)
                    pixbuf = history_state_to_pixbuf(c->_history_state);
                else // parent is not NULL this is a conference participant
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/current.svg", NULL);

                g_free(description);
                description = calltree_display_call_info(c, DISPLAY_TYPE_HISTORY, "");
                gchar * date = get_formatted_start_timestamp(c->_time_start);
                gchar *duration = get_call_duration(c);
                gchar *full_duration = g_strconcat(date , duration , NULL);
                g_free(date);
                g_free(duration);

                gchar *old_description = description;
                description = g_strconcat(old_description, full_duration, NULL);
                g_free(full_duration);
                g_free(old_description);
            }

            gtk_tree_store_set(store, &iter,
                               COLUMN_ACCOUNT_PIXBUF, pixbuf,
                               COLUMN_ACCOUNT_DESC, description,
                               COLUMN_ACCOUNT_SECURITY_PIXBUF, pixbuf_security,
                               COLUMN_ACCOUNT_PTR, c,
                               COLUMN_IS_CONFERENCE, FALSE,
                               -1);

            g_free(description);

            if (pixbuf != NULL)
                g_object_unref(G_OBJECT(pixbuf));
            if (pixbuf_security != NULL)
                g_object_unref(G_OBJECT(pixbuf_security));
        }
    }

    update_actions();
}

void calltree_update_call(calltab_t* tab, callable_obj_t * c)
{
    calltree_update_call_recursive(tab, c, NULL);
}

void calltree_add_call(calltab_t* tab, callable_obj_t * c, GtkTreeIter *parent)
{
    g_assert(tab != history_tab);

    account_t* account_details = NULL;

    GdkPixbuf *pixbuf = NULL;
    GdkPixbuf *pixbuf_security = NULL;
    GtkTreeIter iter;
    gchar* key_exchange = NULL;
    gchar* srtp_enabled = NULL;

    // New call in the list

    gchar *description = calltree_display_call_info(c, DISPLAY_TYPE_CALL, "");

    gtk_tree_store_prepend(tab->store, &iter, parent);

    if (c) {
        account_details = account_list_get_by_id(c->_accountID);

        if (account_details) {
            srtp_enabled = g_hash_table_lookup(account_details->properties, ACCOUNT_SRTP_ENABLED);
            key_exchange = g_hash_table_lookup(account_details->properties, ACCOUNT_KEY_EXCHANGE);
        }
    }

    DEBUG("Added call key exchange is %s", key_exchange);

    if (tab == current_calls_tab) {
        switch (c->_state) {
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
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/current.svg", NULL);
                break;
            case CALL_STATE_HOLD:
                // If the call has been initiated by a another client and, when we start, it is already current
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/hold.svg", NULL);
                break;
            case CALL_STATE_RECORD:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/icon_rec.svg", NULL);
                break;
            case CALL_STATE_FAILURE:
                // If the call has been initiated by a another client and, when we start, it is already current
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/fail.svg", NULL);
                break;
            default:
                WARN("Update calltree add - Should not happen!");
        }

        if (srtp_enabled && utf8_case_equal(srtp_enabled, "true"))
            pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/secure_off.svg", NULL);

    } else if (tab == contacts_tab)
        pixbuf = c->_contact_thumbnail;
    else
        WARN("CallTree: This widget doesn't exist - This is a bug in the application.");

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
                       COLUMN_ACCOUNT_PTR, c,
                       COLUMN_IS_CONFERENCE, FALSE,
                       -1);

    g_free(description);

    if (pixbuf != NULL)
        g_object_unref(G_OBJECT(pixbuf));

    if (pixbuf_security != NULL)
        g_object_unref(G_OBJECT(pixbuf));

    gtk_tree_view_set_model(GTK_TREE_VIEW(history_tab->view), GTK_TREE_MODEL(history_tab->store));

    gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(tab->view)), &iter);
}

void calltree_add_history_entry(callable_obj_t *c)
{
    if (!eel_gconf_get_integer(HISTORY_ENABLED))
        return;

    // New call in the list
    gchar * description = calltree_display_call_info(c, DISPLAY_TYPE_HISTORY, "");

    GtkTreeIter iter;
    gtk_tree_store_prepend(history_tab->store, &iter, NULL);

    GdkPixbuf *pixbuf = history_state_to_pixbuf(c->_history_state);

    gchar *date = get_formatted_start_timestamp(c->_time_start);
    gchar *duration = get_call_duration(c);
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
                       COLUMN_ACCOUNT_PTR, c,
                       COLUMN_IS_CONFERENCE, FALSE,
                       -1);

    g_free(full_description);

    if (pixbuf != NULL)
        g_object_unref(G_OBJECT(pixbuf));

    gtk_tree_view_set_model(GTK_TREE_VIEW(history_tab->view), GTK_TREE_MODEL(history_tab->store));

    history_search();
}


void calltree_add_conference_to_current_calls(conference_obj_t* conf)
{
    account_t *account_details = NULL;

    if (!conf) {
        ERROR("Calltree: Error: Conference is null");
        return;
    } else if (!conf->_confID) {
        ERROR("Calltree: Error: Conference ID is null");
        return;
    }

    DEBUG("Calltree: Add conference %s", conf->_confID);

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
            WARN("Update conference add - Should not happen!");
    }

    //Resize it
    if (pixbuf) {
        if (gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32) {
            GdkPixbuf *new = gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
            g_object_unref(pixbuf);
            pixbuf = new;
        }
    } else
        DEBUG("Error no pixbuff for conference from %s", ICONS_DIR);

    GdkPixbuf *pixbuf_security = NULL;

    // Used to determine if at least one participant use a security feature
    // If true (at least on call use a security feature) we need to display security icons
    conf->_conf_srtp_enabled = FALSE;

    // Used to determine if the conference is secured
    // Every participant to a conference must be secured, the conference is not secured elsewhere
    conf->_conference_secured = TRUE;

    if (conf->participant_list) {
        DEBUG("Calltree: Determine if at least one participant uses SRTP");

        for (GSList *part = conf->participant_list; part; part = g_slist_next(part)) {
            const gchar * const call_id = (const gchar *) part->data;
            callable_obj_t *call = calllist_get_call(current_calls_tab, call_id);

            if (call == NULL)
                ERROR("Calltree: Error: Could not find call %s in call list", call_id);
            else {
                account_details = account_list_get_by_id(call->_accountID);
                gchar *srtp_enabled = "";

                if (!account_details)
                    ERROR("Calltree: Error: Could not find account %s in account list", call->_accountID);
                else
                    srtp_enabled = g_hash_table_lookup(account_details->properties, ACCOUNT_SRTP_ENABLED);

                if (utf8_case_equal(srtp_enabled, "true")) {
                    DEBUG("Calltree: SRTP enabled for participant %s", call_id);
                    conf->_conf_srtp_enabled = TRUE;
                    break;
                } else
                    DEBUG("Calltree: SRTP is not enabled for participant %s", call_id);
            }
        }

        DEBUG("Calltree: Determine if all conference participants are secured");

        if (conf->_conf_srtp_enabled) {
            for (GSList *part = conf->participant_list; part; part = g_slist_next(part)) {
                const gchar * const call_id = (gchar *) part->data;
                callable_obj_t *call = calllist_get_call(current_calls_tab, call_id);

                if (call) {
                    if (call->_srtp_state == SRTP_STATE_UNLOCKED) {
                        DEBUG("Calltree: Participant %s is not secured", call_id);
                        conf->_conference_secured = FALSE;
                        break;
                    } else
                        DEBUG("Calltree: Participant %s is secured", call_id);
                }
            }
        }
    }

    if (conf->_conf_srtp_enabled) {
        if (conf->_conference_secured) {
            DEBUG("Calltree: Conference is secured");
            pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_confirmed.svg", NULL);
        } else {
            DEBUG("Calltree: Conference is not secured");
            pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_off.svg", NULL);
        }
    }

    DEBUG("Calltree: Add conference to tree store");

    gchar *description = g_markup_printf_escaped("<b>%s</b>", "");
    gtk_tree_store_set(current_calls_tab->store, &iter,
                       COLUMN_ACCOUNT_PIXBUF, pixbuf,
                       COLUMN_ACCOUNT_DESC, description,
                       COLUMN_ACCOUNT_SECURITY_PIXBUF, pixbuf_security,
                       COLUMN_ACCOUNT_PTR, conf,
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

        calltree_remove_call(current_calls_tab, call);
        calltree_add_call(current_calls_tab, call, &iter);
    }

    gtk_tree_view_set_model(GTK_TREE_VIEW(current_calls_tab->view),
                            GTK_TREE_MODEL(current_calls_tab->store));

    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(current_calls_tab->store), &iter);

    gtk_tree_view_expand_row(GTK_TREE_VIEW(current_calls_tab->view), path, FALSE);

    update_actions();
}

static
void calltree_remove_conference_recursive(calltab_t* tab, const conference_obj_t* conf, GtkTreeIter *parent)
{
    GtkTreeModel *model = GTK_TREE_MODEL(tab->store);
    int nbChildren = gtk_tree_model_iter_n_children(model, parent);

    for (int i = 0; i < nbChildren; i++) {
        GtkTreeIter iter_parent;

        /* if the nth child of parent has one or more children */
        if (gtk_tree_model_iter_nth_child(model, &iter_parent, parent, i)) {
            /* RECURSION! */
            if (gtk_tree_model_iter_has_child(model, &iter_parent))
                calltree_remove_conference_recursive(tab, conf, &iter_parent);

            GValue confval = G_VALUE_INIT;
            gtk_tree_model_get_value(model, &iter_parent, COLUMN_ACCOUNT_PTR, &confval);

            conference_obj_t *tempconf = (conference_obj_t*) g_value_get_pointer(&confval);
            g_value_unset(&confval);

            /* if this is the conference we want to remove */
            if (tempconf == conf) {
                int nbParticipants = gtk_tree_model_iter_n_children(model, &iter_parent);
                DEBUG("CallTree: nbParticipants: %d", nbParticipants);

                for (int j = 0; j < nbParticipants; j++) {
                    GtkTreeIter iter_child;

                    if (gtk_tree_model_iter_nth_child(model, &iter_child, &iter_parent, j)) {
                        GValue callval = G_VALUE_INIT;
                        gtk_tree_model_get_value(model, &iter_child, COLUMN_ACCOUNT_PTR, &callval);

                        callable_obj_t *call = g_value_get_pointer(&callval);
                        g_value_unset(&callval);

                        // do not add back call in history calltree when cleaning it
                        if (call && tab != history_tab)
                            calltree_add_call(tab, call, NULL);
                    }
                }

                DEBUG("CallTree: Remove conference %s", conf->_confID);
                gtk_tree_store_remove(tab->store, &iter_parent);
            }
        }
    }

    if (calltab_get_selected_conf(tab) == conf)
        calltab_select_conf(tab, NULL);

    update_actions();
}

void calltree_remove_conference(calltab_t* tab, const conference_obj_t* conf)
{
    DEBUG("CallTree: Remove conference %s", conf->_confID);
    calltree_remove_conference_recursive(tab, conf, NULL);
    DEBUG("CallTree: Finished Removing conference");
}

void calltree_display(calltab_t *tab)
{
    /* If we already are displaying the specified calltree */
    if (active_calltree_tab == tab)
        return;

    if (tab == current_calls_tab) {
        if (active_calltree_tab == contacts_tab)
            gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(contactButton_), FALSE);
        else
            gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(historyButton_), FALSE);
    } else if (tab == history_tab) {
        if (active_calltree_tab == contacts_tab)
            gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(contactButton_), FALSE);

        gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(historyButton_), TRUE);
    } else if (tab == contacts_tab) {
        if (active_calltree_tab == history_tab)
            gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(historyButton_), FALSE);

        gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(contactButton_), TRUE);
        set_focus_on_addressbook_searchbar();
    } else
        ERROR("CallTree: Error: Not a valid call tab  (%d, %s)", __LINE__, __FILE__);

    gtk_widget_hide(active_calltree_tab->tree);
    active_calltree_tab = tab;
    gtk_widget_show(active_calltree_tab->tree);

    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(active_calltree_tab->view));
    DEBUG("CallTree: Emit signal changed from calltree_display");
    g_signal_emit_by_name(sel, "changed");
    update_actions();
}


gboolean calltree_update_clock(gpointer data UNUSED)
{
    char timestr[20];
    const gchar *msg = "";
    long duration;
    callable_obj_t *c = calltab_get_selected_call(current_calls_tab);

    if (c)
        switch (c->_state) {
            case CALL_STATE_INVALID:
            case CALL_STATE_INCOMING:
            case CALL_STATE_RINGING:
            case CALL_STATE_FAILURE:
            case CALL_STATE_DIALING:
            case CALL_STATE_BUSY:
                break;
            default:
                duration = difftime(time(NULL), c->_time_start);

                if (duration < 0)
                    duration = 0;

                g_snprintf(timestr, sizeof(timestr), "%.2ld:%.2ld", duration / 60, duration % 60);
                msg = timestr;
                break;
        }

    statusbar_update_clock(msg);
    return TRUE;
}

static gboolean
non_draggable_call(callable_obj_t *call)
{
    return call->_state == CALL_STATE_DIALING ||
           call->_state == CALL_STATE_INVALID ||
           call->_state == CALL_STATE_FAILURE ||
           call->_state == CALL_STATE_BUSY    ||
           call->_state == CALL_STATE_TRANSFER;
}

static callable_obj_t *get_call_from_path(GtkTreeModel *model, GtkTreePath *path) {
    callable_obj_t *call = NULL;
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        GValue val = G_VALUE_INIT;
        gtk_tree_model_get_value(model, &iter, COLUMN_ACCOUNT_PTR, &val);
        call = (callable_obj_t *) g_value_get_pointer(&val);
        g_value_unset(&val);
        return call;
    }

    ERROR("Call iterator is invalid returning NULL");

    return NULL;
}

/**
 * Source side drag signals
 * The data to be dragged to
 */
static void drag_data_get_cb(GtkTreeDragSource *drag_source UNUSED, GtkTreePath *path UNUSED, GtkSelectionData *selection_data UNUSED, gpointer data UNUSED)
{
}

static void drag_begin_cb (GtkWidget *widget UNUSED, GdkDragContext *context UNUSED, gpointer data UNUSED)
{
    DEBUG("CallTree: Source Drag Begin callback");
    DEBUG("CallTreeS: selected_path %s, selected_call_id %s, selected_path_depth %d",
          calltree_selected_path, calltree_selected_call_id, calltree_selected_path_depth);
    calltree_selected_path_for_drag = calltree_selected_path;
    calltree_selected_call_id_for_drag = calltree_selected_call_id;
    calltree_selected_path_depth_for_drag = calltree_selected_path_depth;
    calltree_selected_call_for_drag = calltree_selected_call;
}

static void undo_drag_call_action(callable_obj_t *c, GtkTreePath *spath)
{
    calltree_remove_call(current_calls_tab, c);

    if (spath && calltree_selected_call_for_drag->_confID) {
        gtk_tree_path_up(spath);
        // conference for which this call is attached
        GtkTreeIter parent_conference;
        gtk_tree_model_get_iter(GTK_TREE_MODEL(current_calls_tab->store), &parent_conference, spath);
        calltree_add_call(current_calls_tab, c, &parent_conference);
    }
    else {
        calltree_add_call(current_calls_tab, c, NULL);
    }

    calltree_dragged_call = NULL;
}

static void drag_end_cb(GtkWidget * widget UNUSED, GdkDragContext * context UNUSED, gpointer data UNUSED)
{
    if (active_calltree_tab == history_tab)
        return;

    DEBUG("CallTree: Source Drag End callback");
    DEBUG("CallTreeS: selected_path %s, selected_call_id %s, selected_path_depth %d",
          calltree_selected_path_for_drag, calltree_selected_call_id_for_drag, calltree_selected_path_depth_for_drag);
    DEBUG("CallTree: dragged path %s, dragged_call_id %s, dragged_path_depth %d",
          calltree_selected_path, calltree_selected_call_id, calltree_selected_path_depth);

    GtkTreeView *treeview = GTK_TREE_VIEW(widget);
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);

    calltree_dragged_path = calltree_selected_path;
    calltree_dragged_call_id = calltree_selected_call_id;
    calltree_dragged_path_depth = calltree_selected_path_depth;

    GtkTreePath *path = gtk_tree_path_new_from_string(calltree_dragged_path);
    GtkTreePath *dpath = gtk_tree_path_new_from_string(calltree_dragged_path);
    GtkTreePath *spath = gtk_tree_path_new_from_string(calltree_selected_path_for_drag);

    GtkTreeIter iter;

    // Make sure drag n drop does not imply a dialing call for either selected and dragged call
    if (calltree_selected_call && (calltree_selected_type == A_CALL)) {
        DEBUG("CallTree: Selected a call");

        if (non_draggable_call(calltree_selected_call)) {
            DEBUG("CallTree: Selected an invalid call");
            undo_drag_call_action(calltree_selected_call_for_drag, NULL);
            return;
        }

        if (calltree_dragged_call && (calltree_dragged_type == A_CALL)) {

            DEBUG("CallTree: Dragged on a call");

            if (non_draggable_call(calltree_dragged_call)) {
                DEBUG("CallTree: Dragged on an invalid call");
                undo_drag_call_action(calltree_selected_call_for_drag, spath);
                return;
            }
        }
    }

    // Make sure a conference is only dragged on another conference
    if (calltree_selected_conf && (calltree_selected_type == A_CONFERENCE)) {

        DEBUG("CallTree: Selected a conference");

        if (!calltree_dragged_conf && (calltree_dragged_type == A_CALL)) {

            DEBUG("CallTree: Dragged on a call");
            conference_obj_t* conf = calltree_selected_conf;

            calltree_remove_conference(current_calls_tab, conf);
            calltree_add_conference_to_current_calls(conf);

            calltree_dragged_call = NULL;
            return;
        }
    }

    if (calltree_selected_path_depth_for_drag == 1) {
        if (calltree_dragged_path_depth == 1) {
            if (calltree_selected_type == A_CALL && calltree_dragged_type == A_CALL) {
                // dragged a single call on a single call
                if (calltree_selected_call_for_drag && calltree_dragged_call) {
                    calltree_remove_call(current_calls_tab, calltree_selected_call_for_drag);
                    calltree_add_call(current_calls_tab, calltree_selected_call_for_drag, NULL);
                    // pop menu to determine if we actually create a conference or do a call transfer
                    gtk_menu_popup(GTK_MENU(calltree_popupmenu), NULL, NULL, NULL, NULL, 0, 0);
                }
            } else if (calltree_selected_type == A_CALL && calltree_dragged_type == A_CONFERENCE) {

                // dragged a single call on a conference
                if (!calltree_selected_call_for_drag) {
                    DEBUG("Error: call dragged on a conference is null");
                    return;
                }

                g_free(calltree_selected_call_for_drag->_confID);
                calltree_selected_call_for_drag->_confID = g_strdup(calltree_dragged_call_id);

                g_free(calltree_selected_call_for_drag->_historyConfID);
                calltree_selected_call_for_drag->_historyConfID = g_strdup(calltree_dragged_call_id);

                sflphone_add_participant(calltree_selected_call_id_for_drag, calltree_dragged_call_id);
            } else if (calltree_selected_type == A_CONFERENCE && calltree_dragged_type == A_CALL) {

                // dragged a conference on a single call
                conference_obj_t* conf = calltree_selected_conf;

                calltree_remove_conference(current_calls_tab, conf);
                calltree_add_conference_to_current_calls(conf);

            } else if (calltree_selected_type == A_CONFERENCE && calltree_dragged_type == A_CONFERENCE) {

                // dragged a conference on a conference
                if (gtk_tree_path_compare(dpath, spath) == 0) {

                    if (!current_calls_tab) {
                        DEBUG("Error while joining the same conference\n");
                        return;
                    }

                    DEBUG("Joined the same conference!\n");
                    gtk_tree_view_expand_row(GTK_TREE_VIEW(current_calls_tab->view), path, FALSE);
                } else {
                    if (!calltree_selected_conf)
                        DEBUG("Error: selected conference is null while joining 2 conference");

                    if (!calltree_dragged_conf)
                        DEBUG("Error: dragged conference is null while joining 2 conference");

                    DEBUG("Joined conferences %s and %s!\n", calltree_dragged_path, calltree_selected_path);
                    dbus_join_conference(calltree_selected_conf->_confID, calltree_dragged_conf->_confID);
                }
            }

            // TODO: dragged a single call on a NULL element (should do nothing)
            // TODO: dragged a conference on a NULL element (should do nothing)

        } else {
            // dragged_path_depth == 2
            if (calltree_selected_type == A_CALL && calltree_dragged_type == A_CALL) {
                // TODO: dragged a call on a conference call
                calltree_remove_call(current_calls_tab, calltree_selected_call);
                calltree_add_call(current_calls_tab, calltree_selected_call, NULL);

            } else if (calltree_selected_type == A_CONFERENCE && calltree_dragged_type == A_CALL) {
                // TODO: dragged a conference on a conference call
                calltree_remove_conference(current_calls_tab, calltree_selected_conf);
                calltree_add_conference_to_current_calls(calltree_selected_conf);
            }

            // TODO: dragged a single call on a NULL element
            // TODO: dragged a conference on a NULL element
        }
    } else {

        if (calltree_dragged_path_depth == 1) {
            if (calltree_selected_type == A_CALL && calltree_dragged_type == A_CALL) {

                // dragged a conference call on a call
                sflphone_detach_participant(calltree_selected_call_id);

                if (calltree_selected_call && calltree_dragged_call)
                    gtk_menu_popup(GTK_MENU(calltree_popupmenu), NULL, NULL, NULL, NULL,
                                   0, 0);

            } else if (calltree_selected_type == A_CALL && calltree_dragged_type == A_CONFERENCE) {
                // dragged a conference call on a conference
                sflphone_detach_participant(calltree_selected_call_id);

                if (calltree_selected_call && calltree_dragged_conf) {
                    DEBUG("Adding a participant, since dragged call on a conference");
                    sflphone_add_participant(calltree_selected_call_id, calltree_dragged_call_id);
                }
            } else {
                // dragged a conference call on a NULL element
                sflphone_detach_participant(calltree_selected_call_id);
            }

        } else {
            // dragged_path_depth == 2
            // dragged a conference call on another conference call (same conference)
            // TODO: dragged a conference call on another conference call (different conference)

            gtk_tree_path_up(path);
            // conference for which this call is attached
            GtkTreeIter parent_conference;
            gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &parent_conference, path);

            gtk_tree_path_up(dpath);
            gtk_tree_path_up(spath);

            if (gtk_tree_path_compare(dpath, spath) == 0) {

                DEBUG("Dragged a call in the same conference");
                calltree_remove_call(current_calls_tab, calltree_selected_call);
                calltree_add_call(current_calls_tab, calltree_selected_call, &parent_conference);
                gtk_widget_hide(calltree_menu_items);
                gtk_menu_popup(GTK_MENU(calltree_popupmenu), NULL, NULL, NULL, NULL,
                               0, 0);
            } else {
                DEBUG("Dragged a conference call onto another conference call %s, %s", gtk_tree_path_to_string(dpath), gtk_tree_path_to_string(spath));

                conference_obj_t *conf = NULL;

                if (gtk_tree_model_get_iter(model, &iter, dpath)) {
                    if (is_conference(model, &iter)) {
                        GValue val = G_VALUE_INIT;
                        gtk_tree_model_get_value(model, &iter, COLUMN_ACCOUNT_PTR, &val);
                        conf = (conference_obj_t*) g_value_get_pointer(&val);
                        g_value_unset(&val);
                    }
                }

                sflphone_detach_participant(calltree_selected_call_id);

                if (conf)
                    sflphone_add_participant(calltree_selected_call_id, conf->_confID);
                else
                    DEBUG("didn't find a conf!");
            }

            // TODO: dragged a conference call on another conference call (different conference)
            // TODO: dragged a conference call on a NULL element (same conference)
            // TODO: dragged a conference call on a NULL element (different conference)
        }
    }
}

void drag_history_received_cb(GtkWidget *widget, GdkDragContext *context UNUSED, gint x UNUSED, gint y UNUSED, GtkSelectionData *selection_data UNUSED, guint info UNUSED, guint t UNUSED, gpointer data UNUSED)
{
    g_signal_stop_emission_by_name(G_OBJECT(widget), "drag_data_received");
}

void drag_data_received_cb(GtkWidget *widget, GdkDragContext *context UNUSED, gint x UNUSED, gint y UNUSED, GtkSelectionData *selection_data UNUSED, guint info UNUSED, guint t UNUSED, gpointer data UNUSED)
{
    if (active_calltree_tab == history_tab) {
        g_signal_stop_emission_by_name(G_OBJECT(widget), "drag_data_received");
        return;
    }

    GtkTreeView *tree_view = GTK_TREE_VIEW(widget);
    GtkTreeModel* tree_model = gtk_tree_view_get_model(tree_view);

    GtkTreeViewDropPosition position;
    GtkTreePath *drop_path;
    gtk_tree_view_get_drag_dest_row(tree_view, &drop_path, &position);

    if (drop_path) {

        GtkTreeIter iter;
        gtk_tree_model_get_iter(tree_model, &iter, drop_path);
        GValue val = G_VALUE_INIT;
        gtk_tree_model_get_value(tree_model, &iter, COLUMN_ACCOUNT_PTR, &val);

        if (is_conference(tree_model, &iter)) {
            DEBUG("CallTree: Dragging on a conference");
            calltree_dragged_type = A_CONFERENCE;
            calltree_dragged_call = NULL;
        } else {
            DEBUG("CallTree: Dragging on a call");
            calltree_dragged_type = A_CALL;
            calltree_dragged_conf = NULL;
        }

        DEBUG("Position %d", position);
        switch (position)  {

            case GTK_TREE_VIEW_DROP_AFTER:
                /* fallthrough */
            case GTK_TREE_VIEW_DROP_BEFORE:
                calltree_dragged_path = gtk_tree_path_to_string(drop_path);
                calltree_dragged_path_depth = gtk_tree_path_get_depth(drop_path);
                calltree_dragged_call_id = NULL;
                calltree_dragged_call = NULL;
                calltree_dragged_conf = NULL;
                break;

            case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
                /* fallthrough */
            case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
                calltree_dragged_path = gtk_tree_path_to_string(drop_path);
                calltree_dragged_path_depth = gtk_tree_path_get_depth(drop_path);

                if (calltree_dragged_type == A_CALL) {
                    calltree_dragged_call_id = ((callable_obj_t*) g_value_get_pointer(&val))->_callID;
                    calltree_dragged_call = (callable_obj_t*) g_value_get_pointer(&val);
                } else {
                    calltree_dragged_call_id = ((conference_obj_t*) g_value_get_pointer(&val))->_confID;
                    calltree_dragged_conf = (conference_obj_t*) g_value_get_pointer(&val);
                }
                break;

            default:
                break;
        }
        g_value_unset(&val);
    }
}

/* Print a string when a menu item is selected */

static void menuitem_response(gchar *string)
{
    if (g_strcmp0(string, SFL_CREATE_CONFERENCE) == 0)
        dbus_join_participant(calltree_selected_call_for_drag->_callID,
                              calltree_dragged_call->_callID);
    else if (g_strcmp0(string, SFL_TRANSFER_CALL) == 0) {
        DEBUG("Calltree: Transferring call %s, to %s",
              calltree_selected_call->_peer_number,
              calltree_dragged_call->_peer_number);
        dbus_attended_transfer(calltree_selected_call, calltree_dragged_call);
        calltree_remove_call(current_calls_tab, calltree_selected_call);
    } else
        DEBUG("CallTree: Error unknown option selected in menu %s", string);

    // Make sure the create conference option will appear next time the menu pops
    // The create conference option will hide if tow call from the same conference are draged on each other
    gtk_widget_show(calltree_menu_items);

    DEBUG("%s", string);
    g_free(string);
}

