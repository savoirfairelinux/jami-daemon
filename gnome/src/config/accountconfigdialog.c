/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <gtk/gtk.h>

#include "config.h"
#include "str_utils.h"
#include "logger.h"
#include "actions.h"
#include "mainwindow.h"
#include "accountlist.h"
#include "audioconf.h"
#include "accountconfigdialog.h"
#include "zrtpadvanceddialog.h"
#include "tlsadvanceddialog.h"
#include "dbus/dbus.h"
#include "utils.h"
#include "unused.h"

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
static GtkWidget *entry_resolve_name_only_once;
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
static GtkWidget *publishedAddrRadioButton;
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

/*
 * Display / Hide the password
 */
static void show_password_cb(GtkWidget *widget UNUSED, gpointer data)
{
    gtk_entry_set_visibility(GTK_ENTRY(data), !gtk_entry_get_visibility(GTK_ENTRY(data)));
}

/* Signal to protocol_combo 'changed' */
void change_protocol_cb(account_t *account UNUSED)
{
    gchar *protocol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo));

    // Only if tabs are not NULL
    if (security_tab && advanced_tab) {
        if (utf8_case_cmp(protocol, "IAX") == 0) {
            gtk_widget_hide(security_tab);
            gtk_widget_hide(advanced_tab);
        } else {
            gtk_widget_show(security_tab);
            gtk_widget_show(advanced_tab);
        }
    }

    g_free(protocol);
}

void
select_dtmf_type(void)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(overrtp)))
        DEBUG("Selected DTMF over RTP");
    else
        DEBUG("Selected DTMF over SIP");
}

static GPtrArray* get_new_credential(void)
{
    gint row_count = 0;
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

        DEBUG("Row %d: %s %s %s", row_count++, username, password, realm);

        GHashTable * new_table = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                       g_free, g_free);
        g_hash_table_insert(new_table, g_strdup(ACCOUNT_REALM), realm);
        g_hash_table_insert(new_table, g_strdup(ACCOUNT_USERNAME), username);
        g_hash_table_insert(new_table, g_strdup(ACCOUNT_PASSWORD), password);

        g_ptr_array_add(credential_array, new_table);
    }

    return credential_array;
}

static void update_credential_cb(GtkWidget *widget, gpointer data UNUSED)
{
    GtkTreeIter iter;

    if (credential_store && gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(credential_store), &iter, "0")) {
        gint column = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "column"));
        gtk_list_store_set(GTK_LIST_STORE(credential_store), &iter, column, gtk_entry_get_text(GTK_ENTRY(widget)), -1);
    }
}

