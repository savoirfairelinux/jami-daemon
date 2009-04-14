/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
 *
 *  File originally copied from evolution module of deskbar-applet 2.24.1
 *   Authors :
 *    Nigel Tao <nigel.tao@myrealbox.com>
 *    Raphaël Slinckx <raphael@slinckx.net>
 *    Mikkel Kamstrup Erlandsen <kamstrup@daimi.au.dk>
 *    Sebastian Pölsterl <marduk@k-d-w.org>
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

#include <glib.h>
#include <glib/gstring.h>
#include <string.h>
#include <pango/pango.h>
#include "eds.h"

typedef struct _Handler_And_Data
{
  int search_id;
  SearchAsyncHandler handler;
  gpointer user_data;
  GList *hits;
  int max_results_remaining;
  int book_views_remaining;
} Handler_And_Data;

static int pixbuf_size = 32;

static EContactField search_fields[] =
  { E_CONTACT_FULL_NAME, E_CONTACT_PHONE_BUSINESS, E_CONTACT_NICKNAME, 0 };
static int n_search_fields = G_N_ELEMENTS (search_fields) - 1;

void
free_hit(Hit *h)
{
  g_free(h->name);
  g_free(h->phone_business);
  g_free(h->phone_home);
  g_free(h->phone_mobile);
  g_free(h);
}

book_data_t *
books_get_book_data_by_uid(gchar *uid)
{
  GSList *book_list_iterator;
  book_data_t *book_data;

  for (book_list_iterator = books_data; book_list_iterator != NULL; book_list_iterator
      = book_list_iterator->next)
    {
      book_data = (book_data_t *) book_list_iterator->data;
      if (strcmp(book_data->uid, uid) == 0)
        return book_data;
    }
  return NULL;
}

/**
 * Split a string of tokens separated by whitespace into an array of tokens.
 */
static GArray *
split_query_string(const gchar *str)
{
  GArray *parts = g_array_sized_new(FALSE, FALSE, sizeof (char *), 2);
  PangoLogAttr *attrs;
  guint str_len = strlen (str), word_start = 0, i;

  attrs = g_new0 (PangoLogAttr, str_len + 1);
  /* TODO: do we need to specify a particular language or is NULL ok? */
  pango_get_log_attrs (str, -1, -1, NULL, attrs, str_len + 1);

  for (i = 0; i < str_len + 1; i++)
    {
      char *start_word, *end_word, *word;
      if (attrs[i].is_word_end)
        {
          start_word = g_utf8_offset_to_pointer (str, word_start);
          end_word = g_utf8_offset_to_pointer (str, i);
          word = g_strndup (start_word, end_word - start_word);
          g_array_append_val (parts, word);
        }
      if (attrs[i].is_word_start)
        {
          word_start = i;
        }
    }
  g_free (attrs);
  return parts;
}

  /**
   * Create a query which looks for the specified string in a contact's full name, email addresses and
   * nick name.
   */
static EBookQuery*
create_query(const char* s)
{
  EBookQuery *query;
  GArray *parts = split_query_string(s);
  EBookQuery ***field_queries;
  EBookQuery **q;
  EBookQuery **phone;
  guint j;
  int i;

  q = g_new0 (EBookQuery *, n_search_fields);
  field_queries = g_new0 (EBookQuery **, n_search_fields);

  for (i = 0; i < n_search_fields; i++)
    {
      field_queries[i] = g_new0 (EBookQuery *, parts->len);
      for (j = 0; j < parts->len; j++)
        {
          field_queries[i][j] = e_book_query_field_test(search_fields[i],
              E_BOOK_QUERY_CONTAINS, g_array_index (parts, gchar *, j));
        }
      q[i] = e_book_query_and(parts->len, field_queries[i], TRUE);
    }
  g_array_free(parts, TRUE);

  phone = g_new0 (EBookQuery *, 3);
  phone[0] = e_book_query_field_exists(E_CONTACT_PHONE_BUSINESS);
  phone[1] = e_book_query_field_exists(E_CONTACT_PHONE_HOME);
  phone[2] = e_book_query_field_exists(E_CONTACT_PHONE_MOBILE);

  query = e_book_query_andv(e_book_query_or(n_search_fields, q, FALSE),
      e_book_query_or(3, phone, FALSE), NULL);

  for (i = 0; i < n_search_fields; i++)
    {
      g_free(field_queries[i]);
    }
  g_free(field_queries);
  g_free(q);
  g_free(phone);

  return query;
}

