/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

#include <addressbook.h>
#include <searchbar.h>
#include <addressbook-config.h>

static void
handler_async_search(GList *, gpointer);

void
addressbook_search(GtkEntry* entry)
{

  AddressBook_Config *addressbook_config;

  // Activate waiting layer
  activateWaitingLayer();

  // Load the address book parameters
  addressbook_config_load_parameters(&addressbook_config);

  // Start the asynchronous search as soon as we have an entry */
  search_async(gtk_entry_get_text(GTK_ENTRY (entry)), addressbook_config->max_results, &handler_async_search,
      addressbook_config);
}

void
addressbook_init()
{
  init();
}

static void
handler_async_search(GList *hits, gpointer user_data)
{

  GList *i;
  GdkPixbuf *photo = NULL;
  AddressBook_Config *addressbook_config;
  call_t *j;

  // freeing calls
  while ((j = (call_t *) g_queue_pop_tail(contacts->callQueue)) != NULL)
    {
      free_call_t(j);
    }

  // Retrieve the address book parameters
  addressbook_config = (AddressBook_Config*) user_data;

  // reset previous results
  calltree_reset(contacts);
  calllist_reset(contacts);

  for (i = hits; i != NULL; i = i->next)
    {
      Hit *entry;
      entry = i->data;
      if (entry)
        {
          /* Get the photo */
          if (addressbook_display(addressbook_config,
              ADDRESSBOOK_DISPLAY_CONTACT_PHOTO))
            photo = entry->photo;
          /* Create entry for business phone information */
          if (addressbook_display(addressbook_config,
              ADDRESSBOOK_DISPLAY_PHONE_BUSINESS))
            calllist_add_contact(entry->name, entry->phone_business,
                CONTACT_PHONE_BUSINESS, photo);
          /* Create entry for home phone information */
          if (addressbook_display(addressbook_config,
              ADDRESSBOOK_DISPLAY_PHONE_HOME))
            calllist_add_contact(entry->name, entry->phone_home,
                CONTACT_PHONE_HOME, photo);
          /* Create entry for mobile phone information */
          if (addressbook_display(addressbook_config,
              ADDRESSBOOK_DISPLAY_PHONE_MOBILE))
            calllist_add_contact(entry->name, entry->phone_mobile,
                CONTACT_PHONE_MOBILE, photo);
        }
      free_hit(entry);
    }
  g_list_free(hits);

  // Deactivate waiting image
  deactivateWaitingLayer();
}
