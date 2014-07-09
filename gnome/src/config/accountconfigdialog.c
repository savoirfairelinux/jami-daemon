/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#include <glib/gi18n.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <gtk/gtk.h>

#include "config.h"
#include "str_utils.h"
#include "actions.h"
#include "accountlist.h"
#include "audioconf.h"
#include "videoconf.h"
#include "accountconfigdialog.h"
#include "account_schema.h"
#include "zrtpadvanceddialog.h"
#include "tlsadvanceddialog.h"
#include "dbus/dbus.h"
#include "utils.h"
#include "seekslider.h"

/**
 * TODO: tidy this up
 * by storing these variables
 * in a private structure.
 * Local variables
 */
static GtkWidget *entry_alias;
static GtkWidget *protocol_combo;
static GtkWidget *entry_username;
static GtkWidget *entry_route_set;
static GtkWidget *entry_hostname;
static GtkWidget *entry_password;
static GtkWidget *entry_mailbox;
static GtkWidget *entry_user_agent;
static GtkWidget *expire_spin_box;
static GtkListStore *credential_store;
static GtkWidget *delete_cred_button;
static GtkWidget *treeview_credential;
static GtkWidget *zrtp_button;
static GtkWidget *key_exchange_combo;
static GtkWidget *use_sip_tls_check_box;
static GtkWidget *local_address_entry;
static GtkWidget *published_address_entry;
static GtkWidget *local_address_combo;
static GtkWidget *use_stun_check_box;
static GtkWidget *same_as_local_radio_button;
static GtkWidget *published_addr_radio_button;
static GtkWidget *published_port_spin_box;
static GtkWidget *local_port_spin_box;
static GtkWidget *published_address_label;
static GtkWidget *published_port_label;
static GtkWidget *stun_server_label;
static GtkWidget *stun_server_entry;
static GtkWidget *enable_tone;
static GtkWidget *file_chooser;
static GtkWidget *security_tab;
static GtkWidget *advanced_tab;
static GtkWidget *overrtp;
static GtkWidget *ringtone_seekslider;
static GtkWidget *audio_port_min_spin_box;
static GtkWidget *audio_port_max_spin_box;
#ifdef SFL_VIDEO
static GtkWidget *enable_video_button;
static GtkWidget *video_port_min_spin_box;
static GtkWidget *video_port_max_spin_box;
#endif
#ifdef SFL_PRESENCE
static GtkWidget *presence_check_box;
static gboolean is_account_new;
#endif

typedef struct OptionsData {
    account_t *account;
    SFLPhoneClient *client;
} OptionsData;

// Credentials
enum {
    COLUMN_CREDENTIAL_REALM,
    COLUMN_CREDENTIAL_USERNAME,
    COLUMN_CREDENTIAL_PASSWORD,
    COLUMN_CREDENTIAL_DATA,
    COLUMN_CREDENTIAL_COUNT
};

/*
 * The same window is used with different configurations
 * so we need to reset some data to prevent side-effects
 */
static void reset()
{
    entry_alias = NULL;
    protocol_combo = NULL;
    entry_hostname = NULL;
    entry_username = NULL;
    entry_password = NULL;
    entry_user_agent = NULL;
    entry_mailbox = NULL;
}

/* GtkCheckButton is derived from GtkToggleButton */
static void
auto_answer_cb(GtkToggleButton *widget, account_t *account)
{
    account_replace(account, CONFIG_ACCOUNT_AUTOANSWER,
                    gtk_toggle_button_get_active(widget) ? "true" : "false");
}

static void
user_agent_checkbox_cb(GtkToggleButton *widget, account_t *account)
{
    const gboolean is_active = gtk_toggle_button_get_active(widget);
    account_replace(account, CONFIG_ACCOUNT_HAS_CUSTOM_USERAGENT,
                    is_active ? "true" : "false");
    gtk_widget_set_sensitive(entry_user_agent, is_active);
}

/*
 * Display / Hide the password
 */
static void show_password_cb(G_GNUC_UNUSED GtkWidget *widget, gpointer data)
{
    gtk_entry_set_visibility(GTK_ENTRY(data), !gtk_entry_get_visibility(GTK_ENTRY(data)));
}

/* Signal to protocol_combo 'changed' */
static void change_protocol_cb(G_GNUC_UNUSED GtkWidget *widget, gpointer data)
{
    gchar *protocol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo));
    // Only if tabs are not NULL
    if (security_tab && advanced_tab) {
        if (utf8_case_equal(protocol, "IAX")) {
            gtk_widget_hide(security_tab);
            gtk_widget_hide(advanced_tab);
#ifdef SFL_PRESENCE
            gtk_widget_set_sensitive(presence_check_box, FALSE);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(presence_check_box),FALSE);
#endif
        } else {
            gtk_widget_show(security_tab);
            gtk_widget_show(advanced_tab);
#ifdef SFL_PRESENCE
            if (data) {
                account_t * account = data;
                // the presence can be enabled when at least 1 presence feature is supported by the PBX
                // OR when the account is new
                gtk_widget_set_sensitive(presence_check_box,
                        !g_strcmp0(account_lookup(account, CONFIG_PRESENCE_PUBLISH_SUPPORTED), "true") ||
                        !g_strcmp0(account_lookup(account, CONFIG_PRESENCE_SUBSCRIBE_SUPPORTED), "true") ||
                        is_account_new);
            }
#endif
        }
    }

    g_free(protocol);
}

void
select_dtmf_type(void)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(overrtp)))
        g_debug("Selected DTMF over RTP");
    else
        g_debug("Selected DTMF over SIP");
}

static GPtrArray* get_new_credential(void)
{
    GPtrArray *credential_array = g_ptr_array_new();

    GtkTreeIter iter;
    for (gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(credential_store), &iter);
         valid;
         valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(credential_store), &iter)) {
        gchar *username;
        gchar *realm;
        gchar *password;

        gtk_tree_model_get(GTK_TREE_MODEL(credential_store), &iter,
                           COLUMN_CREDENTIAL_REALM, &realm,
                           COLUMN_CREDENTIAL_USERNAME, &username,
                           COLUMN_CREDENTIAL_PASSWORD, &password,
                           -1);

        GHashTable * new_table = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                       g_free, g_free);
        g_hash_table_insert(new_table, g_strdup(CONFIG_ACCOUNT_REALM), realm);
        g_hash_table_insert(new_table, g_strdup(CONFIG_ACCOUNT_USERNAME), username);
        g_hash_table_insert(new_table, g_strdup(CONFIG_ACCOUNT_PASSWORD), password);

        g_ptr_array_add(credential_array, new_table);
    }

    return credential_array;
}

static void update_credential_cb(GtkWidget *widget, G_GNUC_UNUSED gpointer data)
{
    GtkTreeIter iter;

    if (credential_store && gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(credential_store), &iter, "0")) {
        gint column = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "column"));
        gtk_list_store_set(GTK_LIST_STORE(credential_store), &iter, column, gtk_entry_get_text(GTK_ENTRY(widget)), -1);
    }
}

