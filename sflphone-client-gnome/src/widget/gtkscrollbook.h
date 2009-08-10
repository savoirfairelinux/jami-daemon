/*
 * @file gtkscrollbook  GTK+ Scrolling notebook Widget
 * @ingroup pidgin
 */

/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#ifndef __PIDGIN_SCROLL_BOOK_H__
#define __PIDGIN_SCROLL_BOOK_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_SCROLL_BOOK             (pidgin_scroll_book_get_type ())
#define PIDGIN_SCROLL_BOOK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_SCROLL_BOOK, PidginScrollBook))
#define PIDGIN_SCROLL_BOOK_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), PIDGIN_TYPE_SCROLL_BOOK, PidginScrollBookClass))
#define PIDGIN_IS_SCROLL_BOOK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_SCROLL_BOOK))
#define PIDGIN_IS_SCROLL_BOOK_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), PIDGIN_TYPE_SCROLL_BOOK))
#define PIDGIN_SCROLL_BOOK_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_CLASS ((inst), PIDGIN_TYPE_SCROLL_BOOK, PidginScrollBookClass))

typedef struct _PidginScrollBook      PidginScrollBook;
typedef struct _PidginScrollBookClass PidginScrollBookClass;

struct _PidginScrollBook
{
	GtkVBox parent_instance;

	GtkWidget *notebook;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *left_arrow;
	GtkWidget *right_arrow;
	GList *children;
	
	/* Padding for future expansion */
	void (*_gtk_reserved1) (void);
	void (*_gtk_reserved2) (void);
	void (*_gtk_reserved3) (void);

};


struct _PidginScrollBookClass
{
	GtkContainerClass parent_class;

	/* Padding for future expansion */
	void (*_gtk_reserved0) (void);
	void (*_gtk_reserved1) (void);
	void (*_gtk_reserved2) (void);
	void (*_gtk_reserved3) (void);
};


GType         pidgin_scroll_book_get_type         (void) G_GNUC_CONST;
GtkWidget    *pidgin_scroll_book_new              (void);

G_END_DECLS

#endif  /* __PIDGIN_SCROLL_BOOK_H__ */