static GtkWidget* create_basic_tab(account_t *account)
{
    g_assert(account);

    const gchar *account_type = g_hash_table_lookup(account->properties,
                                                    ACCOUNT_TYPE);
    gchar *password = NULL;
    if (g_strcmp0(account_type, "SIP") == 0) {
        /* get password from credentials list */
        if (account->credential_information) {
            GHashTable * element = g_ptr_array_index(account->credential_information, 0);
            password = g_hash_table_lookup(element, ACCOUNT_PASSWORD);
        }
    } else
        password = g_hash_table_lookup(account->properties, ACCOUNT_PASSWORD);

    GtkWidget *frame = gnome_main_section_new(_("Account Parameters"));
    gtk_widget_show(frame);

    GtkWidget *table = NULL;

    if (g_strcmp0(account_type, "SIP") == 0)
        table = gtk_table_new(9, 2,  FALSE/* homogeneous */);
    else if (g_strcmp0(account_type, "IAX") == 0)
        table = gtk_table_new(8, 2, FALSE);
    else {
        ERROR("Unknown account type \"%s\"", account_type);
        return NULL;
    }

    gtk_table_set_row_spacings(GTK_TABLE(table), 10);
    gtk_table_set_col_spacings(GTK_TABLE(table), 10);
    gtk_widget_show(table);
    gtk_container_add(GTK_CONTAINER(frame) , table);

    GtkWidget *label = gtk_label_new_with_mnemonic(_("_Alias"));
    gint row = 0;
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_alias = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_alias);
    gchar *alias = g_hash_table_lookup(account->properties, ACCOUNT_ALIAS);
    gtk_entry_set_text(GTK_ENTRY(entry_alias), alias);
    gtk_table_attach(GTK_TABLE(table), entry_alias, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    row++;
    label = gtk_label_new_with_mnemonic(_("_Protocol"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    protocol_combo = gtk_combo_box_text_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), protocol_combo);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol_combo), "SIP");

    if (dbus_is_iax2_enabled())
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol_combo), "IAX");

    if (g_strcmp0(account_type, "SIP") == 0)
        gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 0);
    else if (g_strcmp0(account_type, "IAX") == 0)
        gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 1);
    else {
        DEBUG("Config: Error: Account protocol not valid");
        /* Should never come here, add debug message. */
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(protocol_combo), _("Unknown"));
        gtk_combo_box_set_active(GTK_COMBO_BOX(protocol_combo), 2);
    }

    gtk_table_attach(GTK_TABLE(table), protocol_combo, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    /* Link signal 'changed' */
    g_signal_connect(G_OBJECT(GTK_COMBO_BOX(protocol_combo)), "changed",
                     G_CALLBACK(change_protocol_cb), account);

    row++;
    label = gtk_label_new_with_mnemonic(_("_Host name"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_hostname = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_hostname);
    const gchar *hostname = g_hash_table_lookup(account->properties,
                                                    ACCOUNT_HOSTNAME);
    gtk_entry_set_text(GTK_ENTRY(entry_hostname), hostname);
    gtk_table_attach(GTK_TABLE(table), entry_hostname, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    row++;
    label = gtk_label_new_with_mnemonic(_("_User name"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_username = gtk_entry_new();
    const gchar *PERSON_IMG = ICONS_DIR "/stock_person.svg";
    gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(entry_username),
                                   GTK_ENTRY_ICON_PRIMARY,
                                   gdk_pixbuf_new_from_file(PERSON_IMG, NULL));
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_username);
    gchar *username = g_hash_table_lookup(account->properties, ACCOUNT_USERNAME);
    gtk_entry_set_text(GTK_ENTRY(entry_username), username);
    gtk_table_attach(GTK_TABLE(table), entry_username, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    if (g_strcmp0(account_type, "SIP") == 0) {
        g_signal_connect(G_OBJECT(entry_username), "changed",
                         G_CALLBACK(update_credential_cb), NULL);
        g_object_set_data(G_OBJECT(entry_username), "column",
                          GINT_TO_POINTER(COLUMN_CREDENTIAL_USERNAME));
    }

    row++;
    label = gtk_label_new_with_mnemonic(_("_Password"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_password = gtk_entry_new();
    gtk_entry_set_icon_from_stock(GTK_ENTRY(entry_password),
                                  GTK_ENTRY_ICON_PRIMARY,
                                  GTK_STOCK_DIALOG_AUTHENTICATION);
    gtk_entry_set_visibility(GTK_ENTRY(entry_password), FALSE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_password);
    password = password ? password : "";
    gtk_entry_set_text(GTK_ENTRY(entry_password), password);
    gtk_table_attach(GTK_TABLE(table), entry_password, 1, 2, row, row + 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    if (g_strcmp0(account_type, "SIP") == 0) {
        g_signal_connect(G_OBJECT(entry_password), "changed", G_CALLBACK(update_credential_cb), NULL);
        g_object_set_data(G_OBJECT(entry_password), "column", GINT_TO_POINTER(COLUMN_CREDENTIAL_PASSWORD));
    }

    row++;
    GtkWidget *clearTextcheck_box = gtk_check_button_new_with_mnemonic(_("Show password"));
    g_signal_connect(clearTextcheck_box, "toggled", G_CALLBACK(show_password_cb), entry_password);
    gtk_table_attach(GTK_TABLE(table), clearTextcheck_box, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    row++;
    label = gtk_label_new_with_mnemonic(_("_Proxy"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_route_set = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_route_set);
    gchar *route_set = g_hash_table_lookup(account->properties, ACCOUNT_ROUTE);
    gtk_entry_set_text(GTK_ENTRY(entry_route_set), route_set);
    gtk_table_attach(GTK_TABLE(table), entry_route_set, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    row++;
    label = gtk_label_new_with_mnemonic(_("_Voicemail number"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_mailbox = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_mailbox);
    gchar *mailbox = g_hash_table_lookup(account->properties, ACCOUNT_MAILBOX);
    mailbox = mailbox ? mailbox : "";
    gtk_entry_set_text(GTK_ENTRY(entry_mailbox), mailbox);
    gtk_table_attach(GTK_TABLE(table), entry_mailbox, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    row++;
    label = gtk_label_new_with_mnemonic(_("_User-agent"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entry_user_agent = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_user_agent);
    gchar *user_agent = g_hash_table_lookup(account->properties, ACCOUNT_USERAGENT);
    gtk_entry_set_text(GTK_ENTRY(entry_user_agent), user_agent);
    gtk_table_attach(GTK_TABLE(table), entry_user_agent, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    gtk_widget_show_all(table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);

    return frame;
}

static void fill_treeview_with_credential(account_t * account)
{
    GtkTreeIter iter;
    gtk_list_store_clear(credential_store);

    if (!account->credential_information) {
        account->credential_information = g_ptr_array_sized_new(1);
        GHashTable * new_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        g_hash_table_insert(new_table, g_strdup(ACCOUNT_REALM), g_strdup("*"));
        g_hash_table_insert(new_table, g_strdup(ACCOUNT_USERNAME), g_strdup(""));
        g_hash_table_insert(new_table, g_strdup(ACCOUNT_PASSWORD), g_strdup(""));
        g_ptr_array_add(account->credential_information, new_table);
    }

    for (unsigned i = 0; i < account->credential_information->len; i++) {
        GHashTable * element = g_ptr_array_index(account->credential_information, i);
        gtk_list_store_append(credential_store, &iter);
        gtk_list_store_set(credential_store, &iter, COLUMN_CREDENTIAL_REALM, g_hash_table_lookup(element, ACCOUNT_REALM),
                           COLUMN_CREDENTIAL_USERNAME, g_hash_table_lookup(element, ACCOUNT_USERNAME),
                           COLUMN_CREDENTIAL_PASSWORD, g_hash_table_lookup(element, ACCOUNT_PASSWORD),
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

static void add_credential_cb(GtkWidget *button UNUSED, gpointer data)
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
delete_credential_cb(GtkWidget *button UNUSED, gpointer data)
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
    DEBUG("path desc in cell_edited_cb: %s\n", text);

    if ((utf8_case_cmp(path_desc, "0") == 0) &&
        utf8_case_cmp(text, gtk_entry_get_text(GTK_ENTRY(entry_username))) != 0)
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry_username),
                                             G_CALLBACK(update_credential_cb),
                                             NULL);

    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, column, text, -1);
    gtk_tree_path_free(path);
}

static void
editing_started_cb(GtkCellRenderer *cell UNUSED, GtkCellEditable * editable,
                   const gchar * path, gpointer data UNUSED)
{
    DEBUG("Editing started");
    DEBUG("path desc in editing_started_cb: %s\n", path);

    // If we are dealing the first row
    if (utf8_case_cmp(path, "0") == 0)
        gtk_entry_set_text(GTK_ENTRY(editable), gtk_entry_get_text(GTK_ENTRY(entry_password)));
}

static void show_advanced_zrtp_options_cb(GtkWidget *widget UNUSED, gpointer data)
{
    gchar *proto = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(key_exchange_combo));

    if (utf8_case_cmp(proto, "ZRTP") == 0)
        show_advanced_zrtp_options((GHashTable *) data);
    else
        show_advanced_sdes_options((GHashTable *) data);

    g_free(proto);
}


static void
show_advanced_tls_options_cb(GtkWidget *widget UNUSED, gpointer data)
{
    show_advanced_tls_options((GHashTable *) data);
}

static void
key_exchange_changed_cb(GtkWidget *widget UNUSED, gpointer data UNUSED)
{
    gchar *active_text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(key_exchange_combo));
    DEBUG("Key exchange changed %s", active_text);

    gboolean sensitive = FALSE;
    sensitive |= utf8_case_cmp(active_text, "SDES") == 0;
    sensitive |= utf8_case_cmp(active_text, "ZRTP") == 0;
    g_free(active_text);
    gtk_widget_set_sensitive(zrtp_button, sensitive);
}


static void use_sip_tls_cb(GtkWidget *widget, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        DEBUG("Using sips");
        gtk_widget_set_sensitive(data, TRUE);
        // Uncheck stun
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_stun_check_box), FALSE);
        gtk_widget_set_sensitive(use_stun_check_box, FALSE);
        gtk_widget_set_sensitive(same_as_local_radio_button, TRUE);
        gtk_widget_set_sensitive(publishedAddrRadioButton, TRUE);
        gtk_widget_hide(stun_server_label);
        gtk_widget_hide(stun_server_entry);

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
            gtk_widget_set_sensitive(publishedAddrRadioButton, FALSE);
            gtk_widget_show(stun_server_label);
            gtk_widget_show(stun_server_entry);
            gtk_widget_hide(published_address_entry);
            gtk_widget_hide(published_port_spin_box);
            gtk_widget_hide(published_address_label);
            gtk_widget_hide(published_port_label);
        } else {
            gtk_widget_set_sensitive(same_as_local_radio_button, TRUE);
            gtk_widget_set_sensitive(publishedAddrRadioButton, TRUE);
            gtk_widget_hide(stun_server_label);
            gtk_widget_hide(stun_server_entry);
        }
    }
}

static gchar *
get_interface_addr_from_name(const gchar * const iface_name)
{
#define	UC(b)	(((int)b)&0xff)

    int fd;

    if ((fd = socket(AF_INET, SOCK_DGRAM,0)) < 0)
        DEBUG("getInterfaceAddrFromName error could not open socket\n");

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));

    strcpy(ifr.ifr_name, iface_name);
    ifr.ifr_addr.sa_family = AF_INET;

    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0)
        DEBUG("getInterfaceAddrFromName use default interface (0.0.0.0)\n");


    struct sockaddr_in *saddr_in = (struct sockaddr_in *) &ifr.ifr_addr;
    struct in_addr *addr_in = &(saddr_in->sin_addr);

    char *tmp_addr = (char *) addr_in;

    gchar *iface_addr = g_strdup_printf("%d.%d.%d.%d", UC(tmp_addr[0]),
                                        UC(tmp_addr[1]), UC(tmp_addr[2]), UC(tmp_addr[3]));

    close(fd);
    return iface_addr;
#undef UC
}

static void local_interface_changed_cb(GtkWidget * widget UNUSED, gpointer data UNUSED)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button))) {
        gchar *local_iface_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(local_address_combo));
        gchar *local_iface_addr = get_interface_addr_from_name(local_iface_name);

        gtk_entry_set_text(GTK_ENTRY(local_address_entry), local_iface_addr);
        gtk_entry_set_text(GTK_ENTRY(published_address_entry), local_iface_addr);
        g_free(local_iface_addr);
        g_free(local_iface_name);
    }
}