static GdkPixbuf*
pixbuf_from_contact(EContact *contact)
{

  GdkPixbuf *pixbuf = NULL;
  EContactPhoto *photo = e_contact_get(contact, E_CONTACT_PHOTO);
  if (photo)
    {
      GdkPixbufLoader *loader;

      loader = gdk_pixbuf_loader_new();

      if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED)
        {
          if (gdk_pixbuf_loader_write(loader,
              (guchar *) photo->data.inlined.data, photo->data.inlined.length,
              NULL))
            pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        }

      if (pixbuf)
        {
          GdkPixbuf *tmp;
          gint width = gdk_pixbuf_get_width(pixbuf);
          gint height = gdk_pixbuf_get_height(pixbuf);
          double scale = 1.0;

          if (height > width)
            {
              scale = pixbuf_size / (double) height;
            }
          else
            {
              scale = pixbuf_size / (double) width;
            }

          if (scale < 1.0)
            {
              tmp = gdk_pixbuf_scale_simple(pixbuf, width * scale, height
                  * scale, GDK_INTERP_BILINEAR);
              g_object_unref(pixbuf);
              pixbuf = tmp;
            }
        }
      e_contact_photo_free(photo);
    }
  return pixbuf;
}

/**
 * Initialize address book
 */
void
init(void)
{
  GSList *list, *l;
  ESourceList *source_list;
  book_data_t *book_data;

  source_list = e_source_list_new_for_gconf_default(
      "/apps/evolution/addressbook/sources");

  if (source_list == NULL)
    {
      return;
    }
  list = e_source_list_peek_groups(source_list);

  for (l = list; l != NULL; l = l->next)
    {
      ESourceGroup *group = l->data;
      GSList *sources = NULL, *m;
      sources = e_source_group_peek_sources(group);
      for (m = sources; m != NULL; m = m->next)
        {
          ESource *source = m->data;
          EBook *book = e_book_new(source, NULL);
          if (book != NULL)
            {
              book_data = g_new(book_data_t, 1);
              book_data->active = FALSE;
              book_data->name = g_strdup(e_source_peek_name(source));
              book_data->uid = g_strdup(e_source_peek_uid(source));
              book_data->ebook = book;
              books_data = g_slist_prepend(books_data, book_data);
              e_book_open(book, TRUE, NULL);
            }
        }
    }

  current_search_id = 0;

  g_object_unref (source_list);
}

              /**
               * Final callback after all books have been processed.
               */
static void
view_finish(EBookView *book_view, Handler_And_Data *had)
{
  GList *i;
  SearchAsyncHandler had_handler = had->handler;
  GList *had_hits = had->hits;
  gpointer had_user_data = had->user_data;
  int search_id = had->search_id;
  g_free(had);

  if (book_view != NULL)
    g_object_unref(book_view);

  if (search_id == current_search_id)
    {
      // Reinitialize search id to prevent overflow
      if (current_search_id > 5000)
        current_search_id = 0;

      // Call display callback
      had_handler(had_hits, had_user_data);
    }
  else
    {
      // Some hits could have been processed but will not be used
      for (i = had_hits; i != NULL; i = i->next)
        {
          Hit *entry;
          entry = i->data;
          free_hit(entry);
        }
      g_list_free(had_hits);
    }
}

/**
 * Callback called after each ebook search completed.
 * Used to store book search results.
 */