static GtkWidget*
create_auto_answer_checkbox(const account_t *account)
{
    GtkWidget *auto_answer_checkbox = gtk_check_button_new_with_mnemonic(_("_Auto-answer calls"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(auto_answer_checkbox), account_has_autoanswer_on(account));
    g_signal_connect(auto_answer_checkbox, "toggled", G_CALLBACK(auto_answer_cb), (gpointer) account);
    return auto_answer_checkbox;
}

static GtkWidget*
create_user_agent_checkbox(const account_t *account)
{
    GtkWidget *user_agent_checkbox = gtk_check_button_new_with_mnemonic(_("_Use custom user-agent"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(user_agent_checkbox), account_has_custom_user_agent(account));
    g_signal_connect(user_agent_checkbox, "toggled", G_CALLBACK(user_agent_checkbox_cb), (gpointer) account);
    return user_agent_checkbox;
}

static void
alias_changed_cb(GtkEditable *editable, gpointer data)
{
    const gchar *alias = gtk_entry_get_text(GTK_ENTRY(editable));
    GtkDialog *dialog = GTK_DIALOG(data);
    gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_APPLY, !!strlen(alias));
}


static GtkWidget*
create_account_parameters(account_t *account, gboolean is_new, GtkWidget *dialog)
{
    g_assert(account);
    gchar *password = NULL;
    if (account_is_SIP(account)) {
        /* get password from credentials list */
        if (account->credential_information) {
            GHashTable * element = g_ptr_array_index(account->credential_information, 0);
            password = g_hash_table_lookup(element, CONFIG_ACCOUNT_PASSWORD);
        }
    } else
        password = account_lookup(account, CONFIG_ACCOUNT_PASSWORD);

    GtkWidget *frame = gnome_main_section_new(_("Account Parameters"));
    gtk_widget_show(frame);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_widget_show(grid);
    gtk_container_add(GTK_CONTAINER(frame), grid);

    GtkWidget *label = gtk_label_new_with_mnemonic(_("_Alias"));
    gint row = 0;
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_alias = gtk_entry_new();
    g_signal_connect(entry_alias, "changed", G_CALLBACK(alias_changed_cb), dialog);
    /* make sure Apply is not sensitive while alias is empty */
    g_signal_emit_by_name(entry_alias, "changed", NULL);

    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_alias);
    gchar *alias = account_lookup(account, CONFIG_ACCOUNT_ALIAS);
    gtk_entry_set_text(GTK_ENTRY(entry_alias), alias);
    gtk_grid_attach(GTK_GRID(grid), entry_alias, 1, row, 1, 1);

    row++;
    label = gtk_label_new_with_mnemonic(_("_Protocol"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    protocol_combo = gtk_combo_box_text_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), protocol_combo);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol_combo), "SIP");

    if (dbus_is_iax2_enabled())
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol_combo), "IAX");

    if (account_is_SIP(account))
        gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 0);
    else if (account_is_IAX(account))
        gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 1);
    else {
        g_warning("Account protocol not valid");
        /* Should never come here, add debug message. */
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol_combo), _("Unknown"));
        gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 2);
    }

    /* Can't change account type after creation */
    if (!is_new)
        gtk_widget_set_sensitive(protocol_combo, FALSE);

    gtk_grid_attach(GTK_GRID(grid), protocol_combo, 1, row, 1, 1);

    /* Link signal 'changed' */
    g_signal_connect(G_OBJECT(GTK_COMBO_BOX(protocol_combo)), "changed",
                     G_CALLBACK(change_protocol_cb), account);

    row++;
    label = gtk_label_new_with_mnemonic(_("_Host name"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_hostname = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_hostname);
    const gchar *hostname = account_lookup(account, CONFIG_ACCOUNT_HOSTNAME);
    gtk_entry_set_text(GTK_ENTRY(entry_hostname), hostname);
    gtk_grid_attach(GTK_GRID(grid), entry_hostname, 1, row, 1, 1);

    row++;
    label = gtk_label_new_with_mnemonic(_("_User name"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_username = gtk_entry_new();
    const gchar *PERSON_IMG = ICONS_DIR "/stock_person.svg";
    gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(entry_username),
                                   GTK_ENTRY_ICON_PRIMARY,
                                   gdk_pixbuf_new_from_file(PERSON_IMG, NULL));
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_username);
    gchar *username = account_lookup(account, CONFIG_ACCOUNT_USERNAME);
    gtk_entry_set_text(GTK_ENTRY(entry_username), username);
    gtk_grid_attach(GTK_GRID(grid), entry_username, 1, row, 1, 1);

    if (account_is_SIP(account)) {
        g_signal_connect(G_OBJECT(entry_username), "changed",
                         G_CALLBACK(update_credential_cb), NULL);
        g_object_set_data(G_OBJECT(entry_username), "column",
                          GINT_TO_POINTER(COLUMN_CREDENTIAL_USERNAME));
    }

    row++;
    label = gtk_label_new_with_mnemonic(_("_Password"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_password = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry_password),
            GTK_ENTRY_ICON_PRIMARY, "dialog-password");

    gtk_entry_set_visibility(GTK_ENTRY(entry_password), FALSE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_password);
    password = password ? password : "";
    gtk_entry_set_text(GTK_ENTRY(entry_password), password);
    gtk_grid_attach(GTK_GRID(grid), entry_password, 1, row, 1, 1);

    if (account_is_SIP(account)) {
        g_signal_connect(G_OBJECT(entry_password), "changed", G_CALLBACK(update_credential_cb), NULL);
        g_object_set_data(G_OBJECT(entry_password), "column", GINT_TO_POINTER(COLUMN_CREDENTIAL_PASSWORD));
    }

    row++;
    GtkWidget *clearTextcheck_box = gtk_check_button_new_with_mnemonic(_("Show password"));
    g_signal_connect(clearTextcheck_box, "toggled", G_CALLBACK(show_password_cb), entry_password);
    gtk_grid_attach(GTK_GRID(grid), clearTextcheck_box, 1, row, 1, 1);

    row++;
    label = gtk_label_new_with_mnemonic(_("_Proxy"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_route_set = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_route_set);
    gchar *route_set = account_lookup(account, CONFIG_ACCOUNT_ROUTESET);
    gtk_entry_set_text(GTK_ENTRY(entry_route_set), route_set);
    gtk_grid_attach(GTK_GRID(grid), entry_route_set, 1, row, 1, 1);

    row++;
    label = gtk_label_new_with_mnemonic(_("_Voicemail number"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_mailbox = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_mailbox);
    gchar *mailbox = account_lookup(account, CONFIG_ACCOUNT_MAILBOX);
    mailbox = mailbox ? mailbox : "";
    gtk_entry_set_text(GTK_ENTRY(entry_mailbox), mailbox);
    gtk_grid_attach(GTK_GRID(grid), entry_mailbox, 1, row, 1, 1);

    row++;
    label = gtk_label_new_with_mnemonic(_("_User-agent"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_user_agent = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_user_agent);
    gchar *user_agent = account_lookup(account, CONFIG_ACCOUNT_USERAGENT);
    gtk_entry_set_text(GTK_ENTRY(entry_user_agent), user_agent);
    gtk_grid_attach(GTK_GRID(grid), entry_user_agent, 1, row, 1, 1);

    gtk_widget_set_sensitive(entry_user_agent, account_has_custom_user_agent(account));

    row++;
    GtkWidget *user_agent_checkbox = create_user_agent_checkbox(account);
    gtk_grid_attach(GTK_GRID(grid), user_agent_checkbox, 0, row, 1, 1);

    row++;
    GtkWidget *auto_answer_checkbox = create_auto_answer_checkbox(account);
    gtk_grid_attach(GTK_GRID(grid), auto_answer_checkbox, 0, row, 1, 1);


    gtk_widget_show_all(grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    return frame;
}

#ifdef SFL_PRESENCE
static GtkWidget*
create_presence_checkbox(const account_t *account)
{
    g_assert(account);

    GtkWidget *frame = gnome_main_section_new(_("Presence notifications"));
    gtk_widget_show(frame);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_widget_show(grid);
    gtk_container_add(GTK_CONTAINER(frame), grid);

    presence_check_box = gtk_check_button_new_with_mnemonic(_("_Enable"));

    gboolean enabled = (g_strcmp0(account_lookup(account, CONFIG_PRESENCE_ENABLED), "true")==0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(presence_check_box),enabled);
    // sensitivity is set later in change_protocol_cb

    gtk_grid_attach(GTK_GRID(grid), presence_check_box, 0, 0, 1, 1);

    gtk_widget_show_all(grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    return frame;
}
#endif

static GtkWidget*
create_basic_tab(account_t *account, gboolean is_new, GtkWidget *dialog)
{
    // Build the advanced tab, to appear on the account configuration panel
    g_debug("Build basic tab");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget *frame = create_account_parameters(account, is_new, dialog);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

#ifdef SFL_PRESENCE
    frame = create_presence_checkbox(account);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    is_account_new = is_new; // need a copy for a cb
#endif

    gtk_widget_show_all(vbox);

    return vbox;
}


static void fill_treeview_with_credential(const account_t * account)
{
    GtkTreeIter iter;
    gtk_list_store_clear(credential_store);

    for (unsigned i = 0; i < account->credential_information->len; i++) {
        GHashTable * element = g_ptr_array_index(account->credential_information, i);
        gtk_list_store_append(credential_store, &iter);
        gtk_list_store_set(credential_store, &iter, COLUMN_CREDENTIAL_REALM, g_hash_table_lookup(element, CONFIG_ACCOUNT_REALM),
                           COLUMN_CREDENTIAL_USERNAME, g_hash_table_lookup(element, CONFIG_ACCOUNT_USERNAME),
                           COLUMN_CREDENTIAL_PASSWORD, g_hash_table_lookup(element, CONFIG_ACCOUNT_PASSWORD),
                           COLUMN_CREDENTIAL_DATA, element, -1);
    }
}

static void select_credential_cb(GtkTreeSelection *selection, GtkTreeModel *model)
{
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
        GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

        const gboolean sensitive = gtk_tree_path_get_indices(path)[0] != 0;
        gtk_widget_set_sensitive(delete_cred_button, sensitive);
    }
}

static void add_credential_cb(G_GNUC_UNUSED GtkWidget *button, gpointer data)
{
    GtkTreeModel *model = (GtkTreeModel *) data;

    GtkTreeIter iter;
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COLUMN_CREDENTIAL_REALM, "*",
                       COLUMN_CREDENTIAL_USERNAME, _("Authentication"),
                       COLUMN_CREDENTIAL_PASSWORD, _("Secret"), -1);
}

static void
delete_credential_cb(G_GNUC_UNUSED GtkWidget *button, gpointer data)
{
    GtkTreeIter iter;
    GtkTreeView *treeview = (GtkTreeView *) data;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);

    if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
        GtkTreePath *path;
        path = gtk_tree_model_get_path(model, &iter);
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        gtk_tree_path_free(path);
    }
}

static void
cell_edited_cb(GtkCellRendererText *renderer, gchar *path_desc, gchar *text,
               gpointer data)
{
    GtkTreeModel *model =(GtkTreeModel *) data;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_desc);

    gint column = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(renderer), "column"));
    g_debug("path desc: %s\n", text);

    if ((utf8_case_equal(path_desc, "0")) &&
        !utf8_case_equal(text, gtk_entry_get_text(GTK_ENTRY(entry_username))))
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry_username),
                                             G_CALLBACK(update_credential_cb),
                                             NULL);

    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, column, text, -1);
    gtk_tree_path_free(path);
}