static void set_published_addr_manually_cb(GtkWidget * widget, gpointer data UNUSED)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        DEBUG("Config: Showing manual publishing options");
        gtk_widget_show(published_port_label);
        gtk_widget_show(published_port_spin_box);
        gtk_widget_show(published_address_label);
        gtk_widget_show(published_address_entry);
    } else {
        DEBUG("Config: Hiding manual publishing options");
        gtk_widget_hide(published_port_label);
        gtk_widget_hide(published_port_spin_box);
        gtk_widget_hide(published_address_label);
        gtk_widget_hide(published_address_entry);
    }
}

static void use_stun_cb(GtkWidget *widget, gpointer data UNUSED)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        DEBUG("Config: Showing stun options, hiding Local/Published info");
        gtk_widget_show(stun_server_label);
        gtk_widget_show(stun_server_entry);
        gtk_widget_set_sensitive(same_as_local_radio_button, FALSE);
        gtk_widget_set_sensitive(publishedAddrRadioButton, FALSE);

        gtk_widget_hide(published_address_label);
        gtk_widget_hide(published_port_label);
        gtk_widget_hide(published_address_entry);
        gtk_widget_hide(published_port_spin_box);
    } else {
        DEBUG("Config: hiding stun options, showing Local/Published info");
        gtk_widget_hide(stun_server_label);
        gtk_widget_hide(stun_server_entry);
        gtk_widget_set_sensitive(same_as_local_radio_button, TRUE);
        gtk_widget_set_sensitive(publishedAddrRadioButton, TRUE);

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(publishedAddrRadioButton))) {
            gtk_widget_show(published_address_label);
            gtk_widget_show(published_port_label);
            gtk_widget_show(published_address_entry);
            gtk_widget_show(published_port_spin_box);
        }
    }

    DEBUG("DONE");
}


