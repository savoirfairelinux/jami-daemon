/**
 * @file minidialog.h API for the #PidginMiniDialog Gtk widget.
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

#ifndef __PIDGIN_MINI_DIALOG_H__
#define __PIDGIN_MINI_DIALOG_H__

#include <glib-object.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_MINI_DIALOG pidgin_mini_dialog_get_type()

#define PIDGIN_MINI_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  PIDGIN_TYPE_MINI_DIALOG, PidginMiniDialog))

#define PIDGIN_MINI_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  PIDGIN_TYPE_MINI_DIALOG, PidginMiniDialogClass))

#define PIDGIN_IS_MINI_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  PIDGIN_TYPE_MINI_DIALOG))

#define PIDGIN_IS_MINI_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  PIDGIN_TYPE_MINI_DIALOG))

#define PIDGIN_MINI_DIALOG_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  PIDGIN_TYPE_MINI_DIALOG, PidginMiniDialogClass))

/**
 * A widget resembling a diminutive dialog box, designed to be embedded in the
 * #PidginBuddyList.  Mini-dialogs have titles, optional descriptions, and a row
 * of buttons at the bottom; above the buttons is a <tt>GtkHBox</tt> into which
 * you can pack any random widgets you want to add to the dialog.  When any of
 * the dialog's buttons is clicked, the dialog will be destroyed.
 *
 * Dialogs have the following GObject properties:
 * <dl>
 *   <dt><tt>"title"</tt> (<tt>char *</tt>)</dt>
 *   <dd>A string to be displayed as the dialog's title.</dd>
 *   <dt><tt>"description"</tt> (<tt>char *</tt>)</dt>
 *   <dd>A string to be displayed as the dialog's description.  If this is @c
 *       NULL, the description widget will be hidden.
 *   </dd>
 *   <dt><tt>"icon-name"</tt> (<tt>char *</tt>)</dt>
 *   <dd>The Gtk stock id of an icon for the dialog, or @c NULL for no icon.
 *       @see pidginstock.h
 *   </dd>
 * </dl>
 */
typedef struct {
	GtkVBox parent;

	/** A GtkVBox into which extra widgets for the dialog should be packed.
	 */
	GtkBox *contents;

	gpointer priv;
} PidginMiniDialog;

/** The class of #PidginMiniDialog objects. */
typedef struct {
	GtkBoxClass parent_class;

	void (*_purple_reserved1) (void);
	void (*_purple_reserved2) (void);
	void (*_purple_reserved3) (void);
	void (*_purple_reserved4) (void);
} PidginMiniDialogClass;

/** The type of a callback triggered by a button in a mini-dialog being pressed.
 * @param mini_dialog a dialog, one of whose buttons has been pressed.
 * @param button      the button which was pressed.
 * @param user_data   arbitrary data, supplied to
 *                    pidgin_mini_dialog_add_button() when the button was
 *                    created.
 */
typedef void (*PidginMiniDialogCallback)(PidginMiniDialog *mini_dialog,
	GtkButton *button, gpointer user_data);

/** Get the GType of #PidginMiniDialog. */
GType pidgin_mini_dialog_get_type (void);

/** Creates a new #PidginMiniDialog.  This is a shortcut for creating the dialog
 *  with @c g_object_new() then setting each property yourself.
 *  @return a new #PidginMiniDialog.
 */
PidginMiniDialog *pidgin_mini_dialog_new(const gchar *title,
	const gchar *description, const gchar *icon_name);

/** Shortcut for setting a mini-dialog's title via GObject properties.
 *  @param mini_dialog a mini-dialog
 *  @param title       the new title for @a mini_dialog
 */
void pidgin_mini_dialog_set_title(PidginMiniDialog *mini_dialog,
	const char *title);

/** Shortcut for setting a mini-dialog's description via GObject properties.
 *  @param mini_dialog a mini-dialog
 *  @param description the new description for @a mini_dialog, or @c NULL to
 *                     hide the description widget.
 */
void pidgin_mini_dialog_set_description(PidginMiniDialog *mini_dialog,
	const char *description);

/** Shortcut for setting a mini-dialog's icon via GObject properties.
 *  @param mini_dialog a mini-dialog
 *  @param icon_name   the Gtk stock ID of an icon, or @c NULL for no icon.
 */
void pidgin_mini_dialog_set_icon_name(PidginMiniDialog *mini_dialog,
	const char *icon_name);

/** Adds a new button to a mini-dialog, and attaches the supplied callback to
 *  its <tt>clicked</tt> signal.  After a button is clicked, the dialog is
 *  destroyed.
 *  @param mini_dialog a mini-dialog
 *  @param text        the text to display on the new button
 *  @param clicked_cb  the function to call when the button is clicked
 *  @param user_data   arbitrary data to pass to @a clicked_cb when it is
 *                     called.
 */
void pidgin_mini_dialog_add_button(PidginMiniDialog *mini_dialog,
	const char *text, PidginMiniDialogCallback clicked_cb,
	gpointer user_data);

/** Gets the number of widgets packed into PidginMiniDialog.contents.
 *  @param mini_dialog a mini-dialog
 *  @return the number of widgets in @a mini_dialog->contents.
 */
guint pidgin_mini_dialog_get_num_children(PidginMiniDialog *mini_dialog);

G_END_DECLS

#endif /* __PIDGIN_MINI_DIALOG_H__ */