static void
editing_started_cb(G_GNUC_UNUSED GtkCellRenderer *cell, GtkCellEditable * editable,
                   const gchar * path, G_GNUC_UNUSED gpointer data)
{
    g_debug("path desc: %s\n", path);

    // If we are dealing the first row
    if (utf8_case_equal(path, "0"))
        gtk_entry_set_text(GTK_ENTRY(editable), gtk_entry_get_text(GTK_ENTRY(entry_password)));
}

static void show_advanced_zrtp_options_cb(G_GNUC_UNUSED GtkWidget *widget, OptionsData *data)
{
    gchar *proto = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(key_exchange_combo));

    if (utf8_case_equal(proto, "ZRTP"))
        show_advanced_zrtp_options(data->account, data->client);
    else
        show_advanced_sdes_options(data->account, data->client);

    g_free(proto);
}


static void
show_advanced_tls_options_cb(G_GNUC_UNUSED GtkWidget *widget, OptionsData *data)
{
    show_advanced_tls_options(data->account, data->client);
}

static void
key_exchange_changed_cb(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gpointer data)
{
    gchar *active_text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(key_exchange_combo));
    g_debug("Key exchange changed %s", active_text);

    gboolean sensitive = FALSE;
    sensitive |= utf8_case_equal(active_text, "SDES");
    sensitive |= utf8_case_equal(active_text, "ZRTP");
    g_free(active_text);
    gtk_widget_set_sensitive(zrtp_button, sensitive);
}

static void use_sip_tls_cb(GtkWidget *widget, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        g_debug("Using sips");
        gtk_widget_set_sensitive(data, TRUE);
        // Uncheck stun
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_stun_check_box), FALSE);
        gtk_widget_set_sensitive(use_stun_check_box, FALSE);
        gtk_widget_set_sensitive(same_as_local_radio_button, TRUE);
        gtk_widget_set_sensitive(published_addr_radio_button, TRUE);
        gtk_widget_set_sensitive(stun_server_entry, FALSE);

        if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button))) {
            gtk_widget_show(published_address_entry);
            gtk_widget_show(published_port_spin_box);
            gtk_widget_show(published_address_label);
            gtk_widget_show(published_port_label);
        }
    } else {
        gtk_widget_set_sensitive(data, FALSE);
        gtk_widget_set_sensitive(use_stun_check_box, TRUE);

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(use_stun_check_box))) {
            gtk_widget_set_sensitive(same_as_local_radio_button, FALSE);
            gtk_widget_set_sensitive(published_addr_radio_button, FALSE);
            gtk_widget_show(stun_server_label);
            gtk_widget_show(stun_server_entry);
            gtk_widget_set_sensitive(stun_server_entry, TRUE);
            gtk_widget_hide(published_address_entry);
            gtk_widget_hide(published_port_spin_box);
            gtk_widget_hide(published_address_label);
            gtk_widget_hide(published_port_label);
        } else {
            gtk_widget_set_sensitive(same_as_local_radio_button, TRUE);
            gtk_widget_set_sensitive(published_addr_radio_button, TRUE);
            gtk_widget_set_sensitive(stun_server_entry, FALSE);
        }
    }
}

static gchar *
get_interface_addr_from_name(const gchar * const iface_name)
{
    g_assert(iface_name);
#define UC(b) (((int)b)&0xff)

    int fd;

    if ((fd = socket(AF_INET, SOCK_DGRAM,0)) < 0) {
        g_warning("could not open socket: %s", g_strerror(errno));
        return g_strdup("");
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));

    strncpy(ifr.ifr_name, iface_name, sizeof ifr.ifr_name);
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';
    ifr.ifr_addr.sa_family = AF_INET;

    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0)
        g_debug("getInterfaceAddrFromName use default interface (0.0.0.0)\n");


    struct sockaddr_in *saddr_in = (struct sockaddr_in *) &ifr.ifr_addr;
    struct in_addr *addr_in = &(saddr_in->sin_addr);

    char *tmp_addr = (char *) addr_in;

    gchar *iface_addr = g_strdup_printf("%d.%d.%d.%d", UC(tmp_addr[0]),
                                        UC(tmp_addr[1]), UC(tmp_addr[2]), UC(tmp_addr[3]));

    close(fd);
    return iface_addr;
