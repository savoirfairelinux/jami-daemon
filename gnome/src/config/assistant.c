/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include "assistant.h"
#include "actions.h"
#include "dbus.h"
#include "account_schema.h"

typedef enum {_SIP, _IAX} account_type_t;

typedef struct {
    GtkWidget *window;
    GtkAssistant *assistant;
    GdkPixbuf *logo;
    GtkWidget *intro;
    /** Page 1  - Protocol selection */
    GtkWidget *protocols;
    GtkWidget *sip;
    GtkWidget *iax;
    /** Page 2 - SIP account creation */
    GtkWidget *sip_account;
    GtkWidget *sip_alias;
    GtkWidget *sip_server;
    GtkWidget *sip_username;
    GtkWidget *sip_password;
    GtkWidget *sip_voicemail;
    GtkWidget *test;
    GtkWidget *state;
    GtkWidget *mailbox;
    GtkWidget *zrtp_enable;
    /** Page 3 - IAX account creation */
    GtkWidget *iax_account;
    GtkWidget *iax_alias;
    GtkWidget *iax_server;
    GtkWidget *iax_username;
    GtkWidget *iax_password;
    GtkWidget *iax_voicemail;
    /** Page 4 - Nat detection */
    GtkWidget *nat;
    GtkWidget *enable;
    GtkWidget *addr;
    /** Page 5 - Registration successful*/
    GtkWidget *summary;
    GtkWidget *label_summary;
    /** Page 6 - Registration failed*/
    GtkWidget *reg_failed;
    /* Wizard context */
    account_t* current;
    char *message;
    account_type_t account_type;
} account_wizard_t;



/**
 * Page template
 */
static GtkWidget* create_vbox(GtkAssistant *assistant, GtkAssistantPageType type, const gchar *title, const gchar *section, gboolean is_complete);
void prefill_sip(void);

void set_account_type(GtkWidget* widget, account_wizard_t *wiz)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
        wiz->account_type = _SIP;
    else
        wiz->account_type = _IAX;
}

static void show_password_cb(G_GNUC_UNUSED GtkWidget *widget, gpointer data)
{
    gtk_entry_set_visibility(GTK_ENTRY(data), !gtk_entry_get_visibility(GTK_ENTRY(data)));
}


/**
 * Fills string message with the final message of account registration
 * with alias, server and username specified.
 */
static void
get_message_summary(account_wizard_t *wiz, const gchar * alias, const gchar * server, const gchar * username, const gboolean zrtp)
{
    wiz->message = g_strdup(_("This assistant is now finished."));
    gchar *tmp =
    g_strconcat(wiz->message, "\n",
    _("You can at any time check your registration state or modify your "
      "accounts parameters in the Options/Accounts window."), "\n\n",
     _("Alias"), " :   ", alias, "\n",
    _("Server"), " :   ", server, "\n",
    _("Username"), " :   ", username, "\n",
    _("Security: "),
    NULL);

    if (zrtp)
        wiz->message = g_strconcat(tmp, _("SRTP/ZRTP draft-zimmermann"), NULL);
    else
        wiz->message = g_strconcat(tmp, _("None"), NULL);
    g_free(tmp);
}

/**
 * Callback when the close button of the dialog is clicked
 * Action : close the assistant widget and get back to sflphone main window
 */
static void close_callback(GtkWidget *widget, account_wizard_t *wiz)
{
    gtk_widget_destroy(widget);
    g_free(wiz);

    status_bar_display_account();
}

/**
 * Callback when the cancel button of the dialog is clicked
 * Action : close the assistant widget and get back to sflphone main window
 */
static void cancel_callback(GtkWidget *widget, account_wizard_t *wiz)
{
    gtk_widget_destroy(widget);
    g_free(wiz);
    wiz = NULL;

    status_bar_display_account();
}

/**
 * Callback when the button apply is clicked
 * Action : Set the account parameters with the entries values and called dbus_add_account
 */
