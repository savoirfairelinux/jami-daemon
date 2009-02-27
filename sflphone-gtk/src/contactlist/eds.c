#include <glib.h>
#include <glib/gstring.h>
#include <libebook/e-book.h>
#include <pango/pango.h>
#include <string.h>
#include "eds.h"

typedef struct _Handler_And_Data {
  SearchAsyncHandler  handler;
  gpointer            user_data;
  GList              *hits;
  int                 max_results_remaining;
  int                 book_views_remaining;
} Handler_And_Data;

static GSList *books = NULL;
static int pixbuf_size = 16;

static EContactField search_fields[] = { E_CONTACT_FULL_NAME, E_CONTACT_EMAIL, E_CONTACT_NICKNAME, 0 };
static int n_search_fields = G_N_ELEMENTS (search_fields) - 1;




int
main (int argc, char *argv[])
{
  GList *results;
  GList *i;

  gtk_init (&argc, &argv);
  init();
  results = search_sync ("sch", 50);

if(results == NULL)
{
  printf("null\n");
  return -1;
}

for (i = results; i != NULL; i = i->next)
{
  Hit *entry;
  entry = i->data;
  printf("entree\n");
  if (i->data) {
    printf("email : %s\n", entry->email);
    printf("text : %s\n", entry->text);
  }
}

printf("fini\n");


return 0;
}






void
free_hit (Hit *h, gpointer unused)
{
    g_free (h->text);
    g_free (h->email);
    g_free (h->uri);
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

static GdkPixbuf*
pixbuf_from_contact (EContact *contact)
{
  GdkPixbuf *pixbuf = NULL;
  EContactPhoto *photo = e_contact_get (contact, E_CONTACT_PHOTO);
  if (photo) {
    GdkPixbufLoader *loader;

    loader = gdk_pixbuf_loader_new ();

    if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
      if (gdk_pixbuf_loader_write (loader, (guchar *) photo->data.inlined.data, photo->data.inlined.length, NULL))
        pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
    }

    if (pixbuf) {
      GdkPixbuf *tmp;
      gint width = gdk_pixbuf_get_width (pixbuf);
      gint height = gdk_pixbuf_get_height (pixbuf);
      double scale = 1.0;

      if (height > width) {
        scale = pixbuf_size / (double) height;
      } else {
        scale = pixbuf_size / (double) width;
      }

      if (scale < 1.0) {
        tmp = gdk_pixbuf_scale_simple (pixbuf, width * scale, height * scale, GDK_INTERP_BILINEAR);
        g_object_unref (pixbuf);
        pixbuf = tmp;
      }
    }
    e_contact_photo_free (photo);
  }
  return pixbuf;
}

static void
view_finish (EBookView *book_view, Handler_And_Data *had)
{
  SearchAsyncHandler had_handler = had->handler;
  GList *had_hits = had->hits;
  gpointer had_user_data = had->user_data;
  g_free (had);

  g_return_if_fail (book_view != NULL);
  g_object_unref (book_view);

  had_handler (had_hits, had_user_data);
}

static void
view_contacts_added_cb (EBookView *book_view, GList *contacts, gpointer user_data)
{
  Handler_And_Data *had = (Handler_And_Data *) user_data;
  if (had->max_results_remaining <= 0) {
    e_book_view_stop (book_view);
    had->book_views_remaining--;
    if (had->book_views_remaining == 0) {
      view_finish (book_view, had);
      return;
    }
  }
  for (; contacts != NULL; contacts = g_list_next (contacts)) {
    EContact *contact;
    Hit *hit;

    contact = E_CONTACT (contacts->data);
    hit = g_new (Hit, 1);
    hit->email = g_strdup ((char*) e_contact_get_const (contact, E_CONTACT_EMAIL_1));
    hit->text = g_strdup_printf ("%s <%s>", (char*)e_contact_get_const (contact, E_CONTACT_NAME_OR_ORG), hit->email);
    hit->pixbuf = pixbuf_from_contact (contact);

    had->hits = g_list_append (had->hits, hit);
    had->max_results_remaining--;
    if (had->max_results_remaining <= 0) {
      e_book_view_stop (book_view);
      had->book_views_remaining--;
      if (had->book_views_remaining == 0) {
        view_finish (book_view, had);
      }
      break;
    }
  }
}

