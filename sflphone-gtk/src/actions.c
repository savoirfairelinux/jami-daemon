/*
 *  Copyright (C) 2007 - 2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#include <actions.h>
#include <calltree.h>
#include <dbus.h>
#include <mainwindow.h>
#include <menus.h>
#include <statusicon.h>
#include <calltab.h>
#include <historyfilter.h>

#include <gtk/gtk.h>
#include <string.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

guint voice_mails;

void
sflphone_notify_voice_mail ( const gchar* accountID , guint count )
{
	voice_mails = count ;
	gchar* id = g_strdup( accountID );
	if(count > 0)
	{
		gchar * message = g_new0(gchar, 50);
		if( count > 1)
		  g_sprintf(message, _("%d voice mails"), count);
		else
		  g_sprintf(message, _("%d voice mail"), count);	  
		statusbar_push_message(message,  __MSG_VOICE_MAILS);
		g_free(message);
	}
	// TODO: add ifdef
	if( account_list_get_size() > 0 )
	{
	  account_t* acc = account_list_get_by_id( id );
	  if( acc != NULL )
	      notify_voice_mails( count , acc );	
	}
}

void
status_bar_display_account( call_t* c)
{
    gchar* msg;
    account_t* acc;
    if(c->accountID != NULL){
      acc = account_list_get_by_id(c->accountID);
      msg = g_markup_printf_escaped(_("%s account- %s") , 
				  (gchar*)g_hash_table_lookup( acc->properties , ACCOUNT_TYPE), 
				  (gchar*)g_hash_table_lookup( acc->properties , ACCOUNT_ALIAS));
      statusbar_push_message( msg , __MSG_ACCOUNT_DEFAULT);
      g_free(msg);
  }
}
  

gboolean
sflphone_quit ()
{
	gboolean quit = FALSE;
	guint count = call_list_get_size(current_calls);
	if(count > 0){
		quit = main_window_ask_quit();
	}
	else{
		quit = TRUE;
	}

	if (quit)
	{
		dbus_unregister(getpid());
		dbus_clean ();
		//call_list_clean(); TODO
		//account_list_clean()
		gtk_main_quit ();
	}
	return quit;
}

void 
sflphone_hold(call_t * c )
{
	c->state = CALL_STATE_HOLD;
	update_call_tree(current_calls,c);
	update_menus();
}

void 
sflphone_ringing(call_t * c )
{
	c->state = CALL_STATE_RINGING;
	update_call_tree(current_calls,c);
	update_menus();
}

void
sflphone_hung_up( call_t * c)
{
  call_list_remove( current_calls, c->callID);
  update_call_tree_remove(current_calls, c);
  c->state = CALL_STATE_DIALING;
  update_menus();
#if GTK_CHECK_VERSION(2,10,0)
  status_tray_icon_blink( FALSE );
#endif
}

/** Internal to actions: Fill account list */
	void
sflphone_fill_account_list(gboolean toolbarInitialized)
{
	account_list_clear ( );

	gchar ** array = (gchar **)dbus_account_list();
	gchar ** accountID;
	for (accountID = array; *accountID; accountID++)
	{
		account_t * a = g_new0(account_t,1);
		a->accountID = g_strdup(*accountID);
		account_list_add(a);
	}
	g_strfreev (array);

	int i;
	for( i = 0; i < account_list_get_size(); i++)
	{
		account_t  * a = account_list_get_nth (i);
		GHashTable * details = (GHashTable *) dbus_account_details(a->accountID);
		a->properties = details;

		gchar * status = g_hash_table_lookup(details, "Status");
		if(strcmp(status, "REGISTERED") == 0)
		{
			a->state = ACCOUNT_STATE_REGISTERED;
		}
		else if(strcmp(status, "UNREGISTERED") == 0)
		{
			a->state = ACCOUNT_STATE_UNREGISTERED;
		}
		else if(strcmp(status, "TRYING") == 0)
		{
			a->state = ACCOUNT_STATE_TRYING;
		}
		else if(strcmp(status, "ERROR") == 0)
		{
			a->state = ACCOUNT_STATE_ERROR;
		}
		else if(strcmp( status , "ERROR_AUTH") == 0 )
		{
		  a->state = ACCOUNT_STATE_ERROR_AUTH;
		}
		else if(strcmp( status , "ERROR_NETWORK") == 0 )
		{
		  a->state = ACCOUNT_STATE_ERROR_NETWORK;
		}
		else if(strcmp( status , "ERROR_HOST") == 0 )
		{
		  a->state = ACCOUNT_STATE_ERROR_HOST;
		}
		else
		{
			a->state = ACCOUNT_STATE_INVALID;
		}

	}

	// Prevent update being called when toolbar is not yet initialized
	if(toolbarInitialized)
		toolbar_update_buttons();
}

