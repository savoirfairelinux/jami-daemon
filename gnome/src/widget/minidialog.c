/**
 * @file minidialog.c Implementation of the #PidginMiniDialog Gtk widget.
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
#include <gtk/gtk.h>
#include "sflphone_const.h"
#include "minidialog.h"

#define HIG_BOX_SPACE 6
#define LABEL_WIDTH 200

static void pidgin_mini_dialog_init(PidginMiniDialog *self);
static void pidgin_mini_dialog_class_init(PidginMiniDialogClass *klass);

static gpointer pidgin_mini_dialog_parent_class = NULL;

static void
pidgin_mini_dialog_class_intern_init(gpointer klass)
{
    pidgin_mini_dialog_parent_class = g_type_class_peek_parent(klass);
    pidgin_mini_dialog_class_init((PidginMiniDialogClass*) klass);
}

GType
pidgin_mini_dialog_get_type(void)
{
    static GType g_define_type_id = 0;

    if (g_define_type_id == 0) {
        static const GTypeInfo g_define_type_info = {
            sizeof(PidginMiniDialogClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) pidgin_mini_dialog_class_intern_init,
            (GClassFinalizeFunc) NULL,
            NULL,   /* class_data */
            sizeof(PidginMiniDialog),
            0,      /* n_preallocs */
            (GInstanceInitFunc) pidgin_mini_dialog_init,
            NULL,
        };
        g_define_type_id = g_type_register_static(GTK_TYPE_BOX,
                           "PidginMiniDialog", &g_define_type_info, 0);
    }

    return g_define_type_id;
}

enum {
    PROP_TITLE = 1,
    PROP_DESCRIPTION,
    PROP_ICON_NAME,

    LAST_PROPERTY
} HazeConnectionProperties;

typedef struct _PidginMiniDialogPrivate {
    GtkImage *icon;
    GtkBox *title_box;
    GtkLabel *title;
    GtkLabel *desc;
    GtkBox *buttons;

    guint idle_destroy_cb_id;
} PidginMiniDialogPrivate;

#define PIDGIN_MINI_DIALOG_GET_PRIVATE(dialog) \
	((PidginMiniDialogPrivate *) ((dialog)->priv))

PidginMiniDialog *
pidgin_mini_dialog_new(const gchar *title,
                       const gchar *description,
                       const gchar *icon_name)
{
    PidginMiniDialog *mini_dialog = g_object_new(PIDGIN_TYPE_MINI_DIALOG,
                                    "title", title,
                                    "description", description,
                                    "icon-name", icon_name,
                                    NULL);

    return mini_dialog;
}

void
pidgin_mini_dialog_set_title(PidginMiniDialog *mini_dialog,
                             const char *title)
{
    g_object_set(G_OBJECT(mini_dialog), "title", title, NULL);
}

void
pidgin_mini_dialog_set_description(PidginMiniDialog *mini_dialog,
                                   const char *description)
{
    g_object_set(G_OBJECT(mini_dialog), "description", description, NULL);
}

void
pidgin_mini_dialog_set_icon_name(PidginMiniDialog *mini_dialog,
                                 const char *icon_name)
{
    g_object_set(G_OBJECT(mini_dialog), "icon_name", icon_name, NULL);
}

struct _mini_dialog_button_clicked_cb_data {
    PidginMiniDialog *mini_dialog;
    PidginMiniDialogCallback callback;
    gpointer user_data;
};

guint
pidgin_mini_dialog_get_num_children(PidginMiniDialog *mini_dialog)
{
    return g_list_length(gtk_container_get_children(GTK_CONTAINER(mini_dialog->contents)));
}

static gboolean
idle_destroy_cb(GtkWidget *mini_dialog)
{
    gtk_widget_destroy(mini_dialog);
    return FALSE;
}

static void
mini_dialog_button_clicked_cb(GtkButton *button,
                              gpointer user_data)
{
    struct _mini_dialog_button_clicked_cb_data *data = user_data;
    PidginMiniDialogPrivate *priv =
        PIDGIN_MINI_DIALOG_GET_PRIVATE(data->mini_dialog);

    /* Set up the destruction callback before calling the clicked callback,
     * so that if the mini-dialog gets destroyed during the clicked callback
     * the idle_destroy_cb is correctly removed by _finalize.
     */
    priv->idle_destroy_cb_id =
        g_idle_add((GSourceFunc) idle_destroy_cb, data->mini_dialog);

    if (data->callback != NULL)
        data->callback(data->mini_dialog, button, data->user_data);

}