static void same_as_local_cb(GtkWidget * widget, gpointer data UNUSED)
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



GtkWidget* create_credential_widget(account_t *a)
{

    GtkWidget *frame, *table, *scrolled_window_credential, *addButton;
    GtkCellRenderer * renderer;
    GtkTreeViewColumn * tree_view_column;
    GtkTreeSelection * treeSelection;

    /* Credentials tree view */
    gnome_main_section_new_with_table(_("Credential"), &frame, &table, 1, 1);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_table_set_row_spacings(GTK_TABLE(table), 10);

    scrolled_window_credential = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window_credential), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window_credential), GTK_SHADOW_IN);
    gtk_table_attach_defaults(GTK_TABLE(table), scrolled_window_credential, 0, 1, 0, 1);

    credential_store = gtk_list_store_new(COLUMN_CREDENTIAL_COUNT,
                                         G_TYPE_STRING,  // Realm
                                         G_TYPE_STRING,  // Username
                                         G_TYPE_STRING,  // Password
                                         G_TYPE_POINTER  // Pointer to the Objectc
                                        );

    treeview_credential = gtk_tree_view_new_with_model(GTK_TREE_MODEL(credential_store));
    treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_credential));
    g_signal_connect(G_OBJECT(treeSelection), "changed", G_CALLBACK(select_credential_cb), credential_store);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, NULL);
    g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(cell_edited_cb), credential_store);
    g_object_set_data(G_OBJECT(renderer), "column", GINT_TO_POINTER(COLUMN_CREDENTIAL_REALM));
    tree_view_column = gtk_tree_view_column_new_with_attributes("Realm",
                     renderer,
                     "markup", COLUMN_CREDENTIAL_REALM,
                     NULL);
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

    fill_treeview_with_credential(a);

    /* Credential Buttons */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 3, 1, 2);

    addButton = gtk_button_new_from_stock(GTK_STOCK_ADD);
    g_signal_connect(addButton, "clicked", G_CALLBACK(add_credential_cb), credential_store);
    gtk_box_pack_start(GTK_BOX(hbox), addButton, FALSE, FALSE, 0);

    delete_cred_button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    g_signal_connect(delete_cred_button, "clicked", G_CALLBACK(delete_credential_cb), treeview_credential);
    gtk_box_pack_start(GTK_BOX(hbox), delete_cred_button, FALSE, FALSE, 0);

    /* Dynamically resize the window to fit the scrolled window */
    gtk_widget_set_size_request(scrolled_window_credential, 400, 120);

    return frame;
}


GtkWidget* create_security_widget(account_t *a)
{
    GtkWidget *frame, *table, *sip_tls_advanced_button, *label;
    gchar *curSRTPEnabled = NULL, *curKeyExchange = NULL, *curTLSEnabled = NULL;

    // Load from SIP/IAX/Unknown ?
    if (a) {
        curKeyExchange = g_hash_table_lookup(a->properties, ACCOUNT_KEY_EXCHANGE);

        if (curKeyExchange == NULL)
            curKeyExchange = "none";

        curSRTPEnabled = g_hash_table_lookup(a->properties, ACCOUNT_SRTP_ENABLED);

        if (curSRTPEnabled == NULL)
            curSRTPEnabled = "false";

        curTLSEnabled = g_hash_table_lookup(a->properties, TLS_ENABLE);

        if (curTLSEnabled == NULL)
            curTLSEnabled = "false";
    }

    gnome_main_section_new_with_table(_("Security"), &frame, &table, 2, 3);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_table_set_row_spacings(GTK_TABLE(table), 10);
    gtk_table_set_col_spacings(GTK_TABLE(table), 10);

    /* TLS subsection */
    sip_tls_advanced_button = gtk_button_new_from_stock(GTK_STOCK_EDIT);
    gtk_table_attach_defaults(GTK_TABLE(table), sip_tls_advanced_button, 2, 3, 0, 1);
    gtk_widget_set_sensitive(sip_tls_advanced_button, FALSE);
    g_signal_connect(G_OBJECT(sip_tls_advanced_button), "clicked", G_CALLBACK(show_advanced_tls_options_cb),a->properties);

    use_sip_tls_check_box = gtk_check_button_new_with_mnemonic(_("Use TLS transport(sips)"));
    g_signal_connect(use_sip_tls_check_box, "toggled", G_CALLBACK(use_sip_tls_cb), sip_tls_advanced_button);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_sip_tls_check_box),(g_strcmp0(curTLSEnabled, "true") == 0) ? TRUE:FALSE);
    gtk_table_attach_defaults(GTK_TABLE(table), use_sip_tls_check_box, 0, 2, 0, 1);

    /* ZRTP subsection */
    label = gtk_label_new_with_mnemonic(_("SRTP key exchange"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    key_exchange_combo = gtk_combo_box_text_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), key_exchange_combo);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(key_exchange_combo), "ZRTP");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(key_exchange_combo), "SDES");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(key_exchange_combo), _("Disabled"));

    zrtp_button = gtk_button_new_from_stock(GTK_STOCK_PREFERENCES);
    g_signal_connect(G_OBJECT(zrtp_button), "clicked", G_CALLBACK(show_advanced_zrtp_options_cb),a->properties);

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

    g_signal_connect(G_OBJECT(GTK_COMBO_BOX(key_exchange_combo)), "changed", G_CALLBACK(key_exchange_changed_cb), a);

    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), key_exchange_combo, 1, 2, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), zrtp_button, 2, 3, 1, 2);

    gtk_widget_show_all(table);

    return frame;
}


