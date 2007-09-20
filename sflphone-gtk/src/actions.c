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

#include <gtk/gtk.h>
#include <string.h>
#include <glib/gprintf.h>
#include <stdlib.h>

#include <actions.h>
#include <mainwindow.h>
#include <calltree.h>
#include <screen.h>
#include <dbus.h>
#include <accountlist.h>


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


void 
sflphone_hold(call_t * c )
{
  c->state = CALL_STATE_HOLD;
  update_call_tree(c);
  screen_clear();
}

void 
sflphone_ringing(call_t * c )
{
  c->state = CALL_STATE_RINGING;
  update_call_tree(c);
}


/** Internal to actions: Fill account list */
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
    main_window_error_message("Unable to connect to the SFLphone server.\nMake sure the daemon is running.");
    return FALSE;
  }
  else 
  {
    sflphone_fill_account_list();
    return TRUE;
  }
}

void 
sflphone_hang_up( call_t  * c )
{
  call_list_remove(c->callID);
  update_call_tree_remove(c);
  screen_clear();
}

void 
sflphone_current( call_t * c )
{
  c->state = CALL_STATE_CURRENT;
  update_call_tree(c);
  screen_set_call(c);
}

void 
sflphone_transfert( call_t * c, gchar * to )
{
  screen_clear();
  update_call_tree_remove(c);
}

void
sflphone_incoming_call (call_t * c) 
{
  call_list_add ( c );
  update_call_tree_add(c);
}

void 
sflphone_hung_up (call_t * c )
{
  call_list_remove(c->callID);
  update_call_tree_remove(c);
  screen_clear();
}

void process_dialing(call_t * c, guint keyval, gchar * key)
{
  switch (keyval)
  {
  case 65293: /* ENTER */
  case 65421: /* ENTER numpad */
    sflphone_place_call(c);
    break;
  case 65307: /* ESCAPE */
    dbus_hang_up(c);
    break;
  case 65288: /* BACKSPACE */
    {  /* Brackets mandatory because of local vars */
      gchar * before = c->to;
      if(strlen(c->to) > 1){
        c->to = g_strndup(c->to, strlen(c->to) -1);
        g_free(before);
        g_print("TO: %s\n", c->to);
      
        g_free(c->from);
        c->from = g_strconcat("\"\" <", c->to, ">", NULL);
        screen_set_call(c);
        update_call_tree(c);
      } 
      else if(strlen(c->to) == 1)
      {
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
      
      g_free(c->from);
      c->from = g_strconcat("\"\" <", c->to, ">", NULL);
      screen_set_call(c);
      update_call_tree(c);
    }
    break;
  }
  
}

void process_new_call(guint keyval, gchar * key){
  if (keyval < 255 || (keyval >65453 && keyval < 65466))
    { 
      /* Brackets mandatory because of local vars */
      call_t * c = g_new0 (call_t, 1);
      c->state = CALL_STATE_DIALING;
      c->from = g_strconcat("\"\" <", key, ">", NULL);
      
      c->callID = g_new0(gchar, 100);
      g_sprintf(c->callID, "%d", rand()); 
      
      c->to = g_strdup(key);
      call_list_add(c);
      screen_set_call(c);
      update_call_tree_add(c);
    }
}

void 
sflphone_keypad( guint keyval, gchar * key)
{
  call_t * c = call_get_selected();
  if(c)
  {
  
    switch(c->state) // Currently dialing => edit number
    {
      case CALL_STATE_DIALING:
        process_dialing(c, keyval, key);
        break;
      case CALL_STATE_CURRENT:
      case CALL_STATE_RINGING:
        switch (keyval)
        {
        case 65307: /* ESCAPE */
          dbus_hang_up(c);
          break;
        default:
          dbus_play_dtmf(key);
          if (keyval < 255 || (keyval >65453 && keyval < 65466))
          { 
            gchar * temp = g_strconcat(call_get_number(c), key, NULL);
            gchar * before = c->from;
            c->from = g_strconcat("\"",call_get_name(c) ,"\" <", temp, ">", NULL);
            g_free(before);
            g_free(temp);
            screen_set_call(c);
            update_call_tree(c);
          }
          break;
        }
        break;
      case CALL_STATE_INCOMING:
        switch (keyval)
        {
        case 65293: /* ENTER */
        case 65421: /* ENTER numpad */
          dbus_accept(c);
          break;
        case 65307: /* ESCAPE */
          dbus_refuse(c);
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
          process_new_call(keyval, key);
          break;
        }
        break;
      default:
        break;
     } 
  }
  else 
  { // Not in a call, not dialing, create a new call 
    process_new_call(keyval, key);
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