static void
mini_dialog_button_destroy_cb(G_GNUC_UNUSED GtkButton *button,
                              gpointer user_data)
{
    struct _mini_dialog_button_clicked_cb_data *data = user_data;
    g_free(data);
}

void
pidgin_mini_dialog_add_button(PidginMiniDialog *self,
                              const char *text,
                              PidginMiniDialogCallback clicked_cb,
                              gpointer user_data)
{
    PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);
    struct _mini_dialog_button_clicked_cb_data *callback_data
    = g_new0(struct _mini_dialog_button_clicked_cb_data, 1);
    GtkWidget *button = gtk_button_new();
    GtkWidget *label = gtk_label_new(NULL);
    char *button_text =
        g_strdup_printf("<span size=\"smaller\">%s</span>", text);

    gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), button_text);
    g_free(button_text);

    callback_data->mini_dialog = self;
    callback_data->callback = clicked_cb;
    callback_data->user_data = user_data;
    g_signal_connect(G_OBJECT(button), "clicked",
                     (GCallback) mini_dialog_button_clicked_cb, callback_data);
    g_signal_connect(G_OBJECT(button), "destroy",
                     (GCallback) mini_dialog_button_destroy_cb, callback_data);

    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    gtk_container_add(GTK_CONTAINER(button), label);

    gtk_box_pack_end(GTK_BOX(priv->buttons), button, FALSE, FALSE,
                     0);
    gtk_widget_show_all(GTK_WIDGET(button));
}

static void
pidgin_mini_dialog_get_property(GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
    PidginMiniDialog *self = PIDGIN_MINI_DIALOG(object);
    PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);

    switch (property_id) {
        case PROP_TITLE:
            g_value_set_string(value, gtk_label_get_text(priv->title));
            break;
        case PROP_DESCRIPTION:
            g_value_set_string(value, gtk_label_get_text(priv->desc));
            break;
        case PROP_ICON_NAME: {
            gchar *icon_name = NULL;
            GtkIconSize size;
            gtk_image_get_stock(priv->icon, &icon_name, &size);
            g_value_set_string(value, icon_name);
            break;
        }
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
mini_dialog_set_title(PidginMiniDialog *self,
                      const char *title)
{
    PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);

    char *title_esc = g_markup_escape_text(title, -1);
    char *title_markup = g_strdup_printf(
                             "<span weight=\"bold\" size=\"smaller\">%s</span>",
                             title_esc ? title_esc : "");

    gtk_label_set_markup(priv->title, title_markup);

    g_free(title_esc);
    g_free(title_markup);
}

static void
mini_dialog_set_description(PidginMiniDialog *self,
                            const char *description)
{
    PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);

    if (description) {
        char *desc_esc = g_markup_escape_text(description, -1);
        char *desc_markup = g_strdup_printf(
                                "<span size=\"smaller\">%s</span>", desc_esc);

        gtk_label_set_markup(priv->desc, desc_markup);

        g_free(desc_esc);
        g_free(desc_markup);

        gtk_widget_show(GTK_WIDGET(priv->desc));
        g_object_set(G_OBJECT(priv->desc), "no-show-all", FALSE, NULL);
    } else {
        gtk_label_set_text(priv->desc, NULL);
        gtk_widget_hide(GTK_WIDGET(priv->desc));
        /* make calling show_all() on the minidialog not affect desc
         * even though it's packed inside it.
          */
        g_object_set(G_OBJECT(priv->desc), "no-show-all", TRUE, NULL);
    }
}

