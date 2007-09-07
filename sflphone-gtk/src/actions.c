/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
#include <accountlist.h>
#include <gtk/gtk.h>

/**
 * Terminate the gtk program
 */
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
    dbus_clean ();
    //call_list_clean(); TODO
    gtk_main_quit ();
  }
  return quit;
}

/**
 * Put the line on hold
 */
void 
sflphone_hold(call_t * c )
{
  c->state = CALL_STATE_HOLD;
  update_call_tree();
  screen_clear();
}



/* Fill account list */
void
sflphone_fill_account_list()
{
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
    else
    {
      a->state = ACCOUNT_STATE_INVALID;
    }
    
  }
  
}

gboolean
sflphone_init()
{
  call_list_init ();
  account_list_init ();
  if(!dbus_connect ())
  {
    main_window_error_message("Unable to connect to the SFLPhone server.\nMake sure the daemon is running.");
    return FALSE;
  }
  else 
  {
    sflphone_fill_account_list();
    return TRUE;
  }
}

/**
 * Put the line on hold
 */
void 
sflphone_unhold(call_t * c )
{
  c->state = CALL_STATE_CURRENT;
  update_call_tree();
  screen_set_call(c);
}

/**
 * Hang up the line
 */
void 
sflphone_hang_up( call_t  * c )
{
  call_list_remove(c->callID);
  update_call_tree();
  screen_clear();
}

/**
 * Incoming call
 */
void 
sflphone_current( call_t * c )
{
  c->state = CALL_STATE_CURRENT;
  update_call_tree();
  screen_set_call(c);
}

/**
 * Transfert the line
 */
void 
sflphone_transfert( call_t * c, gchar * to )
{
  screen_clear();
  update_call_tree();
}

/**
 * Signal Incoming Call
 */
void
sflphone_incoming_call (call_t * c) 
{
  call_list_add ( c );
  update_call_tree();
}
/**
 * Signal Hung up
 */
void 
sflphone_hung_up (call_t * c )
{
  call_list_remove(c->callID);
  update_call_tree();
  screen_clear();
}

void 
sflphone_keypad( guint keyval, gchar * key)
{
  call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_DIALING);
  if(c) // Currently dialing => edit number
  {
    switch (keyval)
    {
    case 65293: /* ENTER */
      sflphone_place_call(c);
      break;
    case 65307: /* ESCAPE */
      sflphone_hang_up(c);
      break;
    case 65288: /* BACKSPACE */
    case 65289: /* TAB */
    case 65513: /* ALT */
    case 65507: /* CTRL */
    case 65515: /* SUPER */
    case 65509: /* CAPS */
    case 65505: /* SHIFT */
      break;
    default:
      if (keyval < 255 || (keyval >65453 && keyval < 65466))
      {  /* Brackets mandatory because of local vars */
        gchar * before = c->to;
        c->to = g_strconcat(c->to, key, NULL);
        g_free(before);
        g_print("TO: %s\n", c->to);
        
        g_free(c->from);
        c->from = g_strconcat("\"\" <", c->to, ">", NULL);
        screen_set_call(c);
      }
      break;
    }
    

  }
  else 
  {
    call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_CURRENT);
    if(c) // Currently in a call => send number to server
    {
      switch (keyval)
      {
      case 65307: /* ESCAPE */
        sflphone_hang_up(c);
        break;
      }
    } 
    else // Not in a call, not dialing, create a new call 
    {
      if (keyval < 255 || (keyval >65453 && keyval < 65466))
      { 
        /* Brackets mandatory because of local vars */
        call_t * c = g_new0 (call_t, 1);
        c->state = CALL_STATE_DIALING;
        c->from = g_strconcat("\"\" <", key, ">", NULL);
        c->callID = "asdf"; // TODO generate a unique number
        c->to = g_strdup(key);
        call_list_add(c);
        screen_set_call(c);
             
      }
    }
  }
} 

void 
sflphone_place_call ( call_t * c )
{
  if(c->state == CALL_STATE_DIALING)
  {
    account_t * a = account_list_get_by_state (ACCOUNT_STATE_REGISTERED);
    if(a)
    {
      c->accountID = a->accountID;
      dbus_place_call(c);
    }
    else
    {
      main_window_error_message("There are no registered accounts to make this call with.");
    }
    
  }
}

void 
sflphone_remove_account ( account_t * a )
{
  dbus_remove_account (a);
}