gboolean
sflphone_init()
{
	int i;
	current_calls = calltab_init();
	history = calltab_init();	
	if(SHOW_SEARCHBAR)  histfilter = create_filter(GTK_TREE_MODEL(history->store));
	account_list_init ();
        codec_list_init();
	if(!dbus_connect ()){
	
		main_window_error_message(_("Unable to connect to the SFLphone server.\nMake sure the daemon is running."));
		return FALSE;
	}
	else 
	{
		dbus_register(getpid(), "Gtk+ Client");
		sflphone_fill_account_list(FALSE);
		sflphone_fill_codec_list();
		sflphone_set_current_account();
		return TRUE;
	}
}

void 
sflphone_hang_up()
{
	call_t * selectedCall = call_get_selected(current_calls);
	if(selectedCall)
	{
		switch(selectedCall->state)
		{
			case CALL_STATE_DIALING:
				dbus_hang_up (selectedCall);
				break;
			case CALL_STATE_RINGING:
				dbus_hang_up (selectedCall);
				selectedCall->state = CALL_STATE_DIALING;
				selectedCall->_stop = 0;
				break;
			case CALL_STATE_CURRENT:
			case CALL_STATE_HOLD:
			case CALL_STATE_BUSY:
			case CALL_STATE_FAILURE:
				dbus_hang_up (selectedCall);
				selectedCall->state = CALL_STATE_DIALING;
				(void) time(&selectedCall->_stop);
				break;
			case CALL_STATE_INCOMING:  
				dbus_refuse (selectedCall);
				selectedCall->state = CALL_STATE_DIALING;
				selectedCall->_stop = 0;
				g_print("from sflphone_hang_up : "); stop_notification();
				break;
			case CALL_STATE_TRANSFERT:  
				dbus_hang_up (selectedCall);
				(void) time(&selectedCall->_stop);
				break;
			default:
				g_warning("Should not happen in sflphone_hang_up()!");
				break;
		}
	}
	update_call_tree( history , selectedCall );
}


void 
sflphone_pick_up()
{
	call_t * selectedCall = call_get_selected(active_calltree);
	if(selectedCall)
	{
		switch(selectedCall->state)
		{
			case CALL_STATE_DIALING:
				sflphone_place_call (selectedCall);
				break;
			case CALL_STATE_INCOMING:
				selectedCall->history_state = INCOMING;
				update_call_tree( history , selectedCall );
				dbus_accept (selectedCall);
				g_print("from sflphone_pick_up : "); stop_notification();
				break;
			case CALL_STATE_HOLD:
				sflphone_new_call();
				break;
			case CALL_STATE_TRANSFERT:
				dbus_transfert (selectedCall);
				(void) time(&selectedCall->_stop);
				break;
			case CALL_STATE_CURRENT:
				sflphone_new_call();
				break;
			case CALL_STATE_RINGING:
				sflphone_new_call();
				break;
			default:
				g_warning("Should not happen in sflphone_pick_up()!");
				break;
		}
	}
}

