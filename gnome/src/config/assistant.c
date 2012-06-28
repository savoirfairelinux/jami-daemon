/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include <glib/gi18n.h>
#include "gtk2_wrappers.h"
#include "unused.h"
#include "assistant.h"
#include "logger.h"
#include "dbus.h"
#include "reqaccount.h"
#include "account_schema.h"

#define SFLPHONE_ORG_SERVER "sip.sflphone.org"

struct _wizard *wiz;
static int account_type;
static gboolean use_sflphone_org = TRUE;
static account_t* current;
static char message[1024];
/**
 * Forward function
 */
static gint forward_page_func(gint current_page, gpointer data);

/**
 * Page template
 */
static GtkWidget* create_vbox(GtkAssistantPageType type, const gchar *title, const gchar *section);
void prefill_sip(void);

void set_account_type(GtkWidget* widget, gpointer data UNUSED)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
        account_type = _SIP;
    else
        account_type = _IAX;
}

static void show_password_cb(GtkWidget *widget UNUSED, gpointer data)
{
    gtk_entry_set_visibility(GTK_ENTRY(data), !gtk_entry_get_visibility(GTK_ENTRY(data)));
}


/**
 * Fills string message with the final message of account registration
 * with alias, server and username specified.
 */
void getMessageSummary(const gchar * alias, const gchar * server, const gchar * username, const gboolean zrtp)
{
    char var[64];
    sprintf(message, _("This assistant is now finished."));
    strcat(message, "\n");
    strcat(message, _("You can at any time check your registration state or modify your accounts parameters in the Options/Accounts window."));
    strcat(message, "\n\n");

    strcat(message, _("Alias"));
    snprintf(var, sizeof(var), " :   %s\n", alias);
    strcat(message, var);

    strcat(message, _("Server"));
    snprintf(var, sizeof(var), " :   %s\n", server);
    strcat(message, var);

    strcat(message, _("Username"));
    snprintf(var, sizeof(var), " :   %s\n", username);
    strcat(message, var);

    strcat(message, _("Security: "));

    if (zrtp)
        strcat(message, _("SRTP/ZRTP draft-zimmermann"));
    else
        strcat(message, _("None"));
}

void set_sflphone_org(GtkWidget* widget, gpointer data UNUSED)
{
    use_sflphone_org = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

/**
 * Callback when the close button of the dialog is clicked
 * Action : close the assistant widget and get back to sflphone main window
 */
static void close_callback(void)
{
    gtk_widget_destroy(wiz->assistant);
    g_free(wiz);
    wiz = NULL;

    status_bar_display_account();
}

/**
 * Callback when the cancel button of the dialog is clicked
 * Action : close the assistant widget and get back to sflphone main window
 */
static void cancel_callback(void)
{
    gtk_widget_destroy(wiz->assistant);
    g_free(wiz);
    wiz = NULL;

    status_bar_display_account();
}

/**
 * Callback when the button apply is clicked
 * Action : Set the account parameters with the entries values and called dbus_add_account
 */
static void sip_apply_callback(void)
{
    if (use_sflphone_org) {
        prefill_sip();
        account_type = _SIP;
    }

    if (account_type == _SIP) {
        account_insert(current, CONFIG_ACCOUNT_ALIAS, gtk_entry_get_text(GTK_ENTRY(wiz->sip_alias)));
        account_insert(current, CONFIG_ACCOUNT_ENABLE, "true");
        account_insert(current, CONFIG_ACCOUNT_MAILBOX, gtk_entry_get_text(GTK_ENTRY(wiz->sip_voicemail)));
        account_insert(current, CONFIG_ACCOUNT_TYPE, "SIP");
        account_insert(current, CONFIG_ACCOUNT_HOSTNAME, gtk_entry_get_text(GTK_ENTRY(wiz->sip_server)));
        account_insert(current, CONFIG_ACCOUNT_PASSWORD, gtk_entry_get_text(GTK_ENTRY(wiz->sip_password)));
        account_insert(current, CONFIG_ACCOUNT_USERNAME, gtk_entry_get_text(GTK_ENTRY(wiz->sip_username)));
        account_insert(current, CONFIG_STUN_ENABLE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->enable)) ? "true" : "false");
        account_insert(current, CONFIG_STUN_SERVER, gtk_entry_get_text(GTK_ENTRY(wiz->addr)));

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->zrtp_enable)) == TRUE) {
            account_insert(current, CONFIG_SRTP_ENABLE, "true");
            account_insert(current, CONFIG_SRTP_KEY_EXCHANGE, ZRTP);
            account_insert(current, CONFIG_ZRTP_DISPLAY_SAS, "true");
            account_insert(current, CONFIG_ZRTP_NOT_SUPP_WARNING, "true");
            account_insert(current, CONFIG_ZRTP_HELLO_HASH, "true");
            account_insert(current, CONFIG_ZRTP_DISPLAY_SAS_ONCE, "false");
        }

        // Add default interface info
        gchar ** iface_list = NULL;
        iface_list = (gchar**) dbus_get_all_ip_interface_by_name();
        gchar ** iface = NULL;

        // select the first interface available
        iface = iface_list;
        DEBUG("Selected interface %s", *iface);

        account_insert(current, CONFIG_LOCAL_INTERFACE, *iface);
        account_insert(current, CONFIG_PUBLISHED_ADDRESS, *iface);

        dbus_add_account(current);
        getMessageSummary(gtk_entry_get_text(GTK_ENTRY(wiz->sip_alias)),
                          gtk_entry_get_text(GTK_ENTRY(wiz->sip_server)),
                          gtk_entry_get_text(GTK_ENTRY(wiz->sip_username)),
                          (gboolean)(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->zrtp_enable)))
                         );

        gtk_label_set_text(GTK_LABEL(wiz->label_summary), message);
    }
}

