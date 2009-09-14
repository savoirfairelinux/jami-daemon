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

#ifndef __ASSISTANT_H
#define __ASSISTANT_H


#include <accountlist.h>
#include <actions.h>
#include <sflphone_const.h>


#if GTK_CHECK_VERSION(2,10,0)

#define _SIP  0
#define _IAX  1

struct _wizard
{
  GtkWidget *window;
  GtkWidget *assistant;
  GdkPixbuf *logo;
  GtkWidget *intro;
  /** Page 1  - Protocol selection */
  GtkWidget *account_type;
  GtkWidget *protocols;
  GtkWidget *sip;
  GtkWidget *email;
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

  GtkWidget *sflphone_org;
  
}; 

/**
 * @file druid.h
 * @brief Implement the configuration wizard
 */

/**
 * Callbacks functions
 */
void set_account_type( GtkWidget* widget , gpointer data );

//static void cancel_callback( void );

//static void close_callback( void );

//static void sip_apply_callback( void );
//static void iax_apply_callback( void );

void enable_stun( GtkWidget *widget );

/**
 * Related-pages function
 */
void build_wizard();
GtkWidget* build_intro( void );
GtkWidget* build_select_account( void );
GtkWidget* build_sip_account_configuration( void );
GtkWidget* build_nat_settings( void );
GtkWidget* build_iax_account_configuration( void );
GtkWidget* build_summary( void );
GtkWidget* build_registration_error( void );
GtkWidget* build_email_configuration( void );
GtkWidget* build_sfl_or_account (void);

/**
 * Forward function
 */
//static gint forward_page_func( gint current_page , gpointer data );

/**
 * Page template
 */
//static GtkWidget* create_vbox(GtkAssistantPageType type, const gchar *title, const gchar *section);

#endif // GTK_CHECK_VERSION

#endif