static void sip_apply_callback(G_GNUC_UNUSED GtkWidget *widget, account_wizard_t *wiz)
{
    if (wiz->account_type != _SIP)
        return;

    account_insert(wiz->current, CONFIG_ACCOUNT_ALIAS, gtk_entry_get_text(GTK_ENTRY(wiz->sip_alias)));
    account_insert(wiz->current, CONFIG_ACCOUNT_ENABLE, "true");
    account_insert(wiz->current, CONFIG_ACCOUNT_MAILBOX, gtk_entry_get_text(GTK_ENTRY(wiz->sip_voicemail)));
    account_insert(wiz->current, CONFIG_ACCOUNT_TYPE, "SIP");
    account_insert(wiz->current, CONFIG_ACCOUNT_HOSTNAME, gtk_entry_get_text(GTK_ENTRY(wiz->sip_server)));
    account_insert(wiz->current, CONFIG_ACCOUNT_PASSWORD, gtk_entry_get_text(GTK_ENTRY(wiz->sip_password)));
    account_insert(wiz->current, CONFIG_ACCOUNT_USERNAME, gtk_entry_get_text(GTK_ENTRY(wiz->sip_username)));
    account_insert(wiz->current, CONFIG_STUN_ENABLE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->enable)) ? "true" : "false");
    account_insert(wiz->current, CONFIG_STUN_SERVER, gtk_entry_get_text(GTK_ENTRY(wiz->addr)));

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->zrtp_enable)) == TRUE) {
        account_insert(wiz->current, CONFIG_SRTP_ENABLE, "true");
        account_insert(wiz->current, CONFIG_SRTP_KEY_EXCHANGE, ZRTP);
        account_insert(wiz->current, CONFIG_ZRTP_DISPLAY_SAS, "true");
        account_insert(wiz->current, CONFIG_ZRTP_NOT_SUPP_WARNING, "true");
        account_insert(wiz->current, CONFIG_ZRTP_HELLO_HASH, "true");
        account_insert(wiz->current, CONFIG_ZRTP_DISPLAY_SAS_ONCE, "false");
    }

    // Add default interface info
    gchar ** iface_list = NULL;
    iface_list = (gchar**) dbus_get_all_ip_interface_by_name();
    gchar ** iface = NULL;

    // select the first interface available
    iface = iface_list;
    g_debug("Selected interface %s", *iface);

    account_insert(wiz->current, CONFIG_LOCAL_INTERFACE, *iface);
    account_insert(wiz->current, CONFIG_PUBLISHED_ADDRESS, *iface);

    dbus_add_account(wiz->current);
    get_message_summary(wiz,
                      gtk_entry_get_text(GTK_ENTRY(wiz->sip_alias)),
                      gtk_entry_get_text(GTK_ENTRY(wiz->sip_server)),
                      gtk_entry_get_text(GTK_ENTRY(wiz->sip_username)),
                      (gboolean)(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->zrtp_enable)))
                     );

    gtk_label_set_text(GTK_LABEL(wiz->label_summary), wiz->message);
}

/**
 * Callback when the button apply is clicked
 * Action : Set the account parameters with the entries values and called dbus_add_account
 */
static void iax_apply_callback(G_GNUC_UNUSED GtkWidget *widget, account_wizard_t *wiz)
{
    if (wiz->account_type != _IAX)
        return;

    account_insert(wiz->current, CONFIG_ACCOUNT_ALIAS, gtk_entry_get_text(GTK_ENTRY(wiz->iax_alias)));
    account_insert(wiz->current, CONFIG_ACCOUNT_ENABLE, "true");
    account_insert(wiz->current, CONFIG_ACCOUNT_MAILBOX, gtk_entry_get_text(GTK_ENTRY(wiz->iax_voicemail)));
    account_insert(wiz->current, CONFIG_ACCOUNT_TYPE, "IAX");
    account_insert(wiz->current, CONFIG_ACCOUNT_USERNAME, gtk_entry_get_text(GTK_ENTRY(wiz->iax_username)));
    account_insert(wiz->current, CONFIG_ACCOUNT_HOSTNAME, gtk_entry_get_text(GTK_ENTRY(wiz->iax_server)));
    account_insert(wiz->current, CONFIG_ACCOUNT_PASSWORD, gtk_entry_get_text(GTK_ENTRY(wiz->iax_password)));

    dbus_add_account(wiz->current);
    get_message_summary(wiz,
                      gtk_entry_get_text(GTK_ENTRY(wiz->iax_alias)),
                      gtk_entry_get_text(GTK_ENTRY(wiz->iax_server)),
                      gtk_entry_get_text(GTK_ENTRY(wiz->iax_username)),
                      FALSE);

    gtk_label_set_text(GTK_LABEL(wiz->label_summary), wiz->message);
}