GtkWidget * create_security_tab(account_t *a)
{
    GtkWidget * frame;
    GtkWidget * ret;

    ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    // Credentials frame
    frame = create_credential_widget(a);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    // Security frame
    frame = create_security_widget(a);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    gtk_widget_show_all(ret);

    return ret;
}

static GtkWidget* create_registration_expire(account_t *a)
{

    GtkWidget *table, *frame, *label;

    gchar *resolve_once=NULL, *account_expire=NULL;
    gchar *orig_key = NULL;
    if (a) {
        gboolean gotkey = FALSE;
        gotkey = g_hash_table_lookup_extended(a->properties, ACCOUNT_RESOLVE_ONCE, (gpointer)&orig_key, (gpointer)&resolve_once);
        if(gotkey == FALSE) {
            ERROR("could not retreive resolve_once from account properties");
        } 
        gotkey = g_hash_table_lookup_extended(a->properties, ACCOUNT_REGISTRATION_EXPIRE, (gpointer)&orig_key, (gpointer)&account_expire);
        if(gotkey == FALSE) {
            ERROR("could not retreive %s from account properties", ACCOUNT_REGISTRATION_EXPIRE);
        }
    }
    

    gnome_main_section_new_with_table(_("Registration"), &frame, &table, 2, 3);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_table_set_row_spacings(GTK_TABLE(table), 5);

    label = gtk_label_new_with_mnemonic(_("Registration expire"));
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    expire_spin_box = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), expire_spin_box);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(expire_spin_box), g_ascii_strtod(account_expire, NULL));
    gtk_table_attach_defaults(GTK_TABLE(table), expire_spin_box, 1, 2, 0, 1);

    entry_resolve_name_only_once = gtk_check_button_new_with_mnemonic(_("_Comply with RFC 3263"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entry_resolve_name_only_once),
                                 utf8_case_cmp(resolve_once,"false") == 0 ? TRUE: FALSE);
    gtk_table_attach_defaults(GTK_TABLE(table), entry_resolve_name_only_once, 0, 2, 1, 2);
    gtk_widget_set_sensitive(entry_resolve_name_only_once , TRUE);

    return frame;
}

GtkWidget* create_network(account_t *a)
{
    GtkWidget *table, *frame, *label;
    gchar *local_interface = NULL;
    gchar *local_port = NULL;

    if (a) {
        local_interface = g_hash_table_lookup(a->properties, LOCAL_INTERFACE);
        local_port = g_hash_table_lookup(a->properties, LOCAL_PORT);
    }

    gnome_main_section_new_with_table(_("Network Interface"), &frame, &table, 2, 3);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_table_set_row_spacings(GTK_TABLE(table), 5);

    /**
     * Retreive the list of IP interface from the
     * the daemon and build the combo box.
     */
    local_address_combo = gtk_combo_box_text_new();


    label = gtk_label_new_with_mnemonic(_("Local address"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    gchar **iface_list = dbus_get_all_ip_interface_by_name();

    int idx = 0;
    for (gchar **iface = iface_list; iface && *iface; iface++, idx++) {

        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(local_address_combo), NULL, *iface);
        if (g_strcmp0(*iface, local_interface) == 0)
            gtk_combo_box_set_active(GTK_COMBO_BOX(local_address_combo), idx);
    }
    if (!local_interface)
        gtk_combo_box_set_active(GTK_COMBO_BOX(local_address_combo), 0);


    gtk_label_set_mnemonic_widget(GTK_LABEL(label), local_address_combo);
    gtk_table_attach(GTK_TABLE(table), local_address_combo, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // Fill the text entry with the ip address of local interface selected
    local_address_entry = gtk_entry_new();
    gchar *local_iface_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(local_address_combo));
    gchar *local_iface_addr = get_interface_addr_from_name(local_iface_name);
    g_free(local_iface_name);
    gtk_entry_set_text(GTK_ENTRY(local_address_entry), local_iface_addr);
    g_free(local_iface_addr);
    gtk_widget_set_sensitive(local_address_entry, FALSE);
    gtk_table_attach(GTK_TABLE(table), local_address_entry, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // Local port widget
    label = gtk_label_new_with_mnemonic(_("Local port"));
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    local_port_spin_box = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), local_port_spin_box);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(local_port_spin_box), g_ascii_strtod(local_port, NULL));

    gtk_table_attach_defaults(GTK_TABLE(table), local_port_spin_box, 1, 2, 1, 2);

    return frame;
}

