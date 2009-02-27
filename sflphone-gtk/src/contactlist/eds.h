
#ifndef __DESKBAR_EVOLUTION_H__
#define __DESKBAR_EVOLUTION_H__

#include <glib/gtypes.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef struct _Hit {
  gchar *text;
  gchar *email;
  GdkPixbuf *pixbuf;
  gchar *uri;
} Hit;

void free_hit (Hit *hit, gpointer unused);

typedef void (* SearchAsyncHandler) (GList *hits, gpointer user_data);

void init (void);

void set_pixbuf_size (int size);

void search_async (const char         *query,
                   int                 max_results,
                   SearchAsyncHandler  handler,
                   gpointer            user_data);

GList * search_sync (const char *query,
                     int         max_results);

G_END_DECLS

#endif /* __DESKBAR_EVOLUTION_H__ */