/**
 * Callback when the button apply is clicked
 * Action : Set the account parameters with the entries values and called dbus_add_account
 */
static void iax_apply_callback(void)
{
    if (account_type == _IAX) {
        account_insert(current, CONFIG_ACCOUNT_ALIAS, gtk_entry_get_text(GTK_ENTRY(wiz->iax_alias)));
        account_insert(current, CONFIG_ACCOUNT_ENABLE, "true");
        account_insert(current, CONFIG_ACCOUNT_MAILBOX, gtk_entry_get_text(GTK_ENTRY(wiz->iax_voicemail)));
        account_insert(current, CONFIG_ACCOUNT_TYPE, "IAX");
        account_insert(current, CONFIG_ACCOUNT_USERNAME, gtk_entry_get_text(GTK_ENTRY(wiz->iax_username)));
        account_insert(current, CONFIG_ACCOUNT_HOSTNAME, gtk_entry_get_text(GTK_ENTRY(wiz->iax_server)));
        account_insert(current, CONFIG_ACCOUNT_PASSWORD, gtk_entry_get_text(GTK_ENTRY(wiz->iax_password)));

        dbus_add_account(current);
        getMessageSummary(gtk_entry_get_text(GTK_ENTRY(wiz->iax_alias)),
                          gtk_entry_get_text(GTK_ENTRY(wiz->iax_server)),
                          gtk_entry_get_text(GTK_ENTRY(wiz->iax_username)),
                          FALSE);

        gtk_label_set_text(GTK_LABEL(wiz->label_summary), message);
    }
}

void enable_stun(GtkWidget* widget)
{
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->addr), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

