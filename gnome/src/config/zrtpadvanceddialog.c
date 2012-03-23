/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include <gtk/gtk.h>
#include "str_utils.h"
#include "mainwindow.h"
#include "zrtpadvanceddialog.h"
#include "sflphone_const.h"
#include "utils.h"

void show_advanced_zrtp_options(account_t *account)
{
    gboolean curSasConfirm = TRUE;
    gboolean curHelloEnabled = TRUE;
    gboolean curZrtpNotSuppOther = TRUE;
    gboolean curDisplaySasOnce = FALSE;

    if (account != NULL) {
        curHelloEnabled = utf8_case_equal(account_lookup(account, ACCOUNT_ZRTP_HELLO_HASH), "true");
        curSasConfirm = utf8_case_equal(account_lookup(account, ACCOUNT_ZRTP_DISPLAY_SAS), "true");
        curZrtpNotSuppOther = utf8_case_equal(account_lookup(account, ACCOUNT_ZRTP_NOT_SUPP_WARNING), "true");
        curDisplaySasOnce = utf8_case_equal(account_lookup(account, ACCOUNT_DISPLAY_SAS_ONCE), "true");
    }

    GtkDialog *securityDialog = GTK_DIALOG(gtk_dialog_new_with_buttons(_("ZRTP Options"),
                                           GTK_WINDOW(get_main_window()),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_STOCK_CANCEL,
                                           GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_SAVE,
                                           GTK_RESPONSE_ACCEPT,
                                           NULL));
    gtk_window_set_resizable(GTK_WINDOW(securityDialog), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(securityDialog), 0);

    GtkWidget *tableZrtp = gtk_table_new(4, 2, FALSE /* homogeneous */);
    gtk_table_set_row_spacings(GTK_TABLE(tableZrtp), 10);
    gtk_table_set_col_spacings(GTK_TABLE(tableZrtp), 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(securityDialog)), tableZrtp, FALSE, FALSE, 0);
    gtk_widget_show(tableZrtp);

    GtkWidget *enableHelloHash = gtk_check_button_new_with_mnemonic(_("Send Hello Hash in S_DP"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableHelloHash), curHelloEnabled);
    gtk_table_attach(GTK_TABLE(tableZrtp), enableHelloHash, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(enableHelloHash), TRUE);

    GtkWidget *enableSASConfirm = gtk_check_button_new_with_mnemonic(_("Ask User to Confirm SAS"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableSASConfirm), curSasConfirm);
    gtk_table_attach(GTK_TABLE(tableZrtp), enableSASConfirm, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(enableSASConfirm), TRUE);

    GtkWidget *enableZrtpNotSuppOther = gtk_check_button_new_with_mnemonic(_("_Warn if ZRTP not supported"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableZrtpNotSuppOther), curZrtpNotSuppOther);
    gtk_table_attach(GTK_TABLE(tableZrtp), enableZrtpNotSuppOther, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(enableZrtpNotSuppOther), TRUE);

    GtkWidget *displaySasOnce = gtk_check_button_new_with_mnemonic(_("Display SAS once for hold events"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(displaySasOnce), curDisplaySasOnce);
    gtk_table_attach(GTK_TABLE(tableZrtp), displaySasOnce, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(displaySasOnce), TRUE);

    gtk_widget_show_all(tableZrtp);

    gtk_container_set_border_width(GTK_CONTAINER(tableZrtp), 10);

    if (gtk_dialog_run(GTK_DIALOG(securityDialog)) == GTK_RESPONSE_ACCEPT) {
        account_replace(account, ACCOUNT_ZRTP_DISPLAY_SAS,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableSASConfirm)) ? "true": "false");

        account_replace(account, ACCOUNT_DISPLAY_SAS_ONCE,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(displaySasOnce)) ? "true": "false");

        account_replace(account, ACCOUNT_ZRTP_HELLO_HASH,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableHelloHash)) ? "true": "false");

        account_replace(account, ACCOUNT_ZRTP_NOT_SUPP_WARNING,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableZrtpNotSuppOther)) ? "true": "false");
    }

    gtk_widget_destroy(GTK_WIDGET(securityDialog));
}


void show_advanced_sdes_options(account_t *account)
{
    gboolean rtpFallback = FALSE;

    if (account != NULL)
        rtpFallback = utf8_case_equal(account_lookup(account, ACCOUNT_SRTP_RTP_FALLBACK), "true");

    GtkDialog *securityDialog = GTK_DIALOG(gtk_dialog_new_with_buttons(_("SDES Options"),
                                           GTK_WINDOW(get_main_window()),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_STOCK_CANCEL,
                                           GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE,
                                           GTK_RESPONSE_ACCEPT, NULL));

    gtk_window_set_resizable(GTK_WINDOW(securityDialog), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(securityDialog), 0);

    GtkWidget *sdesTable = gtk_table_new(1, 2, FALSE /* homogeneous */);
    gtk_table_set_row_spacings(GTK_TABLE(sdesTable), 10);
    gtk_table_set_col_spacings(GTK_TABLE(sdesTable), 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(securityDialog)), sdesTable, FALSE, FALSE, 0);
    gtk_widget_show(sdesTable);

    GtkWidget *enableRtpFallback = gtk_check_button_new_with_mnemonic(_("Fallback on RTP on SDES failure"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableRtpFallback), rtpFallback);
    gtk_table_attach(GTK_TABLE(sdesTable), enableRtpFallback, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(enableRtpFallback), TRUE);

    gtk_widget_show_all(sdesTable);

    gtk_container_set_border_width(GTK_CONTAINER(sdesTable), 10);

    if (gtk_dialog_run(GTK_DIALOG(securityDialog)) == GTK_RESPONSE_ACCEPT) {
        account_replace(account, ACCOUNT_SRTP_RTP_FALLBACK,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableRtpFallback)) ? "true": "false");
    }

    gtk_widget_destroy(GTK_WIDGET(securityDialog));
}