static void
view_completed_cb (EBookView *book_view, EBookViewStatus status, gpointer user_data)
{
  Handler_And_Data *had = (Handler_And_Data *) user_data;
  had->book_views_remaining--;
  if (had->book_views_remaining == 0) {
    view_finish (book_view, had);
  }
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
      const char *p;

      /*p = e_source_get_property (source, "completion");*/

      /*if (p != NULL && strcmp (p, "true") == 0) {*/
      if (1) {
        EBook *book = e_book_new (source, NULL);
        if (book != NULL) {
          books = g_slist_prepend (books, book);
          e_book_open(book, TRUE, NULL);
        }
      }
    }
  }

  g_object_unref (source_list);
}

int
num_address_books_with_completion (void)
{
  int result = 0;
  GSList *list, *l;
  ESourceList *source_list;

  source_list = e_source_list_new_for_gconf_default ("/apps/evolution/addressbook/sources");
  if (source_list == NULL) {
    return 0;
  }

  list = e_source_list_peek_groups (source_list);

  for (l = list; l != NULL; l = l->next) {
    ESourceGroup *group = l->data;
    GSList *sources = NULL, *m;
    sources = e_source_group_peek_sources (group);
    for (m = sources; m != NULL; m = m->next) {
      ESource *source = m->data;
      const char *p;

      p = e_source_get_property (source, "completion");

      if (p != NULL && strcmp (p, "true") == 0) {
        result++;
      }
    }
  }

  g_object_unref (source_list);
  return result;
}

void
set_pixbuf_size (int size)
{
  pixbuf_size = size;
}

void
search_async (const char         *query,
              int                 max_results,
              SearchAsyncHandler  handler,
              gpointer            user_data)
{
  GSList *iter;

  EBookQuery* book_query = create_query (query);

  Handler_And_Data *had = g_new (Handler_And_Data, 1);
  had->handler = handler;
  had->user_data = user_data;
  had->hits = NULL;
  had->max_results_remaining = max_results;
  had->book_views_remaining = 0;
  for (iter = books; iter != NULL; iter = iter->next) {
    EBook *book = (EBook *) iter->data;
    EBookView *book_view = NULL;
    e_book_get_book_view (book, book_query, NULL, max_results, &book_view, NULL);
    if (book_view != NULL) {
      had->book_views_remaining++;
      g_signal_connect (book_view, "contacts_added", (GCallback) view_contacts_added_cb, had);
      g_signal_connect (book_view, "sequence_complete", (GCallback) view_completed_cb, had);
      e_book_view_start (book_view);
    }
  }
  if (had->book_views_remaining == 0) {
    g_free (had);
  }

  e_book_query_unref (book_query);
}

/*
 * Note: you may get a message "WARNING **: FIXME: wait for completion unimplemented"
 * if you call search_sync but are not running the gobject main loop.
 * This appears to be harmless: http://bugzilla.gnome.org/show_bug.cgi?id=314544
 */
GList *
search_sync (const char *query,
             int         max_results)
{
  GSList *iter = NULL;
  GList *contacts = NULL;
  GList *hits = NULL;

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
      const char *uid;
      ESource *source;
      const char *source_uid;

      contact = E_CONTACT (contacts->data);
      hit = g_new (Hit, 1);
      if (e_contact_get (contact, E_CONTACT_IS_LIST)){
        GList *emailList = e_contact_get (contact, E_CONTACT_EMAIL);
        int i=0;
        hit->email = (gchar*)g_list_nth(emailList,i)->data;
        for (i=1; g_list_nth(emailList,i) != NULL; i++)
          hit->email = g_strjoin(",",hit->email,((gchar*)g_list_nth(emailList,i)->data), NULL);
        g_list_foreach(emailList, (GFunc)g_free, NULL);
          g_list_free(emailList);
      }
      else
        hit->email = g_strdup ((char*) e_contact_get_const (contact, E_CONTACT_EMAIL_1));
      hit->text = g_strdup ((char*) e_contact_get_const (contact, E_CONTACT_NAME_OR_ORG));
      hit->pixbuf = pixbuf_from_contact (contact);

      uid = e_contact_get_const (contact, E_CONTACT_UID);
      source = e_book_get_source (book);
      source_uid = e_source_peek_uid (source);
      hit->uri = g_strdup_printf ("contacts:///?source-uid=%s&contact-uid=%s", source_uid, uid);

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