#undef UC
}

static void local_interface_changed_cb(G_GNUC_UNUSED GtkWidget * widget, G_GNUC_UNUSED gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button))) {
        gchar *local_iface_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(local_address_combo));
        if (!local_iface_name) {
            g_warning("Could not get local interface name");
            return;
        }
        gchar *local_iface_addr = get_interface_addr_from_name(local_iface_name);

        gtk_entry_set_text(GTK_ENTRY(local_address_entry), local_iface_addr);
        gtk_entry_set_text(GTK_ENTRY(published_address_entry), local_iface_addr);
        g_free(local_iface_addr);
        g_free(local_iface_name);
    }
}

static void set_published_addr_manually_cb(GtkWidget * widget, G_GNUC_UNUSED gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        g_debug("Showing manual publishing options");
        gtk_widget_show(published_port_label);
        gtk_widget_show(published_port_spin_box);
        gtk_widget_show(published_address_label);
        gtk_widget_show(published_address_entry);
    } else {
        g_debug("Hiding manual publishing options");
        gtk_widget_hide(published_port_label);
        gtk_widget_hide(published_port_spin_box);
        gtk_widget_hide(published_address_label);
        gtk_widget_hide(published_address_entry);
    }
}

static void
use_stun_cb(GtkWidget *widget, G_GNUC_UNUSED gpointer data)
{
    /* Widgets have not been created yet */
    g_return_if_fail(stun_server_label || stun_server_entry ||
                     same_as_local_radio_button ||
                     published_addr_radio_button || published_address_label ||
                     published_port_label || published_address_entry ||
                     published_port_spin_box);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        g_debug("Showing stun options, hiding Local/Published info");
        gtk_widget_show(stun_server_label);
        gtk_widget_show(stun_server_entry);
        gtk_widget_set_sensitive(stun_server_entry, TRUE);
        gtk_widget_set_sensitive(same_as_local_radio_button, FALSE);
        gtk_widget_set_sensitive(published_addr_radio_button, FALSE);

        gtk_widget_hide(published_address_label);
        gtk_widget_hide(published_port_label);
        gtk_widget_hide(published_address_entry);
        gtk_widget_hide(published_port_spin_box);
    } else {
        g_debug("disabling stun options, showing Local/Published info");
        gtk_widget_set_sensitive(stun_server_entry, FALSE);
        gtk_widget_set_sensitive(same_as_local_radio_button, TRUE);
        gtk_widget_set_sensitive(published_addr_radio_button, TRUE);

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(published_addr_radio_button))) {
            gtk_widget_show(published_address_label);
            gtk_widget_show(published_port_label);
            gtk_widget_show(published_address_entry);
            gtk_widget_show(published_port_spin_box);
        }
    }
}


static void same_as_local_cb(GtkWidget * widget, G_GNUC_UNUSED gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        gchar *local_interface = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(local_address_combo));
        gchar *local_address = dbus_get_address_from_interface_name(local_interface);

        gtk_entry_set_text(GTK_ENTRY(published_address_entry), local_address);

        const gchar * local_port = gtk_entry_get_text(GTK_ENTRY(local_port_spin_box));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(published_port_spin_box), g_ascii_strtod(local_port, NULL));
        g_free(local_interface);
    }
}



static GtkWidget* create_credential_widget(const account_t *account)
{
    /* Credentials tree view */
    GtkWidget *frame, *grid;
    gnome_main_section_new_with_grid(_("Credential"), &frame, &grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);

    GtkWidget *scrolled_window_credential = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window_credential), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window_credential), GTK_SHADOW_IN);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window_credential, 0, 0, 1, 1);

    credential_store = gtk_list_store_new(COLUMN_CREDENTIAL_COUNT,
                                         G_TYPE_STRING,  // Realm
                                         G_TYPE_STRING,  // Username
                                         G_TYPE_STRING,  // Password
                                         G_TYPE_POINTER  // Pointer to the Objectc
                                         );

    treeview_credential = gtk_tree_view_new_with_model(GTK_TREE_MODEL(credential_store));
    GtkTreeSelection * tree_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_credential));
    g_signal_connect(G_OBJECT(tree_selection), "changed", G_CALLBACK(select_credential_cb), credential_store);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, NULL);
    g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(cell_edited_cb), credential_store);
    g_object_set_data(G_OBJECT(renderer), "column", GINT_TO_POINTER(COLUMN_CREDENTIAL_REALM));

    GtkTreeViewColumn *tree_view_column = gtk_tree_view_column_new_with_attributes("Realm", renderer, "markup", COLUMN_CREDENTIAL_REALM, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_credential), tree_view_column);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, NULL);
    g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(cell_edited_cb), credential_store);
    g_object_set_data(G_OBJECT(renderer), "column", GINT_TO_POINTER(COLUMN_CREDENTIAL_USERNAME));
    tree_view_column = gtk_tree_view_column_new_with_attributes(_("Authentication name"),
                     renderer,
                     "markup", COLUMN_CREDENTIAL_USERNAME,
                     NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_credential), tree_view_column);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, NULL);
    g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(cell_edited_cb), credential_store);
    g_signal_connect(renderer, "editing-started", G_CALLBACK(editing_started_cb), NULL);
    g_object_set_data(G_OBJECT(renderer), "column", GINT_TO_POINTER(COLUMN_CREDENTIAL_PASSWORD));
    tree_view_column = gtk_tree_view_column_new_with_attributes(_("Password"),
                     renderer, "markup", COLUMN_CREDENTIAL_PASSWORD, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_credential), tree_view_column);

    gtk_container_add(GTK_CONTAINER(scrolled_window_credential), treeview_credential);

    fill_treeview_with_credential(account);

    /* Credential Buttons */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    /* 2x1 */
    gtk_grid_attach(GTK_GRID(grid), hbox, 0, 1, 2, 1);

    GtkWidget *addButton = gtk_button_new_with_label(_("Add"));
    g_signal_connect(addButton, "clicked", G_CALLBACK(add_credential_cb), credential_store);
    gtk_box_pack_start(GTK_BOX(hbox), addButton, FALSE, FALSE, 0);

    delete_cred_button = gtk_button_new_with_label(_("Remove"));
    g_signal_connect(delete_cred_button, "clicked", G_CALLBACK(delete_credential_cb), treeview_credential);
    gtk_box_pack_start(GTK_BOX(hbox), delete_cred_button, FALSE, FALSE, 0);

    /* Dynamically resize the window to fit the scrolled window */
    gtk_widget_set_size_request(scrolled_window_credential, 400, 120);

    return frame;
}


