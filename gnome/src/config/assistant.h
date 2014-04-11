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

#ifndef __ASSISTANT_H
#define __ASSISTANT_H


#include "accountlist.h"
#include "actions.h"
#include "sflphone_const.h"


#define _SIP  0
#define _IAX  1

struct _wizard {
    GtkWidget *window;
    GtkWidget *assistant;
    GdkPixbuf *logo;
    GtkWidget *intro;
    /** Page 1  - Protocol selection */
    GtkWidget *account_type;
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
};

/**
 * Callbacks functions
 */
void set_account_type (GtkWidget* widget , gpointer data);

void enable_stun (GtkWidget *widget);

/**
 * Related-pages function
 */
void build_wizard();
GtkWidget* build_intro (void);
GtkWidget* build_select_account (void);
GtkWidget* build_sip_account_configuration (void);
GtkWidget* build_nat_settings (void);
GtkWidget* build_iax_account_configuration (void);
GtkWidget* build_summary (void);
GtkWidget* build_registration_error (void);
#endif
