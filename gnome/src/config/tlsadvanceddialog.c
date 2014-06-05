/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#include "tlsadvanceddialog.h"
#include "str_utils.h"
#include "account_schema.h"
#include "mainwindow.h"
#include "utils.h"
#include <dbus.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>

static
const gchar *toggle_to_string(GtkWidget *toggle)
{
    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)) ? "true" : "false";
}

/* Caller must free returned string */
static
gchar *
get_filename(GtkWidget *chooser)
{
    return gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
}

static gboolean
confirm_certificate_use(GtkWidget *window)
{
    gchar *msg = g_markup_printf_escaped(_("Are you sure want to use this certificate?"));

    /* Create the widgets */
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_CANCEL,
            "%s", msg);

    gtk_dialog_add_buttons(GTK_DIALOG(dialog), _("Use anyway"), GTK_RESPONSE_OK, NULL);

    gtk_window_set_title(GTK_WINDOW(dialog), _("Invalid certificate detected"));
    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    g_free(msg);

    return response == GTK_RESPONSE_OK;
}

static void
certificate_set_cb(GtkFileChooserButton *widget, gpointer user_data)
{
    gchar *filename = get_filename(GTK_WIDGET(widget));
    gchar *caname = get_filename(GTK_WIDGET(g_object_get_data((GObject *)GTK_WIDGET(widget), "ca")));

    const gboolean is_valid = dbus_check_certificate(caname, filename);

    gboolean contains_key = FALSE;
    GtkWidget *private_key_chooser = user_data;

    /* If certificate is invalid, check if user really wants to use it */
    if (!is_valid && !confirm_certificate_use(gtk_widget_get_toplevel(GTK_WIDGET(widget)))) {
        gtk_file_chooser_unselect_filename(GTK_FILE_CHOOSER(widget), filename);
    } else {

        /* disable private key file chooser if certificate contains key */
        contains_key = dbus_certificate_contains_private_key(filename);
        if (contains_key)
            gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(private_key_chooser));
    }

    /* Defaults to sensitive if no key was found */
    gtk_widget_set_sensitive(private_key_chooser, !contains_key);

    g_free(filename);
}