static GtkWidget*
create_security_widget(account_t *account, SFLPhoneClient *client)
{
    gchar *curSRTPEnabled = NULL, *curKeyExchange = NULL,
          *curTLSEnabled = NULL;

    // Load from SIP/IAX/Unknown ?
    if (account && account->properties) {
        curKeyExchange = account_lookup(account, CONFIG_SRTP_KEY_EXCHANGE);
        if (curKeyExchange == NULL)
            curKeyExchange = "none";

        curSRTPEnabled = account_lookup(account, CONFIG_SRTP_ENABLE);

        if (curSRTPEnabled == NULL)
            curSRTPEnabled = "false";

        curTLSEnabled = account_lookup(account, CONFIG_TLS_ENABLE);

        if (curTLSEnabled == NULL)
            curTLSEnabled = "false";
    }

    GtkWidget *frame, *grid;
    gnome_main_section_new_with_grid(_("Security"), &frame, &grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    /* TLS subsection */
    OptionsData *options = g_new0(OptionsData, 1);
    options->account = account;
    options->client = client;
    GtkWidget *sip_tls_advanced_button = gtk_button_new_with_label(_("Edit"));
    gtk_grid_attach(GTK_GRID(grid), sip_tls_advanced_button, 2, 0, 1, 1);
    gtk_widget_set_sensitive(sip_tls_advanced_button, FALSE);
    g_signal_connect(G_OBJECT(sip_tls_advanced_button), "clicked",
                     G_CALLBACK(show_advanced_tls_options_cb),
                     options);

    use_sip_tls_check_box = gtk_check_button_new_with_mnemonic(_("Use TLS transport(sips)"));
    g_signal_connect(use_sip_tls_check_box, "toggled", G_CALLBACK(use_sip_tls_cb), sip_tls_advanced_button);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_sip_tls_check_box),
                                 g_strcmp0(curTLSEnabled, "true") == 0);
    /* 2x1 */
    gtk_grid_attach(GTK_GRID(grid), use_sip_tls_check_box, 0, 0, 2, 1);

    /* ZRTP subsection */
    GtkWidget *label = gtk_label_new_with_mnemonic(_("SRTP key exchange"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    key_exchange_combo = gtk_combo_box_text_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), key_exchange_combo);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(key_exchange_combo), "ZRTP");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(key_exchange_combo), "SDES");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(key_exchange_combo), _("Disabled"));

    zrtp_button = gtk_button_new_with_label(_("Preferences"));
    g_signal_connect(G_OBJECT(zrtp_button), "clicked",
                     G_CALLBACK(show_advanced_zrtp_options_cb),
                     options);

    if (g_strcmp0(curSRTPEnabled, "false") == 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(key_exchange_combo), 2);
        gtk_widget_set_sensitive(zrtp_button, FALSE);
    } else {
        if (g_strcmp0(curKeyExchange, ZRTP) == 0)
            gtk_combo_box_set_active(GTK_COMBO_BOX(key_exchange_combo),0);
        else if (g_strcmp0(curKeyExchange, SDES) == 0)
            gtk_combo_box_set_active(GTK_COMBO_BOX(key_exchange_combo),1);
        else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(key_exchange_combo), 2);
            gtk_widget_set_sensitive(zrtp_button, FALSE);
        }
    }

    g_signal_connect(G_OBJECT(GTK_COMBO_BOX(key_exchange_combo)), "changed",
                     G_CALLBACK(key_exchange_changed_cb), NULL);

    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), key_exchange_combo, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), zrtp_button, 2, 1, 1, 1);

    gtk_widget_show_all(grid);

    return frame;
}


static GtkWidget *
create_security_tab(account_t *account, SFLPhoneClient *client)
{
    GtkWidget * ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    GtkWidget *frame;

    if (!account_is_IP2IP(account)) {
        // Credentials frame
        frame = create_credential_widget(account);
        gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);
    }

    // Security frame
    frame = create_security_widget(account, client);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    gtk_widget_show_all(ret);

    return ret;
}

static GtkWidget* create_registration_expire(const account_t *account)
{
    gchar *account_expire = NULL;
    void *orig_key = NULL;
    if (account && account->properties)
        if (!g_hash_table_lookup_extended(account->properties, CONFIG_ACCOUNT_REGISTRATION_EXPIRE,
                                          &orig_key, (gpointer) &account_expire))
            g_warning("Could not retrieve %s from account properties",
                  CONFIG_ACCOUNT_REGISTRATION_EXPIRE);

    GtkWidget *grid, *frame;
    gnome_main_section_new_with_grid(_("Registration"), &frame, &grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);

    GtkWidget *label = gtk_label_new_with_mnemonic(_("Registration expire"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    expire_spin_box = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), expire_spin_box);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(expire_spin_box), g_ascii_strtod(account_expire, NULL));
    gtk_grid_attach(GTK_GRID(grid), expire_spin_box, 1, 0, 1, 1);

    return frame;
}

static GtkWidget*
create_network(const account_t *account)
{
    gchar *local_interface = NULL;
    gchar *local_port = NULL;

    if (account) {
        local_interface = account_lookup(account, CONFIG_LOCAL_INTERFACE);
        local_port = account_lookup(account, CONFIG_LOCAL_PORT);
    }

    GtkWidget *grid, *frame;
    gnome_main_section_new_with_grid(_("Network Interface"), &frame, &grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);

    /**
     * Retrieve the list of IP interface from the
     * the daemon and build the combo box.
     */
    local_address_combo = gtk_combo_box_text_new();

    GtkWidget *label = gtk_label_new_with_mnemonic(_("Local address"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    gchar **iface_list = dbus_get_all_ip_interface_by_name();

    int idx = 0;
    for (gchar **iface = iface_list; iface && *iface; iface++, idx++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(local_address_combo), *iface);
        if (g_strcmp0(*iface, local_interface) == 0)
            gtk_combo_box_set_active(GTK_COMBO_BOX(local_address_combo), idx);
    }
    if (!local_interface)
        gtk_combo_box_set_active(GTK_COMBO_BOX(local_address_combo), 0);

    gtk_label_set_mnemonic_widget(GTK_LABEL(label), local_address_combo);
    gtk_grid_attach(GTK_GRID(grid), local_address_combo, 1, 0, 1, 1);

    // Fill the text entry with the ip address of local interface selected
    local_address_entry = gtk_entry_new();
    gchar *local_iface_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(local_address_combo));
    if (!local_iface_name) {
        g_warning("Could not get local interface name");
        return frame;
    }
    gchar *local_iface_addr = get_interface_addr_from_name(local_iface_name);
    g_free(local_iface_name);
    gtk_entry_set_text(GTK_ENTRY(local_address_entry), local_iface_addr);
    g_free(local_iface_addr);
    gtk_widget_set_sensitive(local_address_entry, FALSE);
    gtk_grid_attach(GTK_GRID(grid), local_address_entry, 2, 0, 1, 1);

    // Local port widget
    label = gtk_label_new_with_mnemonic(_("Local port"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    local_port_spin_box = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), local_port_spin_box);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(local_port_spin_box), g_ascii_strtod(local_port, NULL));

    gtk_grid_attach(GTK_GRID(grid), local_port_spin_box, 1, 1, 1, 1);

    return frame;
}

