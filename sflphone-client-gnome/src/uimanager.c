/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com
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

#include <menus.h>
#include <config.h>
#include <preferencesdialog.h>
#include <dbus/dbus.h>
#include <mainwindow.h>
#include <assistant.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <libgnome/gnome-help.h>

static void help_contents_cb (GtkAction *action,  GSRWindow *window)
{
    GError *error = NULL;
    gnome_help_display ("gnome-sound-recorder.xml", NULL, &error);
    if (error != NULL) {
        g_warning ("%s", error->message);
        g_error_free (error);
    }
}

static const GtkActionEntry menu_entries[] = 
{
    /* Call Menu */
    { "Call", NULL, _("_Call") },
    { "NewCall", GTK_STOCK_NEW, _("_New call"), "<control>N", _("Place a new call"), G_CALLBACK (call_new_call) },
    { "PickUp", GTK_STOCK_NEW, _("_Pick up"), NULL, _("Answer the call"), G_CALLBACK (call_pick_up) },
    { "HangUp", GTK_STOCK_NEW, _("_Hang up"), GDK_Escape, _("Finish the call"), G_CALLBACK (call_hang_up) },    
    { "OnHold", GTK_STOCK_NEW, _("O_n hold"), NULL, _("Place the call on hold"), G_CALLBACK (call_hold) },    
    { "Record", GTK_STOCK_MEDIA_RECORD, _("_Record"), "<control>R", _("Record the current conversation"), G_CALLBACK (call_record) },        
    { "OnHold", NULL, _("Configuration _Assistant"), NULL, _("Run the configuration assistant"), G_CALLBACK (call_wizard) },    
    { "Close", GTK_STOCK_CLOSE, _("_Close"), "<control>W", _("Minimize to system tray"), G_CALLBACK (call_minimize) },
    { "Quit", GTK_STOCK_CLOSE, _("_Quit"), "<control>Q", _("Quit the program"), G_CALLBACK (call_quit) },   
    
    /* Edit Menu */
    { "Edit", NULL, _("_Edit") },
    { "Copy", GTK_STOCK_COPY, _("_Copy"), "<control>C", _("Copy the selection"), G_CALLBACK (edit_copy) },
    { "Paste", GTK_STOCK_PASTE, _("_Paste"), "<control>V", _("Paste the clipboard"), G_CALLBACK (edit_paste) },
    { "ClearHistory", GTK_STOCK_CLEAR, _("Clear _history"), NULL, _("Clear the call history"), G_CALLBACK (clear_history) },
    { "Accounts", NULL, _("_Accounts"), NULL, _("Edit your accounts"), G_CALLBACK (edit_accounts) },
    { "Preferences", GTK_STOCK_PREFERENCES, _("_Preferences"), NULL, _("Change your preferences"), G_CALLBACK (edit_preferences) },   
    
    /* Help menu */
    { "Help", NULL, N_("_Help") },
    {"HelpContents", GTK_STOCK_HELP, N_("Contents"), "F1", N_("Open the manual"), G_CALLBACK (help_contents_cb) },
    { "About", GTK_STOCK_ABOUT, NULL, NULL,  N_("About this application"), G_CALLBACK (help_about) }
         
}
