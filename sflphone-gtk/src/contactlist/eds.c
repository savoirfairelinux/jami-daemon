#include <glib.h>
#include <glib/gstring.h>
#include <libebook/e-book.h>
#include <pango/pango.h>
#include <string.h>
#include "eds.h"

static GSList *books = NULL;

static EContactField search_fields[] = { E_CONTACT_FULL_NAME, E_CONTACT_PHONE_BUSINESS, E_CONTACT_NICKNAME, 0 };
static int n_search_fields = G_N_ELEMENTS (search_fields) - 1;

void
free_hit (Hit *h)
{
    g_free (h->name);
    g_free (h->phone);
    g_free (h);
}

/**
 * Split a string of tokens separated by whitespace into an array of tokens.
 */
static GArray *
split_query_string (const gchar *str)
{
  GArray *parts = g_array_sized_new (FALSE, FALSE, sizeof (char *), 2);
  PangoLogAttr *attrs;
  guint str_len = strlen (str), word_start = 0, i;

  attrs = g_new0 (PangoLogAttr, str_len + 1);
  /* TODO: do we need to specify a particular language or is NULL ok? */
  pango_get_log_attrs (str, -1, -1, NULL, attrs, str_len + 1);

  for (i = 0; i < str_len + 1; i++) {
    char *start_word, *end_word, *word;
    if (attrs[i].is_word_end) {
      start_word = g_utf8_offset_to_pointer (str, word_start);
      end_word = g_utf8_offset_to_pointer (str, i);
      word  = g_strndup (start_word, end_word - start_word);
      g_array_append_val (parts, word);
    }
    if (attrs[i].is_word_start) {
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
create_query (const char* s)
{
  EBookQuery *query;
  GArray *parts = split_query_string (s);
  EBookQuery ***field_queries;
  EBookQuery **q;
  guint j;
  int i;

  q = g_new0 (EBookQuery *, n_search_fields);
  field_queries = g_new0 (EBookQuery **, n_search_fields);

  for (i = 0; i < n_search_fields; i++) {
    field_queries[i] = g_new0 (EBookQuery *, parts->len);
    for (j = 0; j < parts->len; j++) {
      field_queries[i][j] = e_book_query_field_test (search_fields[i], E_BOOK_QUERY_CONTAINS, g_array_index (parts, gchar *, j));
    }
    q[i] = e_book_query_and (parts->len, field_queries[i], TRUE);
  }
  g_array_free (parts, TRUE);

  query = e_book_query_or (n_search_fields, q, TRUE);

  for (i = 0; i < n_search_fields; i++) {
    g_free (field_queries[i]);
  }
  g_free (field_queries);
  g_free (q);

  return query;
}

void
init (void)
{
  GSList *list, *l;
  ESourceList *source_list;
  source_list = e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");

  if (source_list == NULL) {
    return;
  }
  list = e_source_list_peek_groups (source_list);

  for (l = list; l != NULL; l = l->next) {
    ESourceGroup *group = l->data;
    GSList *sources = NULL, *m;
    sources = e_source_group_peek_sources (group);
    for (m = sources; m != NULL; m = m->next) {
      ESource *source = m->data;
      EBook *book = e_book_new (source, NULL);
      if (book != NULL) {
        books = g_slist_prepend (books, book);
        e_book_open(book, TRUE, NULL);
      }
    }
  }

  g_object_unref (source_list);
}

GList *
search_sync (const char *query,
             int         max_results)
{
  GSList *iter = NULL;
  GList *contacts = NULL;
  GList *hits = NULL;
  char ext[30];

  EBookQuery* book_query = create_query (query);
  for (iter = books; iter != NULL; iter = iter->next) {
    if (max_results <= 0) {
      break;
    }
    EBook *book = (EBook *) iter->data;
    e_book_get_contacts (book, book_query, &contacts, NULL);
    for (; contacts != NULL; contacts = g_list_next (contacts)) {
      EContact *contact;
      Hit *hit;

      contact = E_CONTACT (contacts->data);
      hit = g_new (Hit, 1);

      hit->phone = g_strdup ((char*) e_contact_get_const (contact, E_CONTACT_PHONE_BUSINESS));
      if(! hit->phone)
      {
        // Temporary fix for empty phone numbers
        sprintf(ext, "%d", rand()%100 + 100);
        hit->phone = g_strconcat("rand",ext,NULL);
        //hit->phone = "";
      }

      hit->name = g_strdup ((char*) e_contact_get_const (contact, E_CONTACT_NAME_OR_ORG));
      if(! hit->name)
        hit->name = "";

      hits = g_list_append (hits, hit);
      max_results--;
      if (max_results <= 0) {
        break;
      }
    }
  }
  e_book_query_unref (book_query);

  return hits;
}