static void
view_contacts_added_cb(EBookView *book_view, GList *contacts,
    gpointer user_data)
{
  GdkPixbuf *photo;

  Handler_And_Data *had = (Handler_And_Data *) user_data;

  if (had->search_id != current_search_id)
    {
      e_book_view_stop(book_view);
      return;
    }

  if (had->max_results_remaining <= 0)
    {
      e_book_view_stop(book_view);
      had->book_views_remaining--;
      if (had->book_views_remaining == 0)
        {
          view_finish(book_view, had);
          return;
        }
    }
  for (; contacts != NULL; contacts = g_list_next (contacts))
    {
      EContact *contact;
      Hit *hit;
      gchar *number;

      contact = E_CONTACT (contacts->data);
      hit = g_new (Hit, 1);

      /* Get the photo contact */
      photo = pixbuf_from_contact(contact);
      hit->photo = photo;

      /* Get business phone information */
      fetch_information_from_contact(contact, E_CONTACT_PHONE_BUSINESS, &number);
      hit->phone_business = g_strdup(number);

      /* Get home phone information */
      fetch_information_from_contact(contact, E_CONTACT_PHONE_HOME, &number);
      hit->phone_home = g_strdup(number);

      /* Get mobile phone information */
      fetch_information_from_contact(contact, E_CONTACT_PHONE_MOBILE, &number);
      hit->phone_mobile = g_strdup(number);

      hit->name = g_strdup((char*) e_contact_get_const(contact,
          E_CONTACT_NAME_OR_ORG));
      if (!hit->name)
        hit->name = "";

      had->hits = g_list_append(had->hits, hit);
      had->max_results_remaining--;
      if (had->max_results_remaining <= 0)
        {
          e_book_view_stop(book_view);
          had->book_views_remaining--;
          if (had->book_views_remaining == 0)
            {
              view_finish(book_view, had);
            }
          break;
        }
    }
}

/**
 * Callback called after each ebook search completed.
 * Used to call final callback when all books have been read.
 */
static void
view_completed_cb(EBookView *book_view, EBookViewStatus status UNUSED,
    gpointer user_data)
{
  Handler_And_Data *had = (Handler_And_Data *) user_data;
  had->book_views_remaining--;
  if (had->book_views_remaining == 0)
    {
      view_finish(book_view, had);
    }
}

/**
 * Perform an asynchronous search
 */
void
search_async(const char *query, int max_results, SearchAsyncHandler handler,
    gpointer user_data)
{
  // Increment search id
  current_search_id++;

  // If query is null
  if (strlen(query) < 1)
    {
      // If data displayed (from previous search), directly call callback
      handler(NULL, user_data);

      return;
    }

  GSList *iter;
  EBookQuery* book_query = create_query(query);
  Handler_And_Data *had = g_new (Handler_And_Data, 1);
  int search_count = 0;

  had->search_id = current_search_id;
  had->handler = handler;
  had->user_data = user_data;
  had->hits = NULL;
  had->max_results_remaining = max_results;
  had->book_views_remaining = 0;
  for (iter = books_data; iter != NULL; iter = iter->next)
    {
      book_data_t *book_data = (book_data_t *) iter->data;

      if (book_data->active)
        {
          EBookView *book_view = NULL;
          e_book_get_book_view(book_data->ebook, book_query, NULL, max_results,
              &book_view, NULL);
          if (book_view != NULL)
            {
              had->book_views_remaining++;
              g_signal_connect (book_view, "contacts_added", (GCallback) view_contacts_added_cb, had);
              g_signal_connect (book_view, "sequence_complete", (GCallback) view_completed_cb, had);
              e_book_view_start(book_view);
              search_count++;
            }
        }
    }

  e_book_query_unref(book_query);

  // If no search has been executed (no book selected)
  if (search_count == 0)
    {
      // Call last callback anyway
      view_finish(NULL, had);
    }
}

/**
 * Fetch information for a specific contact
 */
void
fetch_information_from_contact(EContact *contact, EContactField field,
    gchar **info)
{
  gchar *to_fetch;

  to_fetch = g_strdup((char*) e_contact_get_const(contact, field));
  if (!to_fetch)
    {
      to_fetch = g_strdup(EMPTY_ENTRY);
    }

  *info = g_strdup(to_fetch);
}
