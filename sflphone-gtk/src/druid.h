/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __DRUID_H
#define __DRUID_H

#include <libgnomeui/libgnomeui.h>
#include <accountlist.h>
#include <sflphone_const.h>

#define _SIP  0
#define _IAX  1

struct _wizard
{
  GtkWidget *window;
  GtkWidget *druid;
  GdkPixbuf *logo;
  GtkWidget *first_page;
  /** Page 1  - Protocol selection */
  GtkWidget *account_type;
  GtkWidget *protocols;
  GtkWidget *sip;
  GtkWidget *iax;
  /** Page 2 - SIP account creation */
  GtkWidget *sip_account;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *alias;
  GtkWidget *name;
  GtkWidget *userpart;
  GtkWidget *server;
  GtkWidget *username;
  GtkWidget *password;
  GtkWidget *test;
  GtkWidget *state;
  GtkWidget *mailbox;
  /** Page 3 - IAX account creation */
  GtkWidget *iax_account;
  /** Page 4 - Nat detection */
  GtkWidget *nat;
  GtkWidget *enable;
  GtkWidget *addr;
  /** Page 5 - Test registration */
  GtkWidget *page_end;
  
}; 

/**
 * @file druid.h
 * @brief Implement the configuration wizard
 */
void set_account_type( GtkWidget* widget , gpointer data );
void enable_stun( GtkWidget *widget );
void goto_right_account( void );
void goto_accounts_page( void );
void goto_nat_page( void );
void goto_end_page( void );
void goto_sip_account_page( void );
void quit_wizard( void );
void update_account_parameters( int type );
void build_nat_window( void );
void build_configuration_account( int type );
void build_wizard();


#endif