GtkWidget* create_published_address(const account_t *account)
{
    GtkWidget *frame;
    gchar *use_tls = NULL;
    gchar *published_address = NULL;
    gchar *published_port = NULL;
    gchar *stun_enable = NULL;
    gchar *stun_server = NULL;
    gchar *published_sameas_local = NULL;

    // Get the user configuration
    if (account) {
        use_tls = account_lookup(account, CONFIG_TLS_ENABLE);
        published_sameas_local = account_lookup(account, CONFIG_PUBLISHED_SAMEAS_LOCAL);

        if (utf8_case_equal(published_sameas_local, "true")) {
            published_address = dbus_get_address_from_interface_name(account_lookup(account, CONFIG_LOCAL_INTERFACE));
            published_port = account_lookup(account, CONFIG_LOCAL_PORT);
        } else {
            published_address = account_lookup(account, CONFIG_PUBLISHED_ADDRESS);
            published_port = account_lookup(account, CONFIG_PUBLISHED_PORT);
        }

        stun_enable = account_lookup(account, CONFIG_STUN_ENABLE);
        stun_server = account_lookup(account, CONFIG_STUN_SERVER);
        published_sameas_local = account_lookup(account, CONFIG_PUBLISHED_SAMEAS_LOCAL);
    }

    GtkWidget *grid;
    gnome_main_section_new_with_grid(_("Published address"), &frame, &grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);

    use_stun_check_box = gtk_check_button_new_with_mnemonic(_("Using STUN"));
    gtk_grid_attach(GTK_GRID(grid), use_stun_check_box, 0, 0, 1, 1);
    g_signal_connect(use_stun_check_box, "toggled", G_CALLBACK(use_stun_cb), NULL);

    stun_server_label = gtk_label_new_with_mnemonic(_("STUN server URL"));
    gtk_grid_attach(GTK_GRID(grid), stun_server_label, 0, 1, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(stun_server_label), 0, 0.5);
    stun_server_entry = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(stun_server_label), stun_server_entry);
    gtk_entry_set_text(GTK_ENTRY(stun_server_entry), stun_server);
    gtk_grid_attach(GTK_GRID(grid), stun_server_entry, 1, 1, 1, 1);

    same_as_local_radio_button = gtk_radio_button_new_with_mnemonic_from_widget(NULL, _("Same as local parameters"));
    /* 2x1 */
    gtk_grid_attach(GTK_GRID(grid), same_as_local_radio_button, 0, 3, 2, 1);

    published_addr_radio_button = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(same_as_local_radio_button), _("Set published address and port:"));
    /* 2x1 */
    gtk_grid_attach(GTK_GRID(grid), published_addr_radio_button, 0, 4, 2, 1);

    if (utf8_case_equal(published_sameas_local, "true")) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(published_addr_radio_button), FALSE);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button), FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(published_addr_radio_button), TRUE);
    }

    published_address_label = gtk_label_new_with_mnemonic(_("Published address"));
    gtk_grid_attach(GTK_GRID(grid), published_address_label, 0, 5, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(published_address_label), 0, 0.5);
    published_address_entry = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(published_address_label), published_address_entry);

    gtk_entry_set_text(GTK_ENTRY(published_address_entry), published_address);
    gtk_grid_attach(GTK_GRID(grid), published_address_entry, 1, 5, 1, 1);

    published_port_label = gtk_label_new_with_mnemonic(_("Published port"));
    gtk_grid_attach(GTK_GRID(grid), published_port_label, 0, 6, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(published_port_label), 0, 0.5);
    published_port_spin_box = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(published_port_label), published_port_spin_box);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(published_port_spin_box), g_ascii_strtod(published_port, NULL));

    gtk_grid_attach(GTK_GRID(grid), published_port_spin_box, 1, 6, 1, 1);

    // This will trigger a signal, and the above two
    // widgets need to be instanciated before that.
    g_signal_connect(local_address_combo, "changed", G_CALLBACK(local_interface_changed_cb), local_address_combo);
    g_signal_connect(same_as_local_radio_button, "toggled", G_CALLBACK(same_as_local_cb), same_as_local_radio_button);
    g_signal_connect(published_addr_radio_button, "toggled", G_CALLBACK(set_published_addr_manually_cb), published_addr_radio_button);

    /* Now that widgets have been initialized it's safe to invoke this signal */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_stun_check_box),
                                 utf8_case_equal(stun_enable, "true"));
    gtk_widget_set_sensitive(use_stun_check_box, !utf8_case_equal(use_tls, "true"));

    set_published_addr_manually_cb(published_addr_radio_button, NULL);

    return frame;
}

static void
add_port_spin_button(const account_t *account, GtkWidget *grid, GtkWidget **spin,
                 const gchar *key, const gchar *label_text, int left, int top)
{
    gchar *value = NULL;

    if (account)
        value = account_lookup(account, key);

    *spin = gtk_spin_button_new_with_range(1024, 65535, 1);
    GtkWidget *label = gtk_label_new_with_mnemonic(label_text);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), *spin);
    gtk_grid_attach(GTK_GRID(grid), label, left, top, 1, 1);
    if (value)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(*spin), g_ascii_strtod(value, NULL));
    gtk_grid_attach(GTK_GRID(grid), *spin, left + 1, top, 1, 1);
}

static GtkWidget*
create_port_ranges(const account_t *account, const gchar * title,
                   GtkWidget **min, GtkWidget **max,
                   const gchar *key_min, const gchar *key_max, int top)
{
    GtkWidget *grid, *frame;
    gnome_main_section_new_with_grid(title, &frame, &grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);

    add_port_spin_button(account, grid, min, key_min, _("Min"), 0, top);
    add_port_spin_button(account, grid, max, key_max, _("Max"), 2, top);

    return frame;
}

GtkWidget* create_advanced_tab(const account_t *account)
{
    // Build the advanced tab, to appear on the account configuration panel
    g_debug("Build advanced tab");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget *frame;

    if (!account_is_IP2IP(account)) {
        frame = create_registration_expire(account);
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    }

    frame = create_network(account);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    frame = create_published_address(account);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    frame = create_port_ranges(account, _("Audio RTP Port Range"),
            &audio_port_min_spin_box, &audio_port_max_spin_box,
            CONFIG_ACCOUNT_AUDIO_PORT_MIN, CONFIG_ACCOUNT_AUDIO_PORT_MAX, 0);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

#ifdef SFL_VIDEO
    frame = create_port_ranges(account, _("Video RTP Port Range"),
            &video_port_min_spin_box, &video_port_max_spin_box,
            CONFIG_ACCOUNT_VIDEO_PORT_MIN, CONFIG_ACCOUNT_VIDEO_PORT_MAX, 0);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
#endif

    gtk_widget_show_all(vbox);

    use_stun_cb(use_stun_check_box, NULL);

    set_published_addr_manually_cb(published_addr_radio_button, NULL);

    return vbox;
}

static void ringtone_enabled_cb(G_GNUC_UNUSED GtkWidget *widget, gpointer data, G_GNUC_UNUSED const gchar *accountID)
{
    /* toggle sensitivity */
    gtk_widget_set_sensitive(data, !gtk_widget_is_sensitive(data));
}

void update_ringtone_slider(guint position, guint size)
{
    if (ringtone_seekslider && GTK_IS_WIDGET(ringtone_seekslider))
        sfl_seekslider_update_scale(SFL_SEEKSLIDER(ringtone_seekslider), position, size);
}

static void ringtone_changed_cb(GtkWidget *widget, gpointer data)
{
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(widget);
    SFLSeekSlider *seekslider = data;
    g_object_set(seekslider, "file-path", gtk_file_chooser_get_filename(chooser), NULL);
}