void enable_stun(GtkWidget* widget, account_wizard_t *wiz)
{
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->addr), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}


static void
build_intro(account_wizard_t *wiz)
{
    wiz->intro = create_vbox(wiz->assistant, GTK_ASSISTANT_PAGE_INTRO, "SFLphone GNOME client", _("Welcome to the account registration wizard for SFLphone!"), TRUE);
    GtkWidget *label = gtk_label_new(_("This wizard will help you configure an existing account."));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(label), 380, -1);
    gtk_box_pack_start(GTK_BOX(wiz->intro), label, FALSE, TRUE, 0);

    gtk_assistant_set_page_complete(GTK_ASSISTANT(wiz->assistant), wiz->intro, TRUE);
}

static void
build_select_account(account_wizard_t *wiz)
{
    wiz->protocols = create_vbox(wiz->assistant, GTK_ASSISTANT_PAGE_CONTENT, _("VoIP Protocols"), _("Select an account type"), TRUE);

    GtkWidget *sip = gtk_radio_button_new_with_label(NULL, _("SIP (Session Initiation Protocol)"));
    gtk_box_pack_start(GTK_BOX(wiz->protocols), sip, TRUE, TRUE, 0);
    GtkWidget *iax = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sip), _("IAX2 (InterAsterix Exchange)"));
    gtk_box_pack_start(GTK_BOX(wiz->protocols), iax, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(sip), "clicked", G_CALLBACK(set_account_type), wiz);

    gtk_assistant_set_page_complete(GTK_ASSISTANT(wiz->assistant), wiz->protocols, TRUE);
}

/* Don't allow user to click forward until alias has been set */
static void
alias_changed_cb(GtkEditable *editable, gpointer data)
{
    const gchar *alias = gtk_entry_get_text(GTK_ENTRY(editable));
    account_wizard_t *wiz = data;

    gtk_assistant_set_page_complete(GTK_ASSISTANT(wiz->assistant),
            wiz->account_type ==  _SIP ? wiz->sip_account :
            wiz->iax_account, !!strlen(alias));
}