void build_wizard(void)
{
    use_sflphone_org = TRUE;

    if (wiz)
        return;

    wiz = (struct _wizard*) g_malloc(sizeof(struct _wizard));
    current = create_default_account();

    if (current->properties == NULL) {
        DEBUG("Failed to get default values. Creating from scratch");
        current->properties = g_hash_table_new(NULL, g_str_equal);
    }

    wiz->assistant = gtk_assistant_new();

    gtk_window_set_title(GTK_WINDOW(wiz->assistant), _("SFLphone account creation wizard"));
    gtk_window_set_position(GTK_WINDOW(wiz->assistant), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(wiz->assistant), 200, 200);

    build_intro();
    build_sfl_or_account();
    build_select_account();
    build_sip_account_configuration();
    build_nat_settings();
    build_iax_account_configuration();
    build_email_configuration();
    build_summary();

    g_signal_connect(G_OBJECT(wiz->assistant), "close", G_CALLBACK(close_callback), NULL);

    g_signal_connect(G_OBJECT(wiz->assistant), "cancel", G_CALLBACK(cancel_callback), NULL);

    gtk_widget_show_all(wiz->assistant);

    gtk_assistant_set_forward_page_func(GTK_ASSISTANT(wiz->assistant), (GtkAssistantPageFunc) forward_page_func, NULL, NULL);
    gtk_assistant_update_buttons_state(GTK_ASSISTANT(wiz->assistant));
}

GtkWidget* build_intro()
{
    wiz->intro = create_vbox(GTK_ASSISTANT_PAGE_INTRO, "SFLphone GNOME client", _("Welcome to the Account creation wizard of SFLphone!"));
    GtkWidget *label = gtk_label_new(_("This installation wizard will help you configure an account."));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(label), 380, -1);
    gtk_box_pack_start(GTK_BOX(wiz->intro), label, FALSE, TRUE, 0);

    gtk_assistant_set_page_complete(GTK_ASSISTANT(wiz->assistant),  wiz->intro, TRUE);
    return wiz->intro;
}

GtkWidget* build_select_account()
{
    wiz->protocols = create_vbox(GTK_ASSISTANT_PAGE_CONTENT, _("VoIP Protocols"), _("Select an account type"));

    GtkWidget *sip = gtk_radio_button_new_with_label(NULL, _("SIP (Session Initiation Protocol)"));
    gtk_box_pack_start(GTK_BOX(wiz->protocols), sip, TRUE, TRUE, 0);
    GtkWidget *iax = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sip), _("IAX2 (InterAsterix Exchange)"));
    gtk_box_pack_start(GTK_BOX(wiz->protocols), iax, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(sip), "clicked", G_CALLBACK(set_account_type), NULL);

    gtk_assistant_set_page_complete(GTK_ASSISTANT(wiz->assistant),  wiz->protocols, TRUE);
    return wiz->protocols;
}


GtkWidget* build_sfl_or_account()
{
    wiz->sflphone_org = create_vbox(GTK_ASSISTANT_PAGE_CONTENT, _("Account"), _("Please select one of the following options"));

    GtkWidget *sfl = gtk_radio_button_new_with_label(NULL, _("Create a free SIP/IAX2 account on sflphone.org \n(For testing purpose only)"));
    gtk_box_pack_start(GTK_BOX(wiz->sflphone_org), sfl, TRUE, TRUE, 0);
    GtkWidget *cus = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sfl), _("Register an existing SIP or IAX2 account"));
    gtk_box_pack_start(GTK_BOX(wiz->sflphone_org), cus, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(sfl), "clicked", G_CALLBACK(set_sflphone_org), NULL);

    return wiz->sflphone_org;
}