void 
sflphone_on_hold ()
{
	call_t * selectedCall = call_get_selected(current_calls);
	if(selectedCall)
	{
		switch(selectedCall->state)
		{
			case CALL_STATE_CURRENT:
				dbus_hold (selectedCall);
				break;
			default:
				g_warning("Should not happen in sflphone_on_hold!");
				break;
		}
	}
}

void 
sflphone_off_hold ()
{
	call_t * selectedCall = call_get_selected(current_calls);
	if(selectedCall)
	{
		switch(selectedCall->state)
		{
			case CALL_STATE_HOLD:
				dbus_unhold (selectedCall);
				break;
			default:
				g_warning("Should not happen in sflphone_off_hold ()!");
				break;
		}
	}
}


void 
sflphone_fail( call_t * c )
{
	c->state = CALL_STATE_FAILURE;
	update_call_tree(current_calls,c);
	update_menus();
}

void 
sflphone_busy( call_t * c )
{
	c->state = CALL_STATE_BUSY;
	update_call_tree(current_calls, c);
	update_menus();
}

void 
sflphone_current( call_t * c )
{
  if( c->state != CALL_STATE_HOLD )
	(void) time(&c->_start);
  c->state = CALL_STATE_CURRENT;
  update_call_tree(current_calls,c);
  update_menus();
}

void 
sflphone_set_transfert()
{
	call_t * c = call_get_selected(current_calls);
	if(c)
	{
		c->state = CALL_STATE_TRANSFERT;
		c->to = g_strdup("");
		update_call_tree(current_calls,c);
		update_menus();
	}
	toolbar_update_buttons();
}

void 
sflphone_unset_transfert()
{
	call_t * c = call_get_selected(current_calls);
	if(c)
	{
		c->state = CALL_STATE_CURRENT;
		c->to = g_strdup("");
		update_call_tree(current_calls,c);
		update_menus();
	}
	toolbar_update_buttons();
}

void
sflphone_incoming_call (call_t * c) 
{
	c->history_state = MISSED;
	call_list_add ( current_calls, c );
	call_list_add( history, c );
	update_call_tree_add( current_calls , c );
	update_menus();
	if( active_calltree == history )  switch_tab();
}

void
process_dialing(call_t * c, guint keyval, gchar * key)
{
	// We stop the tone
	if(strlen(c->to) == 0 && c->state != CALL_STATE_TRANSFERT){
	  dbus_start_tone( FALSE , 0 );
	  //dbus_play_dtmf( key );
	}
	switch (keyval)
	{
		case 65293: /* ENTER */
		case 65421: /* ENTER numpad */
			sflphone_place_call(c);
			break;
		case 65307: /* ESCAPE */
			sflphone_hang_up(c);
			break;
		case 65288: /* BACKSPACE */
			{  /* Brackets mandatory because of local vars */
				gchar * before = c->to;
				if(strlen(c->to) >= 1){
					c->to = g_strndup(c->to, strlen(c->to) -1);
					g_free(before);
					g_print("TO: %s\n", c->to);

					if(c->state == CALL_STATE_DIALING)
					{
						g_free(c->from);
						c->from = g_strconcat("\"\" <", c->to, ">", NULL);
					}
					update_call_tree(current_calls,c);
				} 
				else if(strlen(c->to) == 0)
				{
				  if(c->state != CALL_STATE_TRANSFERT)
					dbus_hang_up(c);
				}
			}
			break;
		case 65289: /* TAB */
		case 65513: /* ALT */
		case 65507: /* CTRL */
		case 65515: /* SUPER */
		case 65509: /* CAPS */
			break;
		default:
			if (keyval < 255 || (keyval >65453 && keyval < 65466))
			{ 
				if(c->state != CALL_STATE_TRANSFERT)
				  dbus_play_dtmf( key );
				  gchar * before = c->to;
				  c->to = g_strconcat(c->to, key, NULL);
				  g_free(before);
				  g_print("TO: %s\n", c->to);

				if(c->state == CALL_STATE_DIALING)
				{
					g_free(c->from);
					c->from = g_strconcat("\"\" <", c->to, ">", NULL);
				}
				update_call_tree(current_calls,c);
			}
			break;
	}

}