static void
build_sip_account_configuration(account_wizard_t *wiz)
{
    GtkWidget* label;
    GtkWidget * clearTextCheckbox;

    wiz->sip_account = create_vbox(wiz->assistant, GTK_ASSISTANT_PAGE_CONTENT, _("SIP account settings"), _("Please fill the following information"), TRUE);
    // grid
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(wiz->sip_account), grid, TRUE, TRUE, 0);

    // alias field
    label = gtk_label_new_with_mnemonic(_("_Alias"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->sip_alias = gtk_entry_new();
    g_signal_connect(wiz->sip_alias, "changed", G_CALLBACK(alias_changed_cb), wiz);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->sip_alias);
    gtk_grid_attach(GTK_GRID(grid), wiz->sip_alias, 1, 0, 1, 1);

    // server field
    label = gtk_label_new_with_mnemonic(_("_Host name"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->sip_server = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->sip_server);
    gtk_grid_attach(GTK_GRID(grid), wiz->sip_server, 1, 1, 1, 1);

    // username field
    label = gtk_label_new_with_mnemonic(_("_User name"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->sip_username = gtk_entry_new();
    gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(wiz->sip_username), GTK_ENTRY_ICON_PRIMARY, gdk_pixbuf_new_from_file(ICONS_DIR "/stock_person.svg", NULL));
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->sip_username);
    gtk_grid_attach(GTK_GRID(grid), wiz->sip_username, 1, 2, 1, 1);

    // password field

    label = gtk_label_new_with_mnemonic(_("_Password"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->sip_password = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(wiz->sip_password), GTK_ENTRY_ICON_PRIMARY, "dialog-password");
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->sip_password);
    gtk_entry_set_visibility(GTK_ENTRY(wiz->sip_password), FALSE);
    gtk_grid_attach(GTK_GRID(grid), wiz->sip_password, 1, 3, 1, 1);

    clearTextCheckbox = gtk_check_button_new_with_mnemonic(_("Show password"));
    g_signal_connect(clearTextCheckbox, "toggled", G_CALLBACK(show_password_cb), wiz->sip_password);
    gtk_grid_attach(GTK_GRID(grid), clearTextCheckbox, 1, 4, 1, 1);

    // voicemail number field
    label = gtk_label_new_with_mnemonic(_("_Voicemail number"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 5, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->sip_voicemail = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->sip_voicemail);
    gtk_grid_attach(GTK_GRID(grid), wiz->sip_voicemail, 1, 5, 1, 1);

    // Security options
    wiz->zrtp_enable = gtk_check_button_new_with_mnemonic(_("Secure communications with _ZRTP"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wiz->zrtp_enable), FALSE);
    gtk_grid_attach(GTK_GRID(grid), wiz->zrtp_enable, 0, 6, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->zrtp_enable), TRUE);
}

static void
build_iax_account_configuration(account_wizard_t *wiz)
{
    GtkWidget* label;
    GtkWidget * clearTextCheckbox;

    wiz->iax_account = create_vbox(wiz->assistant, GTK_ASSISTANT_PAGE_CONFIRM, _("IAX2 account settings"), _("Please fill the following information"), TRUE);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(wiz->iax_account), grid, TRUE, TRUE, 0);

    // alias field
    label = gtk_label_new_with_mnemonic(_("_Alias"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->iax_alias = gtk_entry_new();
    g_signal_connect(wiz->iax_alias, "changed", G_CALLBACK(alias_changed_cb), wiz);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->iax_alias);
    gtk_grid_attach(GTK_GRID(grid), wiz->iax_alias, 1, 0, 1, 1);

    // server field
    label = gtk_label_new_with_mnemonic(_("_Host name"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->iax_server = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->iax_server);
    gtk_grid_attach(GTK_GRID(grid), wiz->iax_server, 1, 1, 1, 1);

    // username field
    label = gtk_label_new_with_mnemonic(_("_User name"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->iax_username = gtk_entry_new();
    gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(wiz->iax_username), GTK_ENTRY_ICON_PRIMARY, gdk_pixbuf_new_from_file(ICONS_DIR "/stock_person.svg", NULL));
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->iax_username);
    gtk_grid_attach(GTK_GRID(grid), wiz->iax_username, 1, 2, 1, 1);

    // password field
    label = gtk_label_new_with_mnemonic(_("_Password"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->iax_password = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(wiz->iax_password), GTK_ENTRY_ICON_PRIMARY, "dialog-password");
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->iax_password);
    gtk_entry_set_visibility(GTK_ENTRY(wiz->iax_password), FALSE);
    gtk_grid_attach(GTK_GRID(grid), wiz->iax_password, 1, 3, 1, 1);

    clearTextCheckbox = gtk_check_button_new_with_mnemonic(_("Show password"));
    g_signal_connect(clearTextCheckbox, "toggled", G_CALLBACK(show_password_cb), wiz->iax_password);
    gtk_grid_attach(GTK_GRID(grid), clearTextCheckbox, 1, 4, 1, 1);

    // voicemail number field
    label = gtk_label_new_with_mnemonic(_("_Voicemail number"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 5, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->iax_voicemail = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->iax_voicemail);
    gtk_grid_attach(GTK_GRID(grid), wiz->iax_voicemail, 1, 5, 1, 1);

    wiz->current->state = ACCOUNT_STATE_UNREGISTERED;

    g_signal_connect(G_OBJECT(wiz->assistant), "apply", G_CALLBACK(iax_apply_callback), wiz);
}

static void
build_nat_settings(account_wizard_t *wiz)
{
    GtkWidget* label;

    wiz->nat = create_vbox(wiz->assistant, GTK_ASSISTANT_PAGE_CONFIRM, _("Network Address Translation (NAT)"), _("You should probably enable this if you are behind a firewall."), TRUE);

    // grid
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(wiz->nat), grid, TRUE, TRUE, 0);

    // enable
    wiz->enable = gtk_check_button_new_with_mnemonic(_("E_nable STUN"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wiz->enable), FALSE);
    gtk_grid_attach(GTK_GRID(grid), wiz->enable, 0, 0, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->enable), TRUE);
    g_signal_connect(G_OBJECT(GTK_TOGGLE_BUTTON(wiz->enable)), "toggled", G_CALLBACK(enable_stun), wiz);

    // server address
    label = gtk_label_new_with_mnemonic(_("_STUN server"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    wiz->addr = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), wiz->addr);
    gtk_grid_attach(GTK_GRID(grid), wiz->addr, 1, 1, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->addr), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->enable)));

    g_signal_connect(G_OBJECT(wiz->assistant), "apply", G_CALLBACK(sip_apply_callback), wiz);
}