GtkWidget* build_sip_account_configuration(void)
{
    GtkWidget* table;
    GtkWidget* label;
    GtkWidget * clearTextCheckbox;

    wiz->sip_account = create_vbox(GTK_ASSISTANT_PAGE_CONTENT, _("SIP account settings"), _("Please fill the following information"));
    // table
    table = gtk_table_new(7, 2,  FALSE/* homogeneous */);
    gtk_table_set_row_spacings(GTK_TABLE(table), 10);
    gtk_table_set_col_spacings(GTK_TABLE(table), 10);
    gtk_box_pack_start(GTK_BOX(wiz->sip_account), table, TRUE, TRUE, 0);

    // alias field
    label = gtk_label_new_with_mnemonic(_("_Alias"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->sip_alias = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->sip_alias);
    gtk_table_attach(GTK_TABLE(table), wiz->sip_alias, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // server field
    label = gtk_label_new_with_mnemonic(_("_Host name"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->sip_server = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->sip_server);
    gtk_table_attach(GTK_TABLE(table), wiz->sip_server, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // username field
    label = gtk_label_new_with_mnemonic(_("_User name"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->sip_username = gtk_entry_new();
    gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(wiz->sip_username), GTK_ENTRY_ICON_PRIMARY, gdk_pixbuf_new_from_file(ICONS_DIR "/stock_person.svg", NULL));
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->sip_username);
    gtk_table_attach(GTK_TABLE(table), wiz->sip_username, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // password field

    label = gtk_label_new_with_mnemonic(_("_Password"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->sip_password = gtk_entry_new();
    gtk_entry_set_icon_from_stock(GTK_ENTRY(wiz->sip_password), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_DIALOG_AUTHENTICATION);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->sip_password);
    gtk_entry_set_visibility(GTK_ENTRY(wiz->sip_password), FALSE);
    gtk_table_attach(GTK_TABLE(table), wiz->sip_password, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    clearTextCheckbox = gtk_check_button_new_with_mnemonic(_("Show password"));
    g_signal_connect(clearTextCheckbox, "toggled", G_CALLBACK(show_password_cb), wiz->sip_password);
    gtk_table_attach(GTK_TABLE(table), clearTextCheckbox, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // voicemail number field
    label = gtk_label_new_with_mnemonic(_("_Voicemail number"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->sip_voicemail = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->sip_voicemail);
    gtk_table_attach(GTK_TABLE(table), wiz->sip_voicemail, 1, 2, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // Security options
    wiz->zrtp_enable = gtk_check_button_new_with_mnemonic(_("Secure communications with _ZRTP"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wiz->zrtp_enable), FALSE);
    gtk_table_attach(GTK_TABLE(table), wiz->zrtp_enable, 0, 1, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->zrtp_enable), TRUE);

    //gtk_assistant_set_page_complete(GTK_ASSISTANT(wiz->assistant),  wiz->sip_account, TRUE);
    return wiz->sip_account;
}

GtkWidget* build_email_configuration(void)
{
    GtkWidget* label;
    GtkWidget*  table;

    wiz->email = create_vbox(GTK_ASSISTANT_PAGE_CONTENT, _("Optional email address"), _("This email address will be used to send your voicemail messages."));

    table = gtk_table_new(4, 2,  FALSE/* homogeneous */);
    gtk_table_set_row_spacings(GTK_TABLE(table), 10);
    gtk_table_set_col_spacings(GTK_TABLE(table), 10);
    gtk_box_pack_start(GTK_BOX(wiz->email), table, TRUE, TRUE, 0);

    // email field
    label = gtk_label_new_with_mnemonic(_("_Email address"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->mailbox = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->mailbox);
    gtk_table_attach(GTK_TABLE(table), wiz->mailbox, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // Security options
    wiz->zrtp_enable = gtk_check_button_new_with_mnemonic(_("Secure communications with _ZRTP"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wiz->zrtp_enable), FALSE);
    gtk_table_attach(GTK_TABLE(table), wiz->zrtp_enable, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->zrtp_enable), TRUE);

    return wiz->email;
}

GtkWidget* build_iax_account_configuration(void)
{
    GtkWidget* label;
    GtkWidget*  table;
    GtkWidget * clearTextCheckbox;

    wiz->iax_account = create_vbox(GTK_ASSISTANT_PAGE_CONFIRM, _("IAX2 account settings"), _("Please fill the following information"));

    table = gtk_table_new(6, 2,  FALSE/* homogeneous */);
    gtk_table_set_row_spacings(GTK_TABLE(table), 10);
    gtk_table_set_col_spacings(GTK_TABLE(table), 10);
    gtk_box_pack_start(GTK_BOX(wiz->iax_account), table, TRUE, TRUE, 0);

    // alias field
    label = gtk_label_new_with_mnemonic(_("_Alias"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->iax_alias = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->iax_alias);
    gtk_table_attach(GTK_TABLE(table), wiz->iax_alias, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // server field
    label = gtk_label_new_with_mnemonic(_("_Host name"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->iax_server = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->iax_server);
    gtk_table_attach(GTK_TABLE(table), wiz->iax_server, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // username field
    label = gtk_label_new_with_mnemonic(_("_User name"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->iax_username = gtk_entry_new();
    gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(wiz->iax_username), GTK_ENTRY_ICON_PRIMARY, gdk_pixbuf_new_from_file(ICONS_DIR "/stock_person.svg", NULL));
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->iax_username);
    gtk_table_attach(GTK_TABLE(table), wiz->iax_username, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // password field
    label = gtk_label_new_with_mnemonic(_("_Password"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->iax_password = gtk_entry_new();
    gtk_entry_set_icon_from_stock(GTK_ENTRY(wiz->iax_password), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_DIALOG_AUTHENTICATION);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->iax_password);
    gtk_entry_set_visibility(GTK_ENTRY(wiz->iax_password), FALSE);
    gtk_table_attach(GTK_TABLE(table), wiz->iax_password, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    clearTextCheckbox = gtk_check_button_new_with_mnemonic(_("Show password"));
    g_signal_connect(clearTextCheckbox, "toggled", G_CALLBACK(show_password_cb), wiz->iax_password);
    gtk_table_attach(GTK_TABLE(table), clearTextCheckbox, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // voicemail number field
    label = gtk_label_new_with_mnemonic(_("_Voicemail number"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->iax_voicemail = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->iax_voicemail);
    gtk_table_attach(GTK_TABLE(table), wiz->iax_voicemail, 1, 2, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    current->state = ACCOUNT_STATE_UNREGISTERED;

    g_signal_connect(G_OBJECT(wiz->assistant), "apply", G_CALLBACK(iax_apply_callback), NULL);

    return wiz->iax_account;
}

GtkWidget* build_nat_settings(void)
{
    GtkWidget* label;
    GtkWidget* table;

    wiz->nat = create_vbox(GTK_ASSISTANT_PAGE_CONFIRM, _("Network Address Translation (NAT)"), _("You should probably enable this if you are behind a firewall."));

    // table
    table = gtk_table_new(2, 2, FALSE/* homogeneous */);
    gtk_table_set_row_spacings(GTK_TABLE(table), 10);
    gtk_table_set_col_spacings(GTK_TABLE(table), 10);
    gtk_box_pack_start(GTK_BOX(wiz->nat), table, TRUE, TRUE, 0);

    // enable
    wiz->enable = gtk_check_button_new_with_mnemonic(_("E_nable STUN"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wiz->enable), FALSE);
    gtk_table_attach(GTK_TABLE(table), wiz->enable, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->enable), TRUE);
    g_signal_connect(G_OBJECT(GTK_TOGGLE_BUTTON(wiz->enable)), "toggled", G_CALLBACK(enable_stun), NULL);

    // server address
    label = gtk_label_new_with_mnemonic(_("_STUN server"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->addr = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->addr);
    gtk_table_attach(GTK_TABLE(table), wiz->addr, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->addr), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->enable)));

    g_signal_connect(G_OBJECT(wiz->assistant), "apply", G_CALLBACK(sip_apply_callback), NULL);

    return wiz->nat;
}

GtkWidget* build_summary()
{
    wiz->summary = create_vbox(GTK_ASSISTANT_PAGE_SUMMARY, _("Account Registration"), _("Congratulations!"));

    strcpy(message,"");
    wiz->label_summary = gtk_label_new(message);
    gtk_label_set_selectable(GTK_LABEL(wiz->label_summary), TRUE);
    gtk_misc_set_alignment(GTK_MISC(wiz->label_summary), 0, 0);
    gtk_label_set_line_wrap(GTK_LABEL(wiz->label_summary), TRUE);
    gtk_box_pack_start(GTK_BOX(wiz->summary), wiz->label_summary, FALSE, TRUE, 0);

    return wiz->summary;
}

GtkWidget* build_registration_error()
{
    GtkWidget *label;
    wiz->reg_failed = create_vbox(GTK_ASSISTANT_PAGE_SUMMARY, "Account Registration", "Registration error");

    label = gtk_label_new(" Please correct the information.");
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(label), 380, -1);
    gtk_box_pack_start(GTK_BOX(wiz->reg_failed), label, FALSE, TRUE, 0);

    return wiz->reg_failed;
}

void set_sip_infos_sentivite(gboolean b)
{
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->sip_alias), b);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->sip_server), b);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->sip_username), b);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->sip_password), b);
}

void prefill_sip(void)
{
    if (use_sflphone_org) {
        char alias[300];
        char *email;
        email = (char *) gtk_entry_get_text(GTK_ENTRY(wiz->mailbox));
        rest_account ra = get_rest_account(SFLPHONE_ORG_SERVER,email);

        if (ra.success) {
            set_sip_infos_sentivite(FALSE);
            strcpy(alias,ra.user);
            strcat(alias,"@");
            strcat(alias,"sip.sflphone.org");
            gtk_entry_set_text(GTK_ENTRY(wiz->sip_alias),alias);
            gtk_entry_set_text(GTK_ENTRY(wiz->sip_server), SFLPHONE_ORG_SERVER);
            gtk_entry_set_text(GTK_ENTRY(wiz->sip_username), ra.user);
            gtk_entry_set_text(GTK_ENTRY(wiz->sip_password), ra.passwd);
        }
    }
}

typedef enum {
    PAGE_INTRO,
    PAGE_SFL,
    PAGE_TYPE,
    PAGE_SIP,
    PAGE_STUN,
    PAGE_IAX,
    PAGE_EMAIL,
    PAGE_SUMMARY
} assistant_state;

static gint forward_page_func(gint current_page, gpointer data UNUSED)
{
    gint next_page = 0;

    switch (current_page) {
        case PAGE_INTRO:
            next_page = PAGE_SFL;
            break;
        case PAGE_SFL:

            if (use_sflphone_org)
                next_page = PAGE_EMAIL;
            else
                next_page = PAGE_TYPE;

            break;
        case PAGE_TYPE:

            if (account_type == _SIP) {
                set_sip_infos_sentivite(TRUE);
                next_page = PAGE_SIP;
            } else {
                next_page = PAGE_IAX;
            }

            break;
        case PAGE_SIP:
            next_page = PAGE_STUN;
            break;
        case PAGE_EMAIL:
            next_page = PAGE_STUN;
            break;
        case PAGE_STUN:
            next_page = PAGE_SUMMARY;
            break;
        case PAGE_IAX:
            next_page = PAGE_SUMMARY;
            break;
        case PAGE_SUMMARY:
            next_page = PAGE_SUMMARY;
            break;
        default:
            next_page = -1;
    }

    return next_page;
}


static GtkWidget* create_vbox(GtkAssistantPageType type, const gchar *title, const gchar *section)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 24);

    gtk_assistant_append_page(GTK_ASSISTANT(wiz->assistant), vbox);
    gtk_assistant_set_page_type(GTK_ASSISTANT(wiz->assistant), vbox, type);
    gchar *str = g_strdup_printf(" %s", title);
    gtk_assistant_set_page_title(GTK_ASSISTANT(wiz->assistant), vbox, str);
    g_free(str);

    gtk_assistant_set_page_complete(GTK_ASSISTANT(wiz->assistant), vbox, TRUE);

#if 0
    /* FIXME */
    http://developer.gnome.org/gtk3/stable/GtkAssistant.html#gtk-assistant-set-page-header-image
    wiz->logo = gdk_pixbuf_new_from_file(LOGO, NULL);
    gtk_assistant_set_page_header_image(GTK_ASSISTANT(wiz->assistant),vbox, wiz->logo);
    g_object_unref(wiz->logo);
#endif

    if (section) {
        GtkWidget *label = gtk_label_new(NULL);
        str = g_strdup_printf("<b>%s</b>\n", section);
        gtk_label_set_markup(GTK_LABEL(label), str);
        g_free(str);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    }

    return vbox;
}
