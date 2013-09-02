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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "str_utils.h"
#include "zrtpadvanceddialog.h"
#include "account_schema.h"
#include "utils.h"

void show_advanced_zrtp_options(account_t *account, SFLPhoneClient *client)
{
    gboolean curSasConfirm = TRUE;
    gboolean curHelloEnabled = TRUE;
    gboolean curZrtpNotSuppOther = TRUE;
    gboolean curDisplaySasOnce = FALSE;

    if (account != NULL) {
        curHelloEnabled = utf8_case_equal(account_lookup(account, CONFIG_ZRTP_HELLO_HASH), "true");
        curSasConfirm = utf8_case_equal(account_lookup(account, CONFIG_ZRTP_DISPLAY_SAS), "true");
        curZrtpNotSuppOther = utf8_case_equal(account_lookup(account, CONFIG_ZRTP_NOT_SUPP_WARNING), "true");
        curDisplaySasOnce = utf8_case_equal(account_lookup(account, CONFIG_ZRTP_DISPLAY_SAS_ONCE), "true");
    }

    GtkDialog *securityDialog = GTK_DIALOG(gtk_dialog_new_with_buttons(_("ZRTP Options"),
                                           GTK_WINDOW(client->win),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_STOCK_CANCEL,
                                           GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_SAVE,
                                           GTK_RESPONSE_ACCEPT,
                                           NULL));
    gtk_window_set_resizable(GTK_WINDOW(securityDialog), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(securityDialog), 0);

    GtkWidget *gridZrtp = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(gridZrtp), 10);
    gtk_grid_set_column_spacing(GTK_GRID(gridZrtp), 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(securityDialog)), gridZrtp, FALSE, FALSE, 0);
    gtk_widget_show(gridZrtp);

    GtkWidget *enableHelloHash = gtk_check_button_new_with_mnemonic(_("Send Hello Hash in S_DP"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableHelloHash), curHelloEnabled);
    gtk_grid_attach(GTK_GRID(gridZrtp), enableHelloHash, 0, 2, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(enableHelloHash), TRUE);

    GtkWidget *enableSASConfirm = gtk_check_button_new_with_mnemonic(_("Ask User to Confirm SAS"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableSASConfirm), curSasConfirm);
    gtk_grid_attach(GTK_GRID(gridZrtp), enableSASConfirm, 0, 3, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(enableSASConfirm), TRUE);

    GtkWidget *enableZrtpNotSuppOther = gtk_check_button_new_with_mnemonic(_("_Warn if ZRTP not supported"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableZrtpNotSuppOther), curZrtpNotSuppOther);
    gtk_grid_attach(GTK_GRID(gridZrtp), enableZrtpNotSuppOther, 0, 4, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(enableZrtpNotSuppOther), TRUE);

    GtkWidget *displaySasOnce = gtk_check_button_new_with_mnemonic(_("Display SAS once for hold events"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(displaySasOnce), curDisplaySasOnce);
    gtk_grid_attach(GTK_GRID(gridZrtp), displaySasOnce, 0, 5, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(displaySasOnce), TRUE);

    gtk_widget_show_all(gridZrtp);

    gtk_container_set_border_width(GTK_CONTAINER(gridZrtp), 10);

    if (gtk_dialog_run(GTK_DIALOG(securityDialog)) == GTK_RESPONSE_ACCEPT) {
        account_replace(account, CONFIG_ZRTP_DISPLAY_SAS,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableSASConfirm)) ? "true": "false");

        account_replace(account, CONFIG_ZRTP_DISPLAY_SAS_ONCE,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(displaySasOnce)) ? "true": "false");

        account_replace(account, CONFIG_ZRTP_HELLO_HASH,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableHelloHash)) ? "true": "false");

        account_replace(account, CONFIG_ZRTP_NOT_SUPP_WARNING,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableZrtpNotSuppOther)) ? "true": "false");
    }

    gtk_widget_destroy(GTK_WIDGET(securityDialog));
}


void show_advanced_sdes_options(account_t *account, SFLPhoneClient *client)
{
    gboolean rtpFallback = FALSE;

    if (account != NULL)
        rtpFallback = utf8_case_equal(account_lookup(account, CONFIG_SRTP_RTP_FALLBACK), "true");

    GtkDialog *securityDialog = GTK_DIALOG(gtk_dialog_new_with_buttons(_("SDES Options"),
                                           GTK_WINDOW(client->win),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_STOCK_CANCEL,
                                           GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE,
                                           GTK_RESPONSE_ACCEPT, NULL));

    gtk_window_set_resizable(GTK_WINDOW(securityDialog), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(securityDialog), 0);

    GtkWidget *sdesGrid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(sdesGrid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(sdesGrid), 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(securityDialog)), sdesGrid, FALSE, FALSE, 0);
    gtk_widget_show(sdesGrid);

    GtkWidget *enableRtpFallback = gtk_check_button_new_with_mnemonic(_("Fallback on RTP on SDES failure"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableRtpFallback), rtpFallback);
    gtk_grid_attach(GTK_GRID(sdesGrid), enableRtpFallback, 0, 2, 1, 1);
    gtk_widget_set_sensitive(GTK_WIDGET(enableRtpFallback), TRUE);

    gtk_widget_show_all(sdesGrid);

    gtk_container_set_border_width(GTK_CONTAINER(sdesGrid), 10);

    if (gtk_dialog_run(GTK_DIALOG(securityDialog)) == GTK_RESPONSE_ACCEPT) {
        account_replace(account, CONFIG_SRTP_RTP_FALLBACK,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableRtpFallback)) ? "true": "false");
    }

    gtk_widget_destroy(GTK_WIDGET(securityDialog));
}
