/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
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

#include <actions.h>
#include <calltree.h>
#include <dbus.h>
#include <mainwindow.h>
#include <menus.h>
#include <screen.h>
#include <statusicon.h>

#include <gtk/gtk.h>
#include <string.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define ALSA_ERROR_CAPTURE_DEVICE     0
#define ALSA_ERROR_PLAYBACK_DEVICE    1

#define TONE_WITHOUT_MESSAGE  0 
#define TONE_WITH_MESSAGE     1

guint voice_mails;

	void
sflphone_notify_voice_mail (guint count)
{
	voice_mails = count ;
	if(count > 0)
	{
		gchar * message = g_new0(gchar, 50);
		if( count > 1)
		  g_sprintf(message, _("%d new voice mails"), count);
		else
		  g_sprintf(message, _("%d new voice mail"), count);	  
		status_bar_message_add(message,  __MSG_VOICE_MAILS);
		g_free(message);
	}
}

void
status_bar_display_account( call_t* c)
{
    gchar* msg = malloc(100);
    account_t* acc;
    if(c->accountID != NULL)
      acc = account_list_get_by_id(c->accountID);
    else
      acc = account_list_get_by_id( account_list_get_default());
    sprintf( msg , "%s account: %s" , g_hash_table_lookup( acc->properties , ACCOUNT_TYPE)  , g_hash_table_lookup( acc->properties , ACCOUNT_ALIAS));
    status_bar_message_add( msg , __MSG_ACCOUNT_DEFAULT);
    g_free(msg);
}
  

	gboolean
sflphone_quit ()
{
	gboolean quit = FALSE;
	guint count = call_list_get_size();
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
	update_call_tree(c);
	update_menus();
}

	void 
sflphone_ringing(call_t * c )
{
	c->state = CALL_STATE_RINGING;
	update_call_tree(c);
	update_menus();
}

  void
sflphone_hung_up( call_t * c)
{
  call_list_remove( c->callID);
  update_call_tree_remove(c);
  update_menus();
}

/** Internal to actions: Fill account list */
	void
sflphone_fill_account_list()
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
		else
		{
			a->state = ACCOUNT_STATE_INVALID;
		}

	}

	toolbar_update_buttons();
	
}

	gboolean
sflphone_init()
{
	call_list_init ();
	account_list_init ();
        codec_list_init();
	if(!dbus_connect ())
	{
		main_window_error_message(_("Unable to connect to the SFLphone server.\nMake sure the daemon is running."));
		return FALSE;
	}
	else 
	{
		dbus_register(getpid(), "Gtk+ Client");
		sflphone_fill_account_list();
		sflphone_set_default_account();
		sflphone_fill_codec_list();
		return TRUE;
	}
}

	void 
sflphone_hang_up()
{
	call_t * selectedCall = call_get_selected();
	if(selectedCall)
	{
		switch(selectedCall->state)
		{
			case CALL_STATE_CURRENT:
			case CALL_STATE_HOLD:
			case CALL_STATE_DIALING:
			case CALL_STATE_RINGING:
			case CALL_STATE_BUSY:
			case CALL_STATE_FAILURE:
				dbus_hang_up (selectedCall);
				break;
			case CALL_STATE_INCOMING:  
				status_tray_icon_blink( FALSE );
				dbus_refuse (selectedCall);
				break;
			case CALL_STATE_TRANSFERT:  
				dbus_hang_up (selectedCall);
				break;
			default:
				g_warning("Should not happen in sflphone_hang_up()!");
				break;
		}
	}
}


	void 
sflphone_pick_up()
{
	call_t * selectedCall = call_get_selected();
	if(selectedCall)
	{
		switch(selectedCall->state)
		{
			case CALL_STATE_DIALING:
				sflphone_place_call (selectedCall);
				break;
			case CALL_STATE_INCOMING:
				status_tray_icon_blink( FALSE );
				dbus_accept (selectedCall);
				break;
			case CALL_STATE_HOLD:
				sflphone_new_call();
				break;
			case CALL_STATE_TRANSFERT:
				dbus_transfert (selectedCall);
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
	call_t * selectedCall = call_get_selected();
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
	call_t * selectedCall = call_get_selected();
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
	update_call_tree(c);
	update_menus();
}

	void 
sflphone_busy( call_t * c )
{
	c->state = CALL_STATE_BUSY;
	update_call_tree(c);
	update_menus();
}

	void 
sflphone_current( call_t * c )
{
	c->state = CALL_STATE_CURRENT;
	update_call_tree(c);
	update_menus();
}

	void 
sflphone_set_transfert()
{
	call_t * c = call_get_selected();
	if(c)
	{
		c->state = CALL_STATE_TRANSFERT;
		c->to = g_strdup("");
		update_call_tree(c);
		update_menus();
	}
	toolbar_update_buttons();
}

	void 
sflphone_unset_transfert()
{
	call_t * c = call_get_selected();
	if(c)
	{
		c->state = CALL_STATE_CURRENT;
		c->to = g_strdup("");
		update_call_tree(c);
		update_menus();
	}
	toolbar_update_buttons();
}
	void
sflphone_incoming_call (call_t * c) 
{
	call_list_add ( c );
	status_icon_unminimize();
	update_call_tree_add(c);
	update_menus();
}

void process_dialing(call_t * c, guint keyval, gchar * key)
{
	// We stop the tone
	dbus_start_tone( FALSE , 0 );
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
					update_call_tree(c);
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
				gchar * before = c->to;
				c->to = g_strconcat(c->to, key, NULL);
				g_free(before);
				g_print("TO: %s\n", c->to);

				if(c->state == CALL_STATE_DIALING)
				{
					g_free(c->from);
					c->from = g_strconcat("\"\" <", c->to, ">", NULL);
				}
				update_call_tree(c);
			}
			break;
	}

}