static GtkWidget*
create_audiocodecs_configuration(const account_t *account)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget *box = audiocodecs_box(account);

    // Box for the audiocodecs
    GtkWidget *audiocodecs = gnome_main_section_new(_("Audio"));
    gtk_box_pack_start(GTK_BOX(vbox), audiocodecs, FALSE, FALSE, 0);
    gtk_widget_set_size_request(audiocodecs, -1, 200);
    gtk_widget_show(audiocodecs);
    gtk_container_add(GTK_CONTAINER(audiocodecs), box);

    // Add DTMF type selection for SIP account only
    GtkWidget *grid;

    if (account_is_SIP(account)) {
        // Box for dtmf
        GtkWidget *dtmf;
        gnome_main_section_new_with_grid(_("DTMF"), &dtmf, &grid);
        gtk_box_pack_start(GTK_BOX(vbox), dtmf, FALSE, FALSE, 0);
        gtk_widget_show(dtmf);

        overrtp = gtk_radio_button_new_with_label(NULL, _("RTP"));
        const gchar * const dtmf_type = account_lookup(account, CONFIG_ACCOUNT_DTMF_TYPE);
        const gboolean dtmf_are_rtp = utf8_case_equal(dtmf_type, OVERRTP);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(overrtp), dtmf_are_rtp);
        gtk_grid_attach(GTK_GRID(grid), overrtp, 0, 0, 1, 1);

        GtkWidget *sipinfo = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(overrtp),  _("SIP"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sipinfo), !dtmf_are_rtp);
        g_signal_connect(G_OBJECT(sipinfo), "clicked", G_CALLBACK(select_dtmf_type), NULL);
        gtk_grid_attach(GTK_GRID(grid), sipinfo, 1, 0, 1, 1);
    }

    // Box for the ringtones
    GtkWidget *frame;
    gnome_main_section_new_with_grid(_("Ringtones"), &frame, &grid);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    file_chooser = gtk_file_chooser_button_new(_("Choose a ringtone"), GTK_FILE_CHOOSER_ACTION_OPEN);

    gpointer ptr = account_lookup(account, CONFIG_RINGTONE_ENABLED);
    enable_tone = gtk_check_button_new_with_mnemonic(_("_Enable ringtones"));
    const gboolean ringtone_enabled = g_strcmp0(ptr, "true") == 0;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_tone), ringtone_enabled);
    g_signal_connect(G_OBJECT(enable_tone), "clicked", G_CALLBACK(ringtone_enabled_cb), file_chooser);
    gtk_grid_attach(GTK_GRID(grid), enable_tone, 0, 0, 1, 1);

    // file chooser button
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser), g_get_home_dir());
    ptr = account_lookup(account, CONFIG_RINGTONE_PATH);
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_chooser), ptr);
    gtk_widget_set_sensitive(file_chooser, ringtone_enabled);

    // button to preview ringtones
    ringtone_seekslider = GTK_WIDGET(sfl_seekslider_new());
    g_object_set(G_OBJECT(ringtone_seekslider), "file-path", ptr, NULL);
    g_signal_connect(G_OBJECT(file_chooser), "selection-changed", G_CALLBACK(ringtone_changed_cb), ringtone_seekslider);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Audio Files"));
    gtk_file_filter_add_pattern(filter, "*.wav");
    gtk_file_filter_add_pattern(filter, "*.ul");
    gtk_file_filter_add_pattern(filter, "*.au");
    gtk_file_filter_add_pattern(filter, "*.flac");
    gtk_file_filter_add_pattern(filter, "*.ogg");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), filter);
    gtk_grid_attach(GTK_GRID(grid), file_chooser, 0, 1, 1, 1);

    GtkWidget *alignment =  gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0, 6, 6);
    gtk_container_add(GTK_CONTAINER(alignment), ringtone_seekslider);
    gtk_box_pack_start(GTK_BOX(vbox), alignment, FALSE, FALSE, 0);

    gtk_widget_show_all(vbox);

    return vbox;
}

#ifdef SFL_VIDEO
static GtkWidget *
create_videocodecs_configuration(const account_t *a)
{
    // Main widget
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget *box = videocodecs_box(a);

    // Box for the videocodecs
    GtkWidget *videocodecs = gnome_main_section_new(_("Video"));
    gtk_box_pack_start(GTK_BOX (vbox), videocodecs, FALSE, FALSE, 0);
    gtk_widget_set_size_request(GTK_WIDGET (videocodecs), -1, 200);
    gtk_widget_show(videocodecs);
    gtk_container_add(GTK_CONTAINER (videocodecs), box);

    /* Check button to enable/disable video for an account */
    gpointer ptr = account_lookup(a, CONFIG_VIDEO_ENABLED);
    enable_video_button = gtk_check_button_new_with_mnemonic(_("_Enable video"));
    const gboolean video_enabled = g_strcmp0(ptr, "true") == 0;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_video_button), video_enabled);
    gtk_box_pack_start(GTK_BOX(vbox), enable_video_button, FALSE, FALSE, 0);

    gtk_widget_show_all(vbox);

    return vbox;
}
#endif

static GtkWidget* create_direct_ip_calls_tab(account_t *account)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    gchar *description = g_markup_printf_escaped(_("This profile is used when "
                         "you want to reach a remote peer simply by typing a sip URI "
                         "such as <b>sip:remotepeer</b>. The settings you define here "
                         "will also be used if no account can be matched to an incoming"
                         " or outgoing call."));
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), description);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    gtk_widget_set_size_request(label, 350, -1);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

    GtkWidget *auto_answer_checkbox = create_auto_answer_checkbox(account);
    gtk_box_pack_start(GTK_BOX(vbox), auto_answer_checkbox, FALSE, FALSE, 0);

    gtk_widget_show_all(vbox);
    return vbox;
}

static const gchar *bool_to_string(gboolean v)
{
    return v ? "true" : "false";
}

static void update_account_from_basic_tab(account_t *account)
{
    const gboolean IS_IP2IP = account_is_IP2IP(account);

    // Update protocol in case it changed
    gchar *proto;
    if (protocol_combo)
        proto = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo));
    else
        proto = g_strdup("SIP");

    if (g_strcmp0(proto, "SIP") == 0) {

        if (!IS_IP2IP) {
            account_replace(account, CONFIG_ACCOUNT_REGISTRATION_EXPIRE,
                    gtk_entry_get_text(GTK_ENTRY(expire_spin_box)));

            account_replace(account, CONFIG_ACCOUNT_ROUTESET,
                    gtk_entry_get_text(GTK_ENTRY(entry_route_set)));
        }

        gboolean v = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(use_stun_check_box));
        account_replace(account, CONFIG_STUN_ENABLE,
                bool_to_string(v));

        account_replace(account, CONFIG_STUN_SERVER,
                gtk_entry_get_text(GTK_ENTRY(stun_server_entry)));

        v = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button));
        account_replace(account, CONFIG_PUBLISHED_SAMEAS_LOCAL, bool_to_string(v));

        account_replace(account, CONFIG_ACCOUNT_AUDIO_PORT_MIN,
                gtk_entry_get_text(GTK_ENTRY(audio_port_min_spin_box)));
        account_replace(account, CONFIG_ACCOUNT_AUDIO_PORT_MAX,
                gtk_entry_get_text(GTK_ENTRY(audio_port_max_spin_box)));
#ifdef SFL_VIDEO
        account_replace(account, CONFIG_ACCOUNT_VIDEO_PORT_MIN,
                gtk_entry_get_text(GTK_ENTRY(video_port_min_spin_box)));
        account_replace(account, CONFIG_ACCOUNT_VIDEO_PORT_MAX,
                gtk_entry_get_text(GTK_ENTRY(video_port_max_spin_box)));
#endif