static void
pidgin_mini_dialog_set_property(GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
    PidginMiniDialog *self = PIDGIN_MINI_DIALOG(object);
    PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);

    switch (property_id) {
        case PROP_TITLE:
            mini_dialog_set_title(self, g_value_get_string(value));
            break;
        case PROP_DESCRIPTION:
            mini_dialog_set_description(self, g_value_get_string(value));
            break;
        case PROP_ICON_NAME:
            gtk_image_set_from_stock(priv->icon, g_value_get_string(value),
                                     GTK_ICON_SIZE_BUTTON);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
pidgin_mini_dialog_finalize(GObject *object)
{
    PidginMiniDialog *self = PIDGIN_MINI_DIALOG(object);
    PidginMiniDialogPrivate *priv = PIDGIN_MINI_DIALOG_GET_PRIVATE(self);

    if (priv->idle_destroy_cb_id)
        g_source_remove(priv->idle_destroy_cb_id);

    g_free(priv);
    self->priv = NULL;


    G_OBJECT_CLASS(pidgin_mini_dialog_parent_class)->finalize(object);
}

static void
pidgin_mini_dialog_class_init(PidginMiniDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    object_class->get_property = pidgin_mini_dialog_get_property;
    object_class->set_property = pidgin_mini_dialog_set_property;
    object_class->finalize = pidgin_mini_dialog_finalize;

    param_spec = g_param_spec_string("title", "title",
                                     "String specifying the mini-dialog's title", NULL,
                                     G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
                                     G_PARAM_READWRITE);
    g_object_class_install_property(object_class, PROP_TITLE, param_spec);

    param_spec = g_param_spec_string("description", "description",
                                     "Description text for the mini-dialog, if desired", NULL,
                                     G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
                                     G_PARAM_READWRITE);
    g_object_class_install_property(object_class, PROP_DESCRIPTION, param_spec);

    param_spec = g_param_spec_string("icon-name", "icon-name",
                                     "String specifying the Gtk stock name of the dialog's icon",
                                     NULL,
                                     G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
                                     G_PARAM_READWRITE);
    g_object_class_install_property(object_class, PROP_ICON_NAME, param_spec);
}

/* 16 is the width of the icon, due to PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL */
#define BLIST_WIDTH_OTHER_THAN_LABEL \
	((6 * 3) + 16)

#define BLIST_WIDTH_PREF \
	(PIDGIN_PREFS_ROOT "/blist/width")

static void
pidgin_mini_dialog_init(PidginMiniDialog *self)
{
    GtkBox *self_box = GTK_BOX(self);
    guint label_width = LABEL_WIDTH;

    PidginMiniDialogPrivate *priv = g_new0(PidginMiniDialogPrivate, 1);
    self->priv = priv;

    gtk_container_set_border_width(GTK_CONTAINER(self), HIG_BOX_SPACE);

    priv->title_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, HIG_BOX_SPACE));

    priv->icon = GTK_IMAGE(gtk_image_new());
    gtk_misc_set_alignment(GTK_MISC(priv->icon), 0, 0);

    priv->title = GTK_LABEL(gtk_label_new(NULL));
    gtk_widget_set_size_request(GTK_WIDGET(priv->title), label_width, -1);
    gtk_label_set_line_wrap(priv->title, TRUE);
    gtk_label_set_selectable(priv->title, TRUE);
    gtk_misc_set_alignment(GTK_MISC(priv->title), 0, 0);

    gtk_box_pack_start(priv->title_box, GTK_WIDGET(priv->icon), FALSE, FALSE, 0);
    gtk_box_pack_start(priv->title_box, GTK_WIDGET(priv->title), TRUE, TRUE, 0);

    priv->desc = GTK_LABEL(gtk_label_new(NULL));
    gtk_widget_set_size_request(GTK_WIDGET(priv->desc), label_width, -1);
    gtk_label_set_line_wrap(priv->desc, TRUE);
    gtk_misc_set_alignment(GTK_MISC(priv->desc), 0, 0);
    gtk_label_set_selectable(priv->desc, TRUE);
    /* make calling show_all() on the minidialog not affect desc even though
     * it's packed inside it.
     */
    g_object_set(G_OBJECT(priv->desc), "no-show-all", TRUE, NULL);

    self->contents = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

    priv->buttons = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

    gtk_box_pack_start(self_box, GTK_WIDGET(priv->title_box), FALSE, FALSE, 0);
    gtk_box_pack_start(self_box, GTK_WIDGET(priv->desc), FALSE, FALSE, 0);
    gtk_box_pack_start(self_box, GTK_WIDGET(self->contents), TRUE, TRUE, 0);
    gtk_box_pack_start(self_box, GTK_WIDGET(priv->buttons), FALSE, FALSE, 0);

    gtk_orientable_set_orientation(GTK_ORIENTABLE (self),
                                   GTK_ORIENTATION_VERTICAL);

    gtk_widget_show_all(GTK_WIDGET(self));
}