GtkWidget* create_published_address(account_t *a)
{

    GtkWidget *table, *frame;
    gchar *use_tls =NULL;
    gchar *published_address = NULL;
    gchar *published_port = NULL;
    gchar *stun_enable = NULL;
    gchar *stun_server = NULL;
    gchar *published_sameas_local = NULL;

    // Get the user configuration
    if (a) {

        use_tls = g_hash_table_lookup(a->properties, TLS_ENABLE);
        published_sameas_local = g_hash_table_lookup(a->properties, PUBLISHED_SAMEAS_LOCAL);

        if (utf8_case_cmp(published_sameas_local, "true") == 0) {
            published_address = dbus_get_address_from_interface_name(g_hash_table_lookup(a->properties, LOCAL_INTERFACE));
            published_port = g_hash_table_lookup(a->properties, LOCAL_PORT);
        } else {
            published_address = g_hash_table_lookup(a->properties, PUBLISHED_ADDRESS);
            published_port = g_hash_table_lookup(a->properties, PUBLISHED_PORT);
        }

        stun_enable = g_hash_table_lookup(a->properties, ACCOUNT_SIP_STUN_ENABLED);
        stun_server = g_hash_table_lookup(a->properties, ACCOUNT_SIP_STUN_SERVER);
        published_sameas_local = g_hash_table_lookup(a->properties, PUBLISHED_SAMEAS_LOCAL);
    }

    gnome_main_section_new_with_table(_("Published address"), &frame, &table, 2, 3);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);
    gtk_table_set_row_spacings(GTK_TABLE(table), 5);

    use_stun_check_box = gtk_check_button_new_with_mnemonic(_("Using STUN"));
    gtk_table_attach_defaults(GTK_TABLE(table), use_stun_check_box, 0, 1, 0, 1);
    g_signal_connect(use_stun_check_box, "toggled", G_CALLBACK(use_stun_cb), a);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_stun_check_box),
                                 utf8_case_cmp(stun_enable, "true") == 0);
    gtk_widget_set_sensitive(use_stun_check_box, utf8_case_cmp(use_tls, "true") != 0);

    stun_server_label = gtk_label_new_with_mnemonic(_("STUN server URL"));
    gtk_table_attach_defaults(GTK_TABLE(table), stun_server_label, 0, 1, 1, 2);
    gtk_misc_set_alignment(GTK_MISC(stun_server_label), 0, 0.5);
    stun_server_entry = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(stun_server_label), stun_server_entry);
    gtk_entry_set_text(GTK_ENTRY(stun_server_entry), stun_server);
    gtk_table_attach_defaults(GTK_TABLE(table), stun_server_entry, 1, 2, 1, 2);

    same_as_local_radio_button = gtk_radio_button_new_with_mnemonic_from_widget(NULL, _("Same as local parameters"));
    gtk_table_attach_defaults(GTK_TABLE(table), same_as_local_radio_button, 0, 2, 3, 4);

    publishedAddrRadioButton = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(same_as_local_radio_button), _("Set published address and port:"));
    gtk_table_attach_defaults(GTK_TABLE(table), publishedAddrRadioButton, 0, 2, 4, 5);

    if (utf8_case_cmp(published_sameas_local, "true") == 0) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(publishedAddrRadioButton), FALSE);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button), FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(publishedAddrRadioButton), TRUE);
    }

    published_address_label = gtk_label_new_with_mnemonic(_("Published address"));
    gtk_table_attach_defaults(GTK_TABLE(table), published_address_label, 0, 1, 5, 6);
    gtk_misc_set_alignment(GTK_MISC(published_address_label), 0, 0.5);
    published_address_entry = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(published_address_label), published_address_entry);

    gtk_entry_set_text(GTK_ENTRY(published_address_entry), published_address);
    gtk_table_attach_defaults(GTK_TABLE(table), published_address_entry, 1, 2, 5, 6);

    published_port_label = gtk_label_new_with_mnemonic(_("Published port"));
    gtk_table_attach_defaults(GTK_TABLE(table), published_port_label, 0, 1, 6, 7);
    gtk_misc_set_alignment(GTK_MISC(published_port_label), 0, 0.5);
    published_port_spin_box = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(published_port_label), published_port_spin_box);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(published_port_spin_box), g_ascii_strtod(published_port, NULL));

    gtk_table_attach_defaults(GTK_TABLE(table), published_port_spin_box, 1, 2, 6, 7);

    // This will trigger a signal, and the above two
    // widgets need to be instanciated before that.
    g_signal_connect(local_address_combo, "changed", G_CALLBACK(local_interface_changed_cb), local_address_combo);


    g_signal_connect(same_as_local_radio_button, "toggled", G_CALLBACK(same_as_local_cb), same_as_local_radio_button);
    g_signal_connect(publishedAddrRadioButton, "toggled", G_CALLBACK(set_published_addr_manually_cb), publishedAddrRadioButton);

    set_published_addr_manually_cb(publishedAddrRadioButton, NULL);

    return frame;
}

GtkWidget* create_advanced_tab(account_t *a)
{

    // Build the advanced tab, to appear on the account configuration panel
    DEBUG("Config: Build advanced tab");

    GtkWidget *vbox, *frame;

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    frame = create_registration_expire(a);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    frame = create_network(a);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    frame = create_published_address(a);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    gtk_widget_show_all(vbox);

    use_stun_cb(use_stun_check_box, NULL);

    set_published_addr_manually_cb(publishedAddrRadioButton, NULL);

    return vbox;
}

static void ringtone_enabled_cb(GtkWidget *widget UNUSED, gpointer data, const gchar *accountID UNUSED)
{
    /* toggle sensitivity */
    gtk_widget_set_sensitive(data, !gtk_widget_is_sensitive(data));
}