static void
build_summary(account_wizard_t *wiz)
{
    wiz->summary = create_vbox(wiz->assistant, GTK_ASSISTANT_PAGE_SUMMARY, _("Account Registration"), _("Congratulations!"), TRUE);

    if (wiz->message)
        g_free(wiz->message);
    wiz->message = g_strdup("");
    wiz->label_summary = gtk_label_new(wiz->message);
    gtk_label_set_selectable(GTK_LABEL(wiz->label_summary), TRUE);
    gtk_misc_set_alignment(GTK_MISC(wiz->label_summary), 0, 0);
    gtk_label_set_line_wrap(GTK_LABEL(wiz->label_summary), TRUE);
    gtk_box_pack_start(GTK_BOX(wiz->summary), wiz->label_summary, FALSE, TRUE, 0);
}

static void
sip_info_set_sensitive(account_wizard_t *wiz, gboolean b)
{
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->sip_alias), b);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->sip_server), b);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->sip_username), b);
    gtk_widget_set_sensitive(GTK_WIDGET(wiz->sip_password), b);
}

typedef enum {
    PAGE_INTRO,
    PAGE_TYPE,
    PAGE_SIP,
    PAGE_STUN,
    PAGE_IAX,
    PAGE_SUMMARY
} assistant_state;

static gint forward_page_func(gint current_page, account_wizard_t *wiz)
{
    gint next_page = 0;

    switch (current_page) {
        case PAGE_INTRO:
            next_page = PAGE_TYPE;
            break;
        case PAGE_TYPE:
            if (wiz->account_type == _SIP) {
                sip_info_set_sensitive(wiz, TRUE);
                next_page = PAGE_SIP;
            } else {
                next_page = PAGE_IAX;
            }

            break;
        case PAGE_SIP:
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


static GtkWidget*
create_vbox(GtkAssistant *assistant, GtkAssistantPageType type, const gchar *title, const gchar *section, gboolean is_complete)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 24);

    gtk_assistant_append_page(assistant, vbox);
    gtk_assistant_set_page_type(assistant, vbox, type);
    gchar *str = g_strdup_printf(" %s", title);
    gtk_assistant_set_page_title(assistant, vbox, str);
    g_free(str);

    gtk_assistant_set_page_complete(assistant, vbox, is_complete);

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

void build_wizard(void)
{
    account_wizard_t *wiz = g_new0(account_wizard_t, 1);
    wiz->current = create_default_account();

    if (wiz->current->properties == NULL) {
        g_debug("Failed to get default values. Creating from scratch");
        wiz->current->properties = g_hash_table_new(NULL, g_str_equal);
    }

    wiz->assistant = GTK_ASSISTANT(gtk_assistant_new());

    gtk_window_set_title(GTK_WINDOW(wiz->assistant), _("SFLphone account registration wizard"));
    gtk_window_set_position(GTK_WINDOW(wiz->assistant), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(wiz->assistant), 200, 200);

    build_intro(wiz);
    build_select_account(wiz);
    build_sip_account_configuration(wiz);
    build_nat_settings(wiz);
    build_iax_account_configuration(wiz);
    build_summary(wiz);

    g_signal_connect(G_OBJECT(wiz->assistant), "close", G_CALLBACK(close_callback), wiz);

    g_signal_connect(G_OBJECT(wiz->assistant), "cancel", G_CALLBACK(cancel_callback), wiz);

    gtk_widget_show_all(GTK_WIDGET(wiz->assistant));

    gtk_assistant_set_forward_page_func(GTK_ASSISTANT(wiz->assistant), (GtkAssistantPageFunc) forward_page_func, wiz, NULL);
    gtk_assistant_update_buttons_state(GTK_ASSISTANT(wiz->assistant));
}