call_t * 
sflphone_new_call()
{
	// Play a tone when creating a new call
	if( call_list_get_size(current_calls) == 0 )
	  dbus_start_tone( TRUE , ( voice_mails > 0 )? TONE_WITH_MESSAGE : TONE_WITHOUT_MESSAGE) ;

	call_t * c = g_new0 (call_t, 1);
	c->state = CALL_STATE_DIALING;
	c->from = g_strconcat("\"\" <>", NULL);

	c->callID = g_new0(gchar, 30);
	g_sprintf(c->callID, "%d", rand()); 

	c->to = g_strdup("");

	c->_start = 0;
	c->_stop = 0;

	call_list_add(current_calls,c);
	update_call_tree_add(current_calls,c);  
	update_menus();

	return c;
}


void 
sflphone_keypad( guint keyval, gchar * key)
{

	call_t * c = call_get_selected(current_calls);
	if(c)
	{
		switch(c->state) 
		{
			case CALL_STATE_DIALING: // Currently dialing => edit number
				process_dialing(c, keyval, key);
				break;
			case CALL_STATE_CURRENT:
				switch (keyval)
				{
					case 65307: /* ESCAPE */
						dbus_hang_up(c);
						(void) time(&c->_stop);
						update_call_tree( history , c );
						break;
					default:  
						// To play the dtmf when calling mail box for instance
						dbus_play_dtmf(key);
						if (keyval < 255 || (keyval >65453 && keyval < 65466))
						{ 
							//gchar * temp = g_strconcat(call_get_number(c), key, NULL);
							//gchar * before = c->from;
							//c->from = g_strconcat("\"",call_get_name(c) ,"\" <", temp, ">", NULL);
							//g_free(before);
							//g_free(temp);
							//update_call_tree(current_calls,c);
						}
						break;
				}
				break;
			case CALL_STATE_INCOMING:
				switch (keyval)
				{
					case 65293: /* ENTER */
					case 65421: /* ENTER numpad */
						c->history_state = INCOMING;
						update_call_tree( history , c );
						dbus_accept(c);
						g_print("from sflphone_keypad ( enter ) : "); stop_notification();
						break;
					case 65307: /* ESCAPE */
						dbus_refuse(c);
						g_print("from sflphone_keypad ( escape ) : "); stop_notification();
						break;
				}
				break;
			case CALL_STATE_TRANSFERT:
				switch (keyval)
				{
					case 65293: /* ENTER */
					case 65421: /* ENTER numpad */
						dbus_transfert(c);
						(void) time(&c->_stop);
						break;
					case 65307: /* ESCAPE */
						sflphone_unset_transfert(c); 
						break;
					default: // When a call is on transfert, typing new numbers will add it to c->to
						process_dialing(c, keyval, key);
						break;
				}
				break;
			case CALL_STATE_HOLD:
				switch (keyval)
				{
					case 65293: /* ENTER */
					case 65421: /* ENTER numpad */
						dbus_unhold(c);
						break;
					case 65307: /* ESCAPE */
						dbus_hang_up(c);
						break;
					default: // When a call is on hold, typing new numbers will create a new call
						process_dialing(sflphone_new_call(), keyval, key);
						break;
				}
				break;
			case CALL_STATE_RINGING:
			case CALL_STATE_BUSY:
			case CALL_STATE_FAILURE:
				switch (keyval)
				{
					case 65307: /* ESCAPE */
					  dbus_hang_up(c);
					  c->_stop = 0;
					  update_call_tree( history , c );
					  break;
				}
				break;
			default:
				break;
		} 
	}
	else 
	{ // Not in a call, not dialing, create a new call 
		//dbus_play_dtmf(key);
		switch (keyval)
		{
			case 65293: /* ENTER */
			case 65421: /* ENTER numpad */
			case 65307: /* ESCAPE */
				break;
			default:
				if( active_calltree == history )
				  switch_tab();
				process_dialing(sflphone_new_call(), keyval, key);
				break;
		}


	}
 } 

