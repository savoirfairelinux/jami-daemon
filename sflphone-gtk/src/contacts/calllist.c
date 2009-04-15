/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
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

#include <calllist.h>
#include <calltree.h>
#include <contacts/searchbar.h>

// TODO : sflphoneGTK : try to do this more generic
void calllist_add_contact (gchar *contact_name, gchar *contact_phone, contact_type_t type, GdkPixbuf *photo){

    gchar *from;
    call_t *new_call;
    GdkPixbuf *pixbuf;

    /* Check if the information is valid */
    if (g_strcasecmp (contact_phone, EMPTY_ENTRY) != 0){
        from = g_strconcat("\"" , contact_name, "\"<", contact_phone, ">", NULL);
        create_new_call (from, from, CALL_STATE_DIALING, "", &new_call);

        // Attach a pixbuf to a contact
        if (photo) {
            attach_thumbnail (new_call, gdk_pixbuf_copy(photo));
        }
        else {
            switch (type) {
                case CONTACT_PHONE_BUSINESS:
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/face-monkey.svg", NULL);
                    break;
                case CONTACT_PHONE_HOME:
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/home.svg", NULL);
                    break;
                case CONTACT_PHONE_MOBILE:
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/users.svg", NULL);
                    break;
                default:
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/contact_default.svg", NULL);
                    break;
            }
            attach_thumbnail (new_call, pixbuf);
        }

        calllist_add (contacts, new_call);
        calltree_add_call(contacts, new_call);
    }
}

void
calllist_init (calltab_t* tab)
{
  tab->callQueue = g_queue_new ();
  tab->selectedCall = NULL;
}

void
calllist_clean (calltab_t* tab)
{
  g_queue_free (tab->callQueue);
}

void
calllist_reset (calltab_t* tab)
{
  g_queue_free (tab->callQueue);
  tab->callQueue = g_queue_new();
}

void
calllist_add (calltab_t* tab, call_t * c)
{
  if( tab == history )
  {
    // First case: can still add calls to the list
    if( calllist_get_size(tab) < dbus_get_max_calls() )
    {
      g_queue_push_tail (tab->callQueue, (gpointer *) c);
      calltree_add_call( history , c );
    }
    // List full -> Remove the last call from history and preprend the new call to the list
    else
    {
      calltree_remove_call( history , (call_t*)g_queue_pop_head( tab -> callQueue ) );
      g_queue_push_tail (tab->callQueue, (gpointer *) c);
      calltree_add_call( history , c );
    }
  }
  else
    g_queue_push_tail (tab->callQueue, (gpointer *) c);
}

// TODO : sflphoneGTK : try to do this more generic
void
calllist_clean_history( void )
{
  unsigned int i;
  guint size = calllist_get_size( history );
  g_print("history list size = %i\n", calllist_get_size( history ));
  for( i = 0 ; i < size ; i++ )
  {
    g_print("Delete calls");
    call_t* c = calllist_get_nth( history , i );
    // Delete the call from the call tree
    g_print("Delete calls");
    calltree_remove_call(history , c);
  }
  calllist_reset( history );
}

// TODO : sflphoneGTK : try to do this more generic
void
calllist_remove_from_history( call_t* c )
{
  calllist_remove( history, c->callID );
  calltree_remove_call( history, c );
  g_print("Size of history = %i\n" , calllist_get_size( history ));
}

void
calllist_remove (calltab_t* tab, const gchar * callID)
{
  call_t * c = calllist_get(tab, callID);
  if (c)
  {
    g_queue_remove(tab->callQueue, c);
  }
}


call_t *
calllist_get_by_state (calltab_t* tab, call_state_t state )
{
  GList * c = g_queue_find_custom (tab->callQueue, &state, get_state_callstruct);
  if (c)
  {
    return (call_t *)c->data;
  }
  else
  {
    return NULL;
  }

}

guint
calllist_get_size (calltab_t* tab)
{
  return g_queue_get_length (tab->callQueue);
}

call_t *
calllist_get_nth (calltab_t* tab, guint n )
{
  return g_queue_peek_nth (tab->callQueue, n);
}

call_t *
calllist_get (calltab_t* tab, const gchar * callID )
{
  GList * c = g_queue_find_custom (tab->callQueue, callID, is_callID_callstruct);
  if (c)
  {
    return (call_t *)c->data;
  }
  else
  {
    return NULL;
  }
}