#ifdef SFL_PRESENCE
        if (!IS_IP2IP) {
            v = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(presence_check_box));
            account_replace(account, CONFIG_PRESENCE_ENABLED, bool_to_string(v));
            // TODO enable/disable the presence window view
        }
#endif

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(overrtp))) {
            g_debug("Set dtmf over rtp");
            account_replace(account, CONFIG_ACCOUNT_DTMF_TYPE, OVERRTP);
        } else {
            g_debug("Set dtmf over sip");
            account_replace(account, CONFIG_ACCOUNT_DTMF_TYPE, SIPINFO);
        }

        gchar* key_exchange = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(key_exchange_combo));

        if (utf8_case_equal(key_exchange, "ZRTP")) {
            account_replace(account, CONFIG_SRTP_ENABLE, "true");
            account_replace(account, CONFIG_SRTP_KEY_EXCHANGE, ZRTP);
        } else if (utf8_case_equal(key_exchange, "SDES")) {
            account_replace(account, CONFIG_SRTP_ENABLE, "true");
            account_replace(account, CONFIG_SRTP_KEY_EXCHANGE, SDES);
        } else {
            account_replace(account, CONFIG_SRTP_ENABLE, "false");
            account_replace(account, CONFIG_SRTP_KEY_EXCHANGE, "");
        }

        g_free(key_exchange);
        const gboolean tls_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(use_sip_tls_check_box));
        account_replace(account, CONFIG_TLS_ENABLE, bool_to_string(tls_enabled));

        gchar *address_combo_text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(local_address_combo));
        account_replace(account, CONFIG_LOCAL_INTERFACE, address_combo_text);
        g_free(address_combo_text);

        account_replace(account, CONFIG_LOCAL_PORT,
                        gtk_entry_get_text(GTK_ENTRY(local_port_spin_box)));
    }

    account_replace(account, CONFIG_ACCOUNT_USERAGENT,
            gtk_entry_get_text(GTK_ENTRY(entry_user_agent)));

    if (!IS_IP2IP) {
        account_replace(account, CONFIG_ACCOUNT_ALIAS, gtk_entry_get_text(GTK_ENTRY(entry_alias)));
        account_replace(account, CONFIG_ACCOUNT_TYPE, proto);
        account_replace(account, CONFIG_ACCOUNT_HOSTNAME, gtk_entry_get_text(GTK_ENTRY(entry_hostname)));
        account_replace(account, CONFIG_ACCOUNT_USERNAME, gtk_entry_get_text(GTK_ENTRY(entry_username)));
        account_replace(account, CONFIG_ACCOUNT_PASSWORD, gtk_entry_get_text(GTK_ENTRY(entry_password)));
        account_replace(account, CONFIG_ACCOUNT_MAILBOX, gtk_entry_get_text(GTK_ENTRY(entry_mailbox)));
    }

    g_free(proto);
}

void update_account_from_dialog(GtkWidget *dialog, const gchar *accountID)
{
    if (!dialog)
        return;

    account_t *account = account_list_get_by_id(accountID);
    if (!account)
        return;

    const gboolean IS_IP2IP = account_is_IP2IP(account);
    update_account_from_basic_tab(account);

    if (account_is_SIP(account)) {
        if (IS_IP2IP ||
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button))) {
            account_replace(account, CONFIG_PUBLISHED_PORT,
                    gtk_entry_get_text(GTK_ENTRY(local_port_spin_box)));
            account_replace(account, CONFIG_LOCAL_PORT,
                    gtk_entry_get_text(GTK_ENTRY(local_port_spin_box)));
            gchar *local_interface = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(local_address_combo));

            gchar *published_address = dbus_get_address_from_interface_name(local_interface);
            g_free(local_interface);

            account_replace(account, CONFIG_PUBLISHED_ADDRESS, published_address);
        } else {
            account_replace(account, CONFIG_PUBLISHED_PORT,
                    gtk_entry_get_text(GTK_ENTRY(published_port_spin_box)));

            account_replace(account, CONFIG_PUBLISHED_ADDRESS,
                    gtk_entry_get_text(GTK_ENTRY(published_address_entry)));
        }
    }

    const gboolean tone_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_tone));
    account_replace(account, CONFIG_RINGTONE_ENABLED, bool_to_string(tone_enabled));

#ifdef SFL_VIDEO
    const gboolean video_enabled =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_video_button));
    account_replace(account, CONFIG_VIDEO_ENABLED, bool_to_string(video_enabled));
#endif

    gchar *ringtone_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
    account_replace(account, CONFIG_RINGTONE_PATH, ringtone_path);
    g_free(ringtone_path);

    // Get current protocol for this account
    gchar *current_protocol;
    if (protocol_combo)
        current_protocol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo));
    else
        current_protocol = g_strdup("SIP");

    if (!IS_IP2IP && g_strcmp0(current_protocol, "SIP") == 0)
        account->credential_information = get_new_credential();

    /** @todo Verify if it's the best condition to check */
    if (g_strcmp0(account->accountID, "new") == 0) {
        dbus_add_account(account);
        if (account->credential_information)
            dbus_set_credentials(account);
    } else {
        if (account->credential_information)
            dbus_set_credentials(account);
        dbus_set_account_details(account);
    }

    // propagate changes to the daemon
    codec_list_update_to_daemon(account);

    g_free(current_protocol);
    gtk_widget_destroy(dialog);
}

GtkWidget *
show_account_window(const gchar *accountID, GtkDialog *parent, SFLPhoneClient *client, gboolean is_new)
{
    // First we reset
    reset();

    GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Account settings"),
                        GTK_WINDOW(parent),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL,
                        GTK_STOCK_APPLY,
                        GTK_RESPONSE_APPLY,
                        NULL);

    gtk_container_set_border_width(GTK_CONTAINER(dialog), 0);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), notebook, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 10);
    gtk_widget_show(notebook);

    account_t *account = account_list_get_by_id(accountID);
    if (!account) {
        g_warning("Invalid account %s", accountID);
        return NULL;
    }

    const gboolean IS_IP2IP = account_is_IP2IP(account);

    // We do not need the global settings for the IP2IP account
    if (!IS_IP2IP) {
        /* General Settings */
        GtkWidget *basic_tab = create_basic_tab(account, is_new, dialog);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), basic_tab, gtk_label_new(_("Basic")));
    }

    /* Audio Codecs */
    GtkWidget *audiocodecs_tab = create_audiocodecs_configuration(account);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), audiocodecs_tab, gtk_label_new(_("Audio")));

#ifdef SFL_VIDEO
    /* Video Codecs */
    GtkWidget *videocodecs_tab = create_videocodecs_configuration(account);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), videocodecs_tab, gtk_label_new(_("Video")));
#endif

    if (IS_IP2IP) {
        /* Custom tab for the IP to IP profile */
        GtkWidget *ip_tab = create_direct_ip_calls_tab(account);
        gtk_notebook_prepend_page(GTK_NOTEBOOK(notebook), ip_tab, gtk_label_new(_("General")));
    }

    /* Advanced */
    advanced_tab = create_advanced_tab(account);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), advanced_tab, gtk_label_new(_("Advanced")));

    /* Security */
    security_tab = create_security_tab(account, client);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), security_tab, gtk_label_new(_("Security")));

    // Emit signal to hide advanced and security tabs in case of IAX
    if (protocol_combo)
        g_signal_emit_by_name(protocol_combo, "changed", NULL);

    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);

    /* Run dialog, this blocks */
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_APPLY) {
        return dialog;
    } else {
        gtk_widget_destroy(dialog);
        return NULL;
    }
}