static GtkWidget* create_audiocodecs_configuration(account_t *account)
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
    gpointer p = g_hash_table_lookup(account->properties, ACCOUNT_TYPE);

    GtkWidget *table;

    if (g_strcmp0(p, "SIP") == 0) {
        // Box for dtmf
        GtkWidget *dtmf;
        gnome_main_section_new_with_table(_("DTMF"), &dtmf, &table, 1, 2);
        gtk_box_pack_start(GTK_BOX(vbox), dtmf, FALSE, FALSE, 0);
        gtk_widget_show(dtmf);

        overrtp = gtk_radio_button_new_with_label(NULL, _("RTP"));
        const gchar * const dtmf_type = g_hash_table_lookup(account->properties, ACCOUNT_DTMF_TYPE);
        const gboolean dtmf_are_rtp = !utf8_case_cmp(dtmf_type, OVERRTP);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(overrtp), dtmf_are_rtp);
        gtk_table_attach(GTK_TABLE(table), overrtp, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

        GtkWidget *sipinfo = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(overrtp),  _("SIP"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sipinfo), !dtmf_are_rtp);
        g_signal_connect(G_OBJECT(sipinfo), "clicked", G_CALLBACK(select_dtmf_type), NULL);
        gtk_table_attach(GTK_TABLE(table), sipinfo, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    }

    // Box for the ringtones
    GtkWidget *frame;
    gnome_main_section_new_with_table(_("Ringtones"), &frame, &table, 1, 2);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    file_chooser = gtk_file_chooser_button_new(_("Choose a ringtone"), GTK_FILE_CHOOSER_ACTION_OPEN);

    p = g_hash_table_lookup(account->properties, CONFIG_RINGTONE_ENABLED);
    enable_tone = gtk_check_button_new_with_mnemonic(_("_Enable ringtones"));
    const gboolean ringtone_enabled = g_strcmp0(p, "true") == 0;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_tone), ringtone_enabled);
    g_signal_connect(G_OBJECT(enable_tone) , "clicked", G_CALLBACK(ringtone_enabled_cb), file_chooser);
    gtk_table_attach(GTK_TABLE(table), enable_tone, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // file chooser button
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser) , g_get_home_dir());
    p = g_hash_table_lookup(account->properties, CONFIG_RINGTONE_PATH);
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_chooser) , p);
    gtk_widget_set_sensitive(file_chooser, ringtone_enabled);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Audio Files"));
    gtk_file_filter_add_pattern(filter, "*.wav");
    gtk_file_filter_add_pattern(filter, "*.ul");
    gtk_file_filter_add_pattern(filter, "*.au");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), filter);
    gtk_table_attach(GTK_TABLE(table), file_chooser, 0, 1, 1, 2,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    gtk_widget_show_all(vbox);

    return vbox;
}

GtkWidget* create_direct_ip_calls_tab(account_t *a)
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

    GtkWidget *frame = create_network(a);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    frame = create_security_widget(a);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    gtk_widget_show_all(vbox);
    return vbox;
}

