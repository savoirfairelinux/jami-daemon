#ifndef _RINGAPPLICATIONWINDOW_H
#define _RINGAPPLICATIONWINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define RING_APPLICATION_WINDOW_TYPE (ring_application_window_get_type ())
#define RING_APPLICATION_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RING_APPLICATION_WINDOW_TYPE, RingApplicationWindow))


typedef struct _RingApplicationWindow         RingApplicationWindow;
typedef struct _RingApplicationWindowClass    RingApplicationWindowClass;


GType       ring_application_window_get_type     (void);
GtkWidget  *ring_application_window_new          (GtkApplication *app);

G_END_DECLS

#endif /* _RINGAPPLICATIONWINDOW_H */
