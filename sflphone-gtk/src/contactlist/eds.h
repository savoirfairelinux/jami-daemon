
#ifndef __DESKBAR_EVOLUTION_H__
#define __DESKBAR_EVOLUTION_H__

#include <glib/gtypes.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef struct _Hit {
  gchar *name;
  gchar *phone;
} Hit;

void free_hit (Hit *hit, gpointer unused);

void init (void);

GList * search_sync (const char *query,
                     int         max_results);

G_END_DECLS

#endif /* __DESKBAR_EVOLUTION_H__ */