call_t * sflphone_new_call()
{
	if( call_list_get_size() == 0 )
	{
	  dbus_start_tone( TRUE , ( voice_mails > 0 )? TONE_WITH_MESSAGE : TONE_WITHOUT_MESSAGE) ;
	}
	call_t * c = g_new0 (call_t, 1);
	c->state = CALL_STATE_DIALING;
	c->from = g_strconcat("\"\" <>", NULL);

	c->callID = g_new0(gchar, 30);
	g_sprintf(c->callID, "%d", rand()); 

	c->to = g_strdup("");

	call_list_add(c);
	update_call_tree_add(c);  
	update_menus();

	return c;
}

	void 
sflphone_keypad( guint keyval, gchar * key)
{
	call_t * c = call_get_selected();
	if(c)
	{

		switch(c->state) 
		{
			case CALL_STATE_DIALING: // Currently dialing => edit number
				dbus_play_dtmf(key);
				process_dialing(c, keyval, key);
				break;
			case CALL_STATE_CURRENT:
				switch (keyval)
				{
					case 65307: /* ESCAPE */
						dbus_hang_up(c);
						break;
					default:  // TODO should this be here?
						dbus_play_dtmf(key);
						if (keyval < 255 || (keyval >65453 && keyval < 65466))
						{ 
							gchar * temp = g_strconcat(call_get_number(c), key, NULL);
							gchar * before = c->from;
							c->from = g_strconcat("\"",call_get_name(c) ,"\" <", temp, ">", NULL);
							g_free(before);
							g_free(temp);
							//update_call_tree(c);
						}
						break;
				}
				break;
			case CALL_STATE_INCOMING:
				switch (keyval)
				{
					case 65293: /* ENTER */
					case 65421: /* ENTER numpad */
						status_tray_icon_blink( FALSE );
						status_bar_display_account(c);
						dbus_accept(c);
						break;
					case 65307: /* ESCAPE */
						status_tray_icon_blink( FALSE );
						dbus_refuse(c);
						break;
				}
				break;
			case CALL_STATE_TRANSFERT:
				switch (keyval)
				{
					case 65293: /* ENTER */
					case 65421: /* ENTER numpad */
						dbus_transfert(c);
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
						break;
				}
				break;
			default:
				break;
		} 
	}
	else 
	{ // Not in a call, not dialing, create a new call 
		dbus_play_dtmf(key);
		switch (keyval)
		{
			case 65293: /* ENTER */
			case 65421: /* ENTER numpad */
			case 65307: /* ESCAPE */
				break;
			default:
				process_dialing(sflphone_new_call(), keyval, key);
				break;
		}


	}
} 

/*
 * Place a call with the default account.
 * If there is no default account selected, place a call with the first 
 * registered account of the account list
 * Else, popup an error message
 */
void 
sflphone_place_call ( call_t * c )
{
	status_bar_display_account(c);
	if(c->state == CALL_STATE_DIALING)
	{
		account_t * account;
		gchar * default_account =  account_list_get_default();
		account = account_list_get_by_id(default_account);
		
		if(account)
		{
			if(strcmp(g_hash_table_lookup(account->properties, "Status"),"REGISTERED")==0)
			{
				c->accountID = default_account;
				dbus_place_call(c);
			}
			else
			{
				main_window_error_message(_("The account selected as default is not registered."));
			}
			
		}
		else{
			account = account_list_get_by_state (ACCOUNT_STATE_REGISTERED);
			if(account)
			{
				c->accountID = account->accountID;
				dbus_place_call(c);
			}
			else
			{
				main_window_error_message(_("There is no registered account to make this call with."));
			}

		}
	}
}

/* Internal to action - set the DEFAULT_ACCOUNT variable */
	void
sflphone_set_default_account( )
{
	gchar* default_id = strdup(dbus_get_default_account());
	account_list_set_default(default_id);	
}

void
sflphone_throw_exception( int errCode )
{
  gchar* markup = malloc(1000);
  switch( errCode ){
    case ALSA_ERROR_PLAYBACK_DEVICE:
      sprintf( markup , _("<b>ALSA notification</b>\n\nError while opening playback device"));
      break;
    case ALSA_ERROR_CAPTURE_DEVICE:
      sprintf( markup , _("<b>ALSA notification</b>\n\nError while opening capture device"));
      break;
  }
  main_window_error_message( markup );  
  free( markup );
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
    gchar* markup = malloc(1000);
    sprintf(markup , _("<b>Error: No audio codecs found.\n\n</b> SFL audio codecs have to be placed in <i>%s</i> or in the <b>.sflphone</b> directory in your home( <i>%s</i> )"), CODECS_DIR , g_get_home_dir());
    main_window_error_message( markup );
    g_free( markup );
    dbus_unregister(getpid());
    exit(0);
  }
}