void show_advanced_tls_options(account_t *account, SFLPhoneClient *client)
{
    GtkDialog *tlsDialog = GTK_DIALOG(gtk_dialog_new_with_buttons(_("Advanced options for TLS"),
                                      GTK_WINDOW(client->win),
                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_STOCK_CANCEL,
                                      GTK_RESPONSE_CANCEL,
                                      GTK_STOCK_SAVE,
                                      GTK_RESPONSE_ACCEPT,
                                      NULL));

    gtk_window_set_resizable(GTK_WINDOW(tlsDialog), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(tlsDialog), 0);

    GtkWidget *ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(tlsDialog)), ret, FALSE, FALSE, 0);

    GtkWidget *frame, *grid;
    gnome_main_section_new_with_grid(_("TLS transport"), &frame, &grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    gchar * description = g_markup_printf_escaped(_("TLS transport can be used along with UDP for those calls that would\n"\
                          "require secure sip transactions (aka SIPS). You can configure a different\n"\
                          "TLS transport for each account. However, each of them will run on a dedicated\n"\
                          "port, different one from each other\n"));

    GtkWidget * label = gtk_label_new(NULL);
    gtk_widget_set_size_request(label, 600, 70);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label), description);
    /* 2x1 */
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 2, 1);

    gchar * tls_listener_port = NULL;
    gchar * tls_ca_list_file = NULL;
    gchar * tls_certificate_file = NULL;
    gchar * tls_private_key_file = NULL;
    gchar * tls_password = NULL;
    gchar * tls_method = NULL;
    gchar * tls_ciphers = NULL;
    gchar * tls_server_name = NULL;
    gchar * verify_server = NULL;
    gchar * verify_client = NULL;
    gchar * require_client_certificate = NULL;
    gchar * negotiation_timeout_sec = NULL;

    if (account->properties != NULL) {
        tls_listener_port = account_lookup(account, CONFIG_TLS_LISTENER_PORT);
        tls_ca_list_file = account_lookup(account, CONFIG_TLS_CA_LIST_FILE);
        tls_certificate_file = account_lookup(account, CONFIG_TLS_CERTIFICATE_FILE);
        tls_private_key_file = account_lookup(account, CONFIG_TLS_PRIVATE_KEY_FILE);
        tls_password = account_lookup(account, CONFIG_TLS_PASSWORD);
        tls_method = account_lookup(account, CONFIG_TLS_METHOD);
        tls_ciphers = account_lookup(account, CONFIG_TLS_CIPHERS);
        tls_server_name = account_lookup(account, CONFIG_TLS_SERVER_NAME);
        verify_server = account_lookup(account, CONFIG_TLS_VERIFY_SERVER);
        verify_client = account_lookup(account, CONFIG_TLS_VERIFY_CLIENT);
        require_client_certificate = account_lookup(account, CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE);
        negotiation_timeout_sec = account_lookup(account, CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC);
    }


    label = gtk_label_new(_("TLS listener port"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
    GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_grid_attach(GTK_GRID(grid), hbox, 1, 2, 1, 1);
    GtkWidget *tlsListenerPort = gtk_spin_button_new_with_range(0, 65535, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), tlsListenerPort);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tlsListenerPort), g_ascii_strtod(tls_listener_port, NULL));
    gtk_box_pack_start(GTK_BOX(hbox), tlsListenerPort, TRUE, TRUE, 0);

    label = gtk_label_new(_("Certificate of Authority list"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1);
    GtkWidget * caListFileChooser = gtk_file_chooser_button_new(_("Choose a CA list file (optional)"), GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_grid_attach(GTK_GRID(grid), caListFileChooser, 1, 3, 1, 1);

    if (tls_ca_list_file && *tls_ca_list_file) {
        GFile *file = g_file_new_for_path(tls_ca_list_file);
        gtk_file_chooser_set_file(GTK_FILE_CHOOSER(caListFileChooser), file, NULL);
        g_object_unref(file);
    } else {
        gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(caListFileChooser));
    }

    label = gtk_label_new(_("Public endpoint certificate file"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 4, 1, 1);
    GtkWidget * certificateFileChooser = gtk_file_chooser_button_new(_("Choose a public endpoint certificate (optional)"), GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_grid_attach(GTK_GRID(grid), certificateFileChooser, 1, 4, 1, 1);

    gboolean contains_key = FALSE;

    /* save the ca path in order to use it when validating the certificate */
    g_object_set_data((GObject *)certificateFileChooser, "ca", GTK_WIDGET(caListFileChooser));

    if (!tls_certificate_file) {
        gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(caListFileChooser));
    } else {
        if (!*tls_certificate_file) {
            gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(certificateFileChooser));
        } else {
            GFile * file = g_file_new_for_path(tls_certificate_file);
            gtk_file_chooser_set_file(GTK_FILE_CHOOSER(certificateFileChooser), file, NULL);
            g_object_unref(file);
            contains_key = dbus_certificate_contains_private_key(tls_certificate_file);
        }
    }

    label = gtk_label_new(("Private key file"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 5, 1, 1);
    GtkWidget *privateKeyFileChooser = gtk_file_chooser_button_new(_("Choose a private key file (optional)"), GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_grid_attach(GTK_GRID(grid), privateKeyFileChooser, 1, 5, 1, 1);

    /* if certificate contains private key file, disallow private
     * key file selection */
    g_signal_connect(GTK_FILE_CHOOSER(certificateFileChooser), "file-set", G_CALLBACK(certificate_set_cb), privateKeyFileChooser);

    if (!tls_private_key_file || contains_key) {
        gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(privateKeyFileChooser));
    } else {
        if (!*tls_private_key_file || contains_key) {
            gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(privateKeyFileChooser));
        } else {
            GFile * file = g_file_new_for_path(tls_private_key_file);
            gtk_file_chooser_set_file(GTK_FILE_CHOOSER(privateKeyFileChooser), file, NULL);
            g_object_unref(file);
        }
    }
    gtk_widget_set_sensitive(privateKeyFileChooser, !contains_key);

    label = gtk_label_new_with_mnemonic(_("Password for the private key"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 6, 1, 1);
    GtkWidget * privateKeyPasswordEntry;
    privateKeyPasswordEntry = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(privateKeyPasswordEntry),
            GTK_ENTRY_ICON_PRIMARY, "dialog-password");

    gtk_entry_set_visibility(GTK_ENTRY(privateKeyPasswordEntry), FALSE);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), privateKeyPasswordEntry);
    gtk_entry_set_text(GTK_ENTRY(privateKeyPasswordEntry), tls_password);
    gtk_grid_attach(GTK_GRID(grid), privateKeyPasswordEntry, 1, 6, 1, 1);

    /* TLS protocol methods */

    label = gtk_label_new_with_mnemonic(_("TLS protocol method"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 7, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    GtkWidget *tlsProtocolMethodCombo = gtk_combo_box_text_new();
    gchar** supported_tls_method = dbus_get_supported_tls_method();

    gint supported_tls_method_idx = 0;
    gint idx = 0;
    for (char **supported_tls_method_ptr = supported_tls_method; supported_tls_method_ptr && *supported_tls_method_ptr; supported_tls_method_ptr++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tlsProtocolMethodCombo), NULL, *supported_tls_method_ptr);
        /* make sure current TLS method from is active */
        if (g_strcmp0(*supported_tls_method_ptr, tls_method) == 0)
            supported_tls_method_idx = idx;
        ++idx;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(tlsProtocolMethodCombo), supported_tls_method_idx);

    gtk_label_set_mnemonic_widget(GTK_LABEL(label), tlsProtocolMethodCombo);
    gtk_grid_attach(GTK_GRID(grid), tlsProtocolMethodCombo, 1, 7, 1, 1);

    /* Cipher list */
    GtkWidget * cipherListEntry;
    label = gtk_label_new_with_mnemonic(_("TLS cipher list"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 8, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    cipherListEntry = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), cipherListEntry);
    gtk_entry_set_text(GTK_ENTRY(cipherListEntry), tls_ciphers);
    gtk_grid_attach(GTK_GRID(grid), cipherListEntry, 1, 8, 1, 1);

    GtkWidget * serverNameInstance;
    label = gtk_label_new_with_mnemonic(_("Server name instance for outgoing TLS connection"));
    gtk_grid_attach(GTK_GRID(grid), label, 0, 9, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    serverNameInstance = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), serverNameInstance);
    gtk_entry_set_text(GTK_ENTRY(serverNameInstance), tls_server_name);
    gtk_grid_attach(GTK_GRID(grid), serverNameInstance, 1, 9, 1, 1);

    label = gtk_label_new(_("Negotiation timeout (seconds)"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 10, 1, 1);
    GtkWidget * tlsTimeOutSec;
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_grid_attach(GTK_GRID(grid), hbox, 1, 10, 1, 1);
    tlsTimeOutSec = gtk_spin_button_new_with_range(0, pow(2, sizeof(long)), 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), tlsTimeOutSec);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tlsTimeOutSec), g_ascii_strtod(negotiation_timeout_sec, NULL));
    gtk_box_pack_start(GTK_BOX(hbox), tlsTimeOutSec, TRUE, TRUE, 0);

    GtkWidget * verifyCertificateServer;
    verifyCertificateServer = gtk_check_button_new_with_mnemonic(_("Verify incoming certificates, as a server"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(verifyCertificateServer),
                                 utf8_case_equal(verify_server, "true"));
    gtk_grid_attach(GTK_GRID(grid), verifyCertificateServer, 0, 11, 1, 1);

    GtkWidget * verifyCertificateClient;
    verifyCertificateClient = gtk_check_button_new_with_mnemonic(_("Verify certificates from answer, as a client"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(verifyCertificateClient),
                                 utf8_case_equal(verify_client, "true"));
    gtk_grid_attach(GTK_GRID(grid), verifyCertificateClient, 0, 12, 1, 1);

    GtkWidget * requireCertificate;
    requireCertificate = gtk_check_button_new_with_mnemonic(_("Require certificate for incoming tls connections"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(requireCertificate),
                                 utf8_case_equal(require_client_certificate,"true"));
    gtk_grid_attach(GTK_GRID(grid), requireCertificate, 0, 13, 1, 1);

    gtk_widget_show_all(ret);

    if (gtk_dialog_run(GTK_DIALOG(tlsDialog)) == GTK_RESPONSE_ACCEPT) {
        account_replace(account, CONFIG_TLS_LISTENER_PORT,
                        gtk_entry_get_text(GTK_ENTRY(tlsListenerPort)));

        gchar *ca_list_file = get_filename(caListFileChooser);
        account_replace(account, CONFIG_TLS_CA_LIST_FILE, ca_list_file);
        g_free(ca_list_file);


        gchar *certificate_file = get_filename(certificateFileChooser);
        account_replace(account, CONFIG_TLS_CERTIFICATE_FILE, certificate_file);
        g_free(certificate_file);

        gchar *private_key_file = get_filename(privateKeyFileChooser);
        account_replace(account, CONFIG_TLS_PRIVATE_KEY_FILE, private_key_file);
        g_free(private_key_file);

        account_replace(account, CONFIG_TLS_PASSWORD, gtk_entry_get_text(GTK_ENTRY(privateKeyPasswordEntry)));

        gchar *tls_text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(tlsProtocolMethodCombo));
        account_replace(account, CONFIG_TLS_METHOD, tls_text);
        g_free(tls_text);

        account_replace(account, CONFIG_TLS_CIPHERS, gtk_entry_get_text(GTK_ENTRY(cipherListEntry)));

        account_replace(account, CONFIG_TLS_SERVER_NAME, gtk_entry_get_text(GTK_ENTRY(serverNameInstance)));

        account_replace(account, CONFIG_TLS_VERIFY_SERVER, toggle_to_string(verifyCertificateServer));

        account_replace(account, CONFIG_TLS_VERIFY_CLIENT, toggle_to_string(verifyCertificateClient));

        account_replace(account, CONFIG_TLS_REQUIRE_CLIENT_CERTIFICATE, toggle_to_string(requireCertificate));

        account_replace(account, CONFIG_TLS_NEGOTIATION_TIMEOUT_SEC, gtk_entry_get_text(GTK_ENTRY(tlsTimeOutSec)));
    }

    gtk_widget_destroy(GTK_WIDGET(tlsDialog));
}