/*
 * Place a call with the current account.
 * If there is no default account selected, place a call with the first 
 * registered account of the account list
 * Else, popup an error message
 */
void 
sflphone_place_call ( call_t * c )
{
  if(c->state == CALL_STATE_DIALING)
  {
    if( account_list_get_size() == 0 )
    {
      notify_no_accounts();
      sflphone_fail(c);
    }
    else if( account_list_get_by_state( ACCOUNT_STATE_REGISTERED ) == NULL )
    {
      notify_no_registered_accounts();
      sflphone_fail(c);
    }
    else
    {
      account_t * current = account_list_get_current();
      if( current )
      {
	if(g_strcasecmp(g_hash_table_lookup( current->properties, "Status"),"REGISTERED")==0)
	{ 
	  // OK, everything alright - the call is made with the current account
	  c -> accountID = current -> accountID;
	  status_bar_display_account(c);
	  dbus_place_call(c);
	}
	else
	{
	  // Current account is not registered 
	  // So we place a call with the first registered account
	  // And we switch the current account
	  current = account_list_get_by_state( ACCOUNT_STATE_REGISTERED );
	  c -> accountID = current -> accountID;
	  dbus_place_call(c);
	  notify_current_account( current );
	  status_bar_display_account(c);
	  account_list_set_current_id( c-> accountID );
	}
      }
      else
      {
	// No current accounts have been setup. 
	// So we place a call with the first registered account
	// and we change the current account
	current = account_list_get_by_state( ACCOUNT_STATE_REGISTERED );
	c -> accountID = current -> accountID;
	dbus_place_call(c);
	notify_current_account( current );
	status_bar_display_account(c);
	account_list_set_current_id( c-> accountID );
      }
    }
	// Update history
	c->history_state = OUTGOING;
	call_list_add(history, c);
  }
}

/* Internal to action - set the __CURRENT_ACCOUNT variable */
void
sflphone_set_current_account()
{
  if( account_list_get_size() > 0 )
    account_list_set_current_pos( 0 );	
}


/* Internal to action - get the codec list */
void	
sflphone_fill_codec_list()
{

  codec_list_clear();
    
  gchar** codecs = (gchar**)dbus_codec_list();
  gchar** order = (gchar**)dbus_get_active_codec_list();
  gchar** details;
  gchar** pl;

  for(pl=order; *order; order++)
  {
    codec_t * c = g_new0(codec_t, 1);
    c->_payload = atoi(*order);
    details = (gchar **)dbus_codec_details(c->_payload);
    //printf("Codec details: %s / %s / %s / %s\n",details[0],details[1],details[2],details[3]);
    c->name = details[0];
    c->is_active = TRUE;
    c->sample_rate = atoi(details[1]);
    c->_bitrate = atof(details[2]);
    c->_bandwidth = atof(details[3]);
    codec_list_add(c);
  }
 
  for(pl=codecs; *codecs; codecs++)
  {
    details = (gchar **)dbus_codec_details(atoi(*codecs));
    if(codec_list_get(details[0])!=NULL){
      // does nothing - the codec is already in the list, so is active.
    }
    else{
      codec_t* c = g_new0(codec_t, 1);
      c->_payload = atoi(*codecs);
      c->name = details[0];
      c->is_active = FALSE;
      c->sample_rate = atoi(details[1]);
      c->_bitrate = atof(details[2]);
      c->_bandwidth = atof(details[3]);
      codec_list_add(c);
    }
  }
  if( codec_list_get_size() == 0) {
    
    gchar* markup = g_markup_printf_escaped(_("<b>Error: No audio codecs found.\n\n</b> SFL audio codecs have to be placed in <i>%s</i> or in the <b>.sflphone</b> directory in your home( <i>%s</i> )") , CODECS_DIR , g_get_home_dir());
    main_window_error_message( markup );
    dbus_unregister(getpid());
    exit(0);
  }
}