void show_account_window(account_t *account)
{
    // First we reset
    reset();

    GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Account settings"),
                        GTK_WINDOW(get_main_window()),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL,
                        GTK_STOCK_APPLY,
                        GTK_RESPONSE_ACCEPT,
                        NULL);

    gtk_container_set_border_width(GTK_CONTAINER(dialog), 0);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), notebook, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 10);
    gtk_widget_show(notebook);

    // We do not need the global settings for the IP2IP account
    if (!is_IP2IP(account)) {
        /* General Settings */
        GtkWidget *basic_tab = create_basic_tab(account);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), basic_tab, gtk_label_new(_("Basic")));
        gtk_notebook_page_num(GTK_NOTEBOOK(notebook), basic_tab);
    }

    /* Audio Codecs */
    GtkWidget *audiocodecs_tab = create_audiocodecs_configuration(account);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), audiocodecs_tab, gtk_label_new(_("Audio")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), audiocodecs_tab);

    // Get current protocol for this account protocol
    gchar *current_protocol;

    if (protocol_combo)
        current_protocol = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo));
    else
        current_protocol = g_strdup("SIP");

    // Do not need advanced or security one for the IP2IP account
    if (!is_IP2IP(account)) {

        /* Advanced */
        advanced_tab = create_advanced_tab(account);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), advanced_tab, gtk_label_new(_("Advanced")));
        gtk_notebook_page_num(GTK_NOTEBOOK(notebook), advanced_tab);

        /* Security */
        security_tab = create_security_tab(account);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), security_tab, gtk_label_new(_("Security")));
        gtk_notebook_page_num(GTK_NOTEBOOK(notebook), security_tab);
    } else {
        /* Custom tab for the IP to IP profile */
        GtkWidget *ip_tab = create_direct_ip_calls_tab(account);
        gtk_notebook_prepend_page(GTK_NOTEBOOK(notebook), ip_tab, gtk_label_new(_("Network")));
        gtk_notebook_page_num(GTK_NOTEBOOK(notebook), ip_tab);
    }

    // Emit signal to hide advanced and security tabs in case of IAX
    if (protocol_combo)
        g_signal_emit_by_name(protocol_combo, "changed", NULL);

    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),  0);

    /* Run dialog */
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    // Update protocol in case it changed
    gchar *proto;

    if (protocol_combo)
        proto = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(protocol_combo));
    else
        proto = g_strdup("SIP");

    // If cancel button is pressed
    if (response == GTK_RESPONSE_CANCEL) {
        gtk_widget_destroy(dialog);
        g_free(proto);
        return;
    }

    if (!is_IP2IP(account)) {
        g_hash_table_replace(account->properties,
                             g_strdup(ACCOUNT_ALIAS),
                             g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_alias))));
        g_hash_table_replace(account->properties,
                             g_strdup(ACCOUNT_TYPE),
                             g_strdup(proto));
        g_hash_table_replace(account->properties,
                             g_strdup(ACCOUNT_HOSTNAME),
                             g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_hostname))));
        g_hash_table_replace(account->properties,
                             g_strdup(ACCOUNT_USERNAME),
                             g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_username))));
        g_hash_table_replace(account->properties,
                             g_strdup(ACCOUNT_PASSWORD),
                             g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_password))));
        g_hash_table_replace(account->properties,
                             g_strdup(ACCOUNT_MAILBOX),
                             g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_mailbox))));
    }

    if (g_strcmp0(proto, "SIP") == 0) {
        if (!is_IP2IP(account)) {

            g_hash_table_replace(account->properties,
                                 g_strdup(ACCOUNT_RESOLVE_ONCE),
                                 g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(entry_resolve_name_only_once)) ? "false" : "true"));

            g_hash_table_replace(account->properties,
                                 g_strdup(ACCOUNT_REGISTRATION_EXPIRE),
                                 g_strdup(gtk_entry_get_text(GTK_ENTRY(expire_spin_box))));

            g_hash_table_replace(account->properties,
                                 g_strdup(ACCOUNT_ROUTE),
                                 g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_route_set))));


            g_hash_table_replace(account->properties,
                                 g_strdup(ACCOUNT_USERAGENT),
                                 g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_user_agent))));

            g_hash_table_replace(account->properties, g_strdup(ACCOUNT_SIP_STUN_ENABLED),
                                 g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(use_stun_check_box)) ? "true":"false"));

            g_hash_table_replace(account->properties, g_strdup(ACCOUNT_SIP_STUN_SERVER),
                                 g_strdup(gtk_entry_get_text(GTK_ENTRY(stun_server_entry))));

            g_hash_table_replace(account->properties, g_strdup(PUBLISHED_SAMEAS_LOCAL), g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button)) ? "true":"false"));

            if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(same_as_local_radio_button))) {
                g_hash_table_replace(account->properties,
                                     g_strdup(PUBLISHED_PORT),
                                     g_strdup(gtk_entry_get_text(GTK_ENTRY(published_port_spin_box))));

                g_hash_table_replace(account->properties,
                                     g_strdup(PUBLISHED_ADDRESS),
                                     g_strdup(gtk_entry_get_text(GTK_ENTRY(published_address_entry))));
            } else {
                g_hash_table_replace(account->properties,
                                     g_strdup(PUBLISHED_PORT),
                                     g_strdup(gtk_entry_get_text(GTK_ENTRY(local_port_spin_box))));
                gchar *local_interface = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(local_address_combo));

                gchar *published_address = dbus_get_address_from_interface_name(local_interface);
                g_free(local_interface);

                g_hash_table_replace(account->properties,
                                     g_strdup(PUBLISHED_ADDRESS),
                                     published_address);
            }
        }

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(overrtp))) {
            DEBUG("Config: Set dtmf over rtp");
            g_hash_table_replace(account->properties, g_strdup(ACCOUNT_DTMF_TYPE), g_strdup(OVERRTP));
        } else {
            DEBUG("Config: Set dtmf over sip");
            g_hash_table_replace(account->properties, g_strdup(ACCOUNT_DTMF_TYPE), g_strdup(SIPINFO));
        }

        gchar* key_exchange = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(key_exchange_combo));

        if (utf8_case_cmp(key_exchange, "ZRTP") == 0) {
            g_hash_table_replace(account->properties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("true"));
            g_hash_table_replace(account->properties, g_strdup(ACCOUNT_KEY_EXCHANGE), g_strdup(ZRTP));
        } else if (utf8_case_cmp(key_exchange, "SDES") == 0) {
            g_hash_table_replace(account->properties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("true"));
            g_hash_table_replace(account->properties, g_strdup(ACCOUNT_KEY_EXCHANGE), g_strdup(SDES));
        } else {
            g_hash_table_replace(account->properties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("false"));
            g_hash_table_replace(account->properties, g_strdup(ACCOUNT_KEY_EXCHANGE), g_strdup(""));
        }

        g_free(key_exchange);
        g_hash_table_replace(account->properties, g_strdup(TLS_ENABLE),
                             g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(use_sip_tls_check_box)) ? "true":"false"));

        gboolean toneEnabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enable_tone));
        g_hash_table_replace(account->properties,
                             g_strdup(CONFIG_RINGTONE_ENABLED),
                             g_strdup(toneEnabled ? "true" : "false"));

        g_hash_table_replace(account->properties,
                             g_strdup(CONFIG_RINGTONE_PATH),
                             g_strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser))));

        g_hash_table_replace(account->properties,
                             g_strdup(LOCAL_INTERFACE),
                             gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(local_address_combo)));

        g_hash_table_replace(account->properties,
                             g_strdup(LOCAL_PORT),
                             g_strdup(gtk_entry_get_text(GTK_ENTRY(local_port_spin_box))));

    }

    /** @todo Verify if it's the best condition to check */
    if (utf8_case_cmp(account->accountID, "new") == 0)
        dbus_add_account(account);
    else
        dbus_set_account_details(account);

    if (g_strcmp0(current_protocol, "SIP") == 0) {
        /* Set new credentials if any */
        DEBUG("Config: Setting credentials");

        if (!is_IP2IP(account)) {
            DEBUG("Config: Get new credentials");
            account->credential_information = get_new_credential();

            if (account->credential_information)
                dbus_set_credentials(account);
        }
    }

    // propagate changes to the daemon
    codec_list_update_to_daemon(account);

    gtk_widget_destroy(dialog);
    g_free(current_protocol);
    g_free(proto);
}

