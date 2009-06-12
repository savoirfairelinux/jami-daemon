/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#include <gtk/gtk.h>

#include <accountlist.h>
#include <accountwindow.h>
#include <actions.h>
#include <config.h>
#include <toolbar.h>
#include <dbus/dbus.h>
#include <mainwindow.h>
#include <audioconf.h>
#include <addressbook-config.h>
#include <hooks-config.h>
#include <utils.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/**
 * Local variables
 */
gboolean accDialogOpen = FALSE;
gboolean dialogOpen = FALSE;
gboolean ringtoneEnabled = TRUE;

GtkListStore *accountStore;
// instead of keeping selected codec as a variable
GtkWidget *addButton;
GtkWidget *editButton;
GtkWidget *deleteButton;
GtkWidget *restoreButton;
GtkWidget *accountMoveDownButton;
GtkWidget *accountMoveUpButton;

/* STUN configuration part */
GtkWidget * stunEnable;
GtkWidget * stunFrame;
GtkWidget * stunServer;
GtkWidget * applyButton;
GtkWidget *history_value;

GtkWidget* status;

static int history_limit;
static gboolean history_enabled = TRUE;

account_t *selectedAccount;

// Account properties
enum {
    COLUMN_ACCOUNT_ALIAS,
    COLUMN_ACCOUNT_TYPE,
    COLUMN_ACCOUNT_STATUS,
    COLUMN_ACCOUNT_ACTIVE,
    COLUMN_ACCOUNT_DATA,
    COLUMN_ACCOUNT_COUNT
};

// Mail notification
GtkWidget * widg;



/**
 * Fills the treelist with accounts
 */
    void
config_window_fill_account_list()
{
    
    if(accDialogOpen)
    {
        GtkTreeIter iter;

        gtk_list_store_clear(accountStore);
        unsigned int i;
        for(i = 0; i < account_list_get_size(); i++)
        {
            account_t * a = account_list_get_nth (i);
	    
            if (a)
            {

                gtk_list_store_append (accountStore, &iter);
                gtk_list_store_set(accountStore, &iter,
                        COLUMN_ACCOUNT_ALIAS, g_hash_table_lookup(a->properties, ACCOUNT_ALIAS),  // Name
                        COLUMN_ACCOUNT_TYPE, g_hash_table_lookup(a->properties, ACCOUNT_TYPE),   // Protocol
                        COLUMN_ACCOUNT_STATUS, account_state_name(a->state),      // Status
                        COLUMN_ACCOUNT_ACTIVE, (g_strcasecmp(g_hash_table_lookup(a->properties, ACCOUNT_ENABLED),"TRUE") == 0)? TRUE:FALSE,  // Enable/Disable
                        COLUMN_ACCOUNT_DATA, a,   // Pointer
                        -1);
            }
        }

        gtk_widget_set_sensitive( GTK_WIDGET(editButton),   FALSE);
        gtk_widget_set_sensitive( GTK_WIDGET(deleteButton), FALSE);
    }
}

/**
 * Delete an account
 */
    static void
delete_account(GtkWidget *widget UNUSED, gpointer data UNUSED)
{
    if(selectedAccount)
    {
        dbus_remove_account(selectedAccount->accountID);
        if(account_list_get_sip_account_number() == 1 &&
                strcmp(g_hash_table_lookup(selectedAccount->properties, ACCOUNT_TYPE), "SIP")==0 )
            gtk_widget_set_sensitive(GTK_WIDGET(stunFrame), FALSE);
    }
}

/**
 * Edit an account
 */
    static void
edit_account(GtkWidget *widget UNUSED, gpointer data UNUSED)
{
    if(selectedAccount)
    {
        show_account_window(selectedAccount);
    }
}

/**
 * Add an account
 */
    static void
add_account(GtkWidget *widget UNUSED, gpointer data UNUSED)
{
    show_account_window(NULL);
}

    void
start_hidden( void )
{
    dbus_start_hidden();
}

    void
set_popup_mode( void )
{
    dbus_switch_popup_mode();
}

    void
set_notif_level(  )
{
    dbus_set_notify();

    if (dbus_get_notify())
      gtk_widget_set_sensitive(widg, TRUE);
    else {
      gtk_widget_set_sensitive(widg, FALSE);
      if (dbus_get_mail_notify())
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(widg), FALSE);
    }
}

    void
set_mail_notif( )
{
    dbus_set_mail_notify( );
}

static void history_limit_cb (GtkSpinButton *button, void *ptr)
{
    history_limit = gtk_spin_button_get_value_as_int((GtkSpinButton *)(ptr));
}

static void history_enabled_cb (GtkWidget *widget)
{
    history_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
    gtk_widget_set_sensitive (GTK_WIDGET (history_value), history_enabled);
        
    // Toggle it through D-Bus
    dbus_set_history_enabled ();
}


    void
clean_history( void )
{
    calllist_clean_history();
}

/**
 * Call back when the user click on an account in the list
 */
    static void
select_account(GtkTreeSelection *selection, GtkTreeModel *model)
{
    GtkTreeIter iter;
    GValue val;

    memset (&val, 0, sizeof(val));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        selectedAccount = NULL;
        gtk_widget_set_sensitive(GTK_WIDGET(accountMoveUpButton), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(accountMoveDownButton), FALSE);
        return;
    }

    // The Gvalue will be initialized in the following function
    gtk_tree_model_get_value(model, &iter, COLUMN_ACCOUNT_DATA, &val);

    selectedAccount = (account_t*)g_value_get_pointer(&val);
    g_value_unset(&val);

    if(selectedAccount)
    {
        gtk_widget_set_sensitive(GTK_WIDGET(editButton), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(deleteButton), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(accountMoveUpButton), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(accountMoveDownButton), TRUE);
    }
    DEBUG("select");
}

    static void
enable_account(GtkCellRendererToggle *rend UNUSED, gchar* path,  gpointer data )
{
    GtkTreeIter iter;
    GtkTreePath *treePath;
    GtkTreeModel *model;
    gboolean enable;
    account_t* acc ;

    // Get path of clicked codec active toggle box
    treePath = gtk_tree_path_new_from_string(path);
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(data));
    gtk_tree_model_get_iter(model, &iter, treePath);

    // Get pointer on object
    gtk_tree_model_get(model, &iter,
            COLUMN_ACCOUNT_ACTIVE, &enable,
            COLUMN_ACCOUNT_DATA, &acc,
            -1);
    enable = !enable;

    // Store value
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
            COLUMN_ACCOUNT_ACTIVE, enable,
            -1);

    gtk_tree_path_free(treePath);

    // Modify account state
    g_hash_table_replace( acc->properties , g_strdup(ACCOUNT_ENABLED) , g_strdup((enable == 1)? "TRUE":"FALSE"));

    dbus_send_register( acc->accountID , enable );
}

/**
 * Move account in list depending on direction and selected account
 */
    static void
account_move(gboolean moveUp, gpointer data)
{
    GtkTreeIter iter;
    GtkTreeIter *iter2;
    GtkTreeView *treeView;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreePath *treePath;
    gchar *path;

    // Get view, model and selection of codec store
    treeView = GTK_TREE_VIEW(data);
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeView));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));

    // Find selected iteration and create a copy
    gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), &model, &iter);
    iter2 = gtk_tree_iter_copy(&iter);

    // Find path of iteration
    path = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(model), &iter);
    treePath = gtk_tree_path_new_from_string(path);
    gint *indices = gtk_tree_path_get_indices(treePath);
    gint indice = indices[0];

    // Depending on button direction get new path
    if(moveUp)
        gtk_tree_path_prev(treePath);
    else
        gtk_tree_path_next(treePath);
    gtk_tree_model_get_iter(model, &iter, treePath);

    // Swap iterations if valid
    if(gtk_list_store_iter_is_valid(GTK_LIST_STORE(model), &iter))
        gtk_list_store_swap(GTK_LIST_STORE(model), &iter, iter2);

    // Scroll to new position
    gtk_tree_view_scroll_to_cell(treeView, treePath, NULL, FALSE, 0, 0);

    // Free resources
    gtk_tree_path_free(treePath);
    gtk_tree_iter_free(iter2);
    g_free(path);

    // Perpetuate changes in account queue
    if(moveUp)
        account_list_move_up(indice);
    else
        account_list_move_down(indice);


    // Set the order in the configuration file
    dbus_set_accounts_order (account_list_get_ordered_list ());
}

/**
 * Called from move up account button signal
 */
    static void
account_move_up(GtkButton *button UNUSED, gpointer data)
{
    // Change tree view ordering and get indice changed
    account_move(TRUE, data);
}

/**
 * Called from move down account button signal
 */
    static void
account_move_down(GtkButton *button UNUSED, gpointer data)
{
    // Change tree view ordering and get indice changed
    account_move(FALSE, data);
}

    static void
set_pulse_app_volume_control( void )
{
    dbus_set_pulse_app_volume_control();
}

static void update_port( GtkSpinButton *button UNUSED, void *ptr )
{
    dbus_set_sip_port(gtk_spin_button_get_value_as_int((GtkSpinButton *)(ptr)));
}

/**
 * Account settings tab
 */
    GtkWidget *
create_accounts_tab()
{
    GtkWidget *ret;
    GtkWidget *scrolledWindow;
    GtkWidget *treeView;
    GtkWidget *buttonBox;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *treeViewColumn;
    GtkTreeSelection *treeSelection;

    selectedAccount = NULL;

    ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER (ret), 10);

    scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(ret), scrolledWindow, TRUE, TRUE, 0);

    accountStore = gtk_list_store_new(COLUMN_ACCOUNT_COUNT,
            G_TYPE_STRING,  // Name
            G_TYPE_STRING,  // Protocol
            G_TYPE_STRING,  // Status
            G_TYPE_BOOLEAN, // Enabled / Disabled
            G_TYPE_POINTER  // Pointer to the Object
            );

    treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(accountStore));
    treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW (treeView));
    g_signal_connect(G_OBJECT (treeSelection), "changed",
            G_CALLBACK (select_account),
            accountStore);

    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes ("Alias",
            renderer,
            "markup", COLUMN_ACCOUNT_ALIAS,
            NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW(treeView), treeViewColumn);

    // A double click on the account line opens the window to edit the account
    g_signal_connect( G_OBJECT( treeView ) , "row-activated" , G_CALLBACK( edit_account ) , NULL );

    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes (_("Protocol"),
            renderer,
            "markup", COLUMN_ACCOUNT_TYPE,
            NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW(treeView), treeViewColumn);

    renderer = gtk_cell_renderer_text_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes (_("Status"),
            renderer,
            "markup", COLUMN_ACCOUNT_STATUS,
            NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW(treeView), treeViewColumn);

    renderer = gtk_cell_renderer_toggle_new();
    treeViewColumn = gtk_tree_view_column_new_with_attributes("", renderer, "active", COLUMN_ACCOUNT_ACTIVE , NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), treeViewColumn);
    g_signal_connect( G_OBJECT(renderer) , "toggled" , G_CALLBACK(enable_account), (gpointer)treeView );

    g_object_unref(G_OBJECT(accountStore));
    gtk_container_add(GTK_CONTAINER(scrolledWindow), treeView);

    // Create button box
    buttonBox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(buttonBox), 10);
    gtk_box_pack_start(GTK_BOX(ret), buttonBox, FALSE, FALSE, 0);

    accountMoveUpButton = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
    gtk_widget_set_sensitive(GTK_WIDGET(accountMoveUpButton), FALSE);
    gtk_box_pack_start(GTK_BOX(buttonBox), accountMoveUpButton, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(accountMoveUpButton), "clicked", G_CALLBACK(account_move_up), treeView);

    accountMoveDownButton = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
    gtk_widget_set_sensitive(GTK_WIDGET(accountMoveDownButton), FALSE);
    gtk_box_pack_start(GTK_BOX(buttonBox), accountMoveDownButton, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(accountMoveDownButton), "clicked", G_CALLBACK(account_move_down), treeView);

    /* The buttons to press! */
    buttonBox = gtk_hbutton_box_new();
    gtk_box_set_spacing(GTK_BOX(buttonBox), 10); //GAIM_HIG_BOX_SPACE
    gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_SPREAD);
    gtk_box_pack_start(GTK_BOX(ret), buttonBox, FALSE, FALSE, 0);
    gtk_widget_show (buttonBox);

    addButton = gtk_button_new_from_stock (GTK_STOCK_ADD);
    g_signal_connect_swapped(G_OBJECT(addButton), "clicked",
            G_CALLBACK(add_account), NULL);
    gtk_box_pack_start(GTK_BOX(buttonBox), addButton, FALSE, FALSE, 0);
    gtk_widget_show(addButton);

    editButton = gtk_button_new_from_stock (GTK_STOCK_EDIT);
    g_signal_connect_swapped(G_OBJECT(editButton), "clicked",
            G_CALLBACK(edit_account), NULL);
    gtk_box_pack_start(GTK_BOX(buttonBox), editButton, FALSE, FALSE, 0);
    gtk_widget_show(editButton);

    deleteButton = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
    g_signal_connect_swapped(G_OBJECT(deleteButton), "clicked",
            G_CALLBACK(delete_account), stunFrame);
    gtk_box_pack_start(GTK_BOX(buttonBox), deleteButton, FALSE, FALSE, 0);
    gtk_widget_show(deleteButton);

    gtk_widget_show_all(ret);

    config_window_fill_account_list();

    return ret;
}

void stun_state( void )
{

    guint stun_enabled = 0;

    gboolean stunActive = (gboolean)gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( stunEnable ));
    gtk_widget_set_sensitive( GTK_WIDGET( stunServer ) , stunActive );

    // Check if we actually change the state
    stun_enabled = dbus_stun_is_enabled();

    if( (stunActive && stun_enabled ==0 ) || (!stunActive && stun_enabled ==1))
    {
        gtk_widget_set_sensitive( GTK_WIDGET( applyButton ) , TRUE );
    }
    else
        gtk_widget_set_sensitive( GTK_WIDGET( applyButton ) , FALSE );

}

void update_registration( void )
{
    dbus_set_stun_server((gchar *)gtk_entry_get_text(GTK_ENTRY(stunServer)));
    dbus_enable_stun();

    gtk_widget_set_sensitive(GTK_WIDGET( applyButton ) , FALSE );
}

GtkWidget* create_stun_tab()
{
    GtkWidget * tableNat;
    gchar * stun_server= "stun.sflphone.org:3478";
    gchar * stun_enabled = "FALSE";
    GtkWidget * label;

    /* Retrieve the STUN configuration */
    stun_enabled = (dbus_stun_is_enabled()==1)?"TRUE":"FALSE";
    stun_server = dbus_get_stun_server();

    tableNat = gtk_table_new ( 3, 2  , FALSE/* homogeneous */);

    // NAT detection code section
    label = gtk_label_new(_("Stun parameters will apply to each SIP account created."));
    gtk_table_attach( GTK_TABLE( tableNat ), label, 0, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC (label), 0.5, 0.5);

    stunEnable = gtk_check_button_new_with_mnemonic(_("E_nable STUN"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stunEnable), strcmp(stun_enabled,"TRUE") == 0 ? TRUE: FALSE);
    g_signal_connect( G_OBJECT (GTK_TOGGLE_BUTTON(stunEnable)) , "toggled" , G_CALLBACK( stun_state ), NULL);
#if GTK_CHECK_VERSION(2,12,0)
    gtk_widget_set_tooltip_text( GTK_WIDGET( stunEnable ) , _("Enable it if you are behind a firewall"));
#endif
    gtk_table_attach ( GTK_TABLE( tableNat ), stunEnable, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    stunServer = gtk_entry_new();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), stunServer);
    gtk_entry_set_text(GTK_ENTRY(stunServer), stun_server);
#if GTK_CHECK_VERSION(2,12,0)
    gtk_widget_set_tooltip_text( GTK_WIDGET( stunServer ) , _("Format: name.server:port"));
#endif
    gtk_table_attach ( GTK_TABLE( tableNat ), stunServer, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive( GTK_WIDGET( stunServer ), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(stunEnable)));

    applyButton = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    gtk_widget_set_size_request( GTK_WIDGET(applyButton), 100, 30);
    gtk_widget_set_sensitive( GTK_WIDGET( applyButton ), FALSE );
    gtk_table_attach ( GTK_TABLE( tableNat ), applyButton, 0, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    g_signal_connect( G_OBJECT( applyButton) , "clicked" , update_registration , NULL);

    gtk_widget_show_all(tableNat);
    gtk_container_set_border_width (GTK_CONTAINER(tableNat), 15);

    return tableNat;
}

    GtkWidget*
create_general_settings ()
{

    int curPort;
    int n;

    GtkWidget *ret;

    GtkWidget *notifAll;
    // GtkWidget *widg;

    GtkWidget *mutewidget;
    GtkWidget *trayItem;
    GtkWidget *frame;
    GtkWidget *history_w;
    GtkWidget *label;
    GtkWidget *entryPort;
    GtkWidget *table;

    // Load history configuration
    history_load_configuration ();

    // Main widget
    ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    // Notifications Frame
    gnome_main_section_new_with_table (_("Desktop Notifications"), &frame, &table, 2, 1);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    // Notification All
    notifAll = gtk_check_button_new_with_mnemonic( _("_Enable notifications"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(notifAll), dbus_get_notify() ); 
    g_signal_connect(G_OBJECT( notifAll ) , "clicked" , G_CALLBACK( set_notif_level ) , NULL );
    gtk_table_attach( GTK_TABLE(table), notifAll, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    // Notification
    widg = gtk_check_button_new_with_mnemonic(  _("Enable voicemail _notifications"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(widg), dbus_get_mail_notify() );
    g_signal_connect(G_OBJECT( widg ) , "clicked" , G_CALLBACK( set_mail_notif ) , NULL);
    gtk_table_attach( GTK_TABLE(table), widg, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    

    if (dbus_get_notify())
       gtk_widget_set_sensitive(widg, TRUE);
    else
       gtk_widget_set_sensitive(widg, FALSE);

    // System Tray option frame
    gnome_main_section_new_with_table (_("System Tray Icon"), &frame, &table, 3, 1);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    GtkWidget* trayItem1 = gtk_radio_button_new_with_mnemonic(NULL,  _("_Popup main window on incoming call"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(trayItem1), dbus_popup_mode() );
    g_signal_connect(G_OBJECT( trayItem1 ), "clicked", G_CALLBACK( set_popup_mode ) , NULL);
    gtk_table_attach( GTK_TABLE(table), trayItem1, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    trayItem = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(trayItem1), _("Ne_ver popup main window"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(trayItem), !dbus_popup_mode() );
    gtk_table_attach( GTK_TABLE(table), trayItem, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    trayItem = gtk_check_button_new_with_mnemonic(_("Hide SFLphone window on _startup"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(trayItem), dbus_is_start_hidden() );
    g_signal_connect(G_OBJECT( trayItem ) , "clicked" , G_CALLBACK( start_hidden ) , NULL);
    gtk_table_attach( GTK_TABLE(table), trayItem, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    // HISTORY CONFIGURATION
    gnome_main_section_new_with_table (_("Calls History"), &frame, &table, 3, 1);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    history_w = gtk_check_button_new_with_mnemonic(_("_Keep my history for at least"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (history_w), history_enabled);
    g_signal_connect (G_OBJECT (history_w) , "clicked" , G_CALLBACK (history_enabled_cb) , NULL);
    gtk_table_attach( GTK_TABLE(table), history_w, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    
    history_value = gtk_spin_button_new_with_range(1, 99, 1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(history_value), history_limit);
    g_signal_connect( G_OBJECT (history_value) , "value-changed" , G_CALLBACK (history_limit_cb) , history_value);
    gtk_widget_set_sensitive (GTK_WIDGET (history_value), gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (history_w)));
    gtk_table_attach( GTK_TABLE(table), history_value, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5); 

    label = gtk_label_new(_(" days"));
    gtk_table_attach( GTK_TABLE(table), label, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    
    /** PULSEAUDIO CONFIGURATION */
    gnome_main_section_new_with_table (_("PulseAudio sound server"), &frame, &table, 1, 1);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    mutewidget = gtk_check_button_new_with_mnemonic(  _("_Mute other applications during a call"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(mutewidget), dbus_get_pulse_app_volume_control() );
    g_signal_connect(G_OBJECT( mutewidget ) , "clicked" , G_CALLBACK( set_pulse_app_volume_control ) , NULL);
    gtk_table_attach( GTK_TABLE(table), mutewidget, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    

    n = account_list_get_sip_account_number();
    DEBUG("sip account number = %i", n);

    /** SIP port information */
    curPort = dbus_get_sip_port();
    if(curPort <= 0 || curPort > 65535)
        curPort = 5060;

    gnome_main_section_new_with_table (_("SIP Port"), &frame, &table, 1, 3);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);
    // gtk_widget_show( frame );
    gtk_widget_set_sensitive( GTK_WIDGET(frame), (n==0)?FALSE:TRUE );

    // hbox = gtk_hbox_new(FALSE, 10);
    // gtk_widget_show( hbox );
    // gtk_container_add( GTK_CONTAINER(frame) , hbox);

    GtkWidget *applyButton = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    //gtk_widget_set_size_request(applyButton, 100, 35);
    //gtk_widget_set_sensitive( GTK_WIDGET(applyButton), (n==0)?FALSE:TRUE );

    label = gtk_label_new(_("Port:"));
    // gtk_misc_set_alignment(GTK_MISC(label), 0.03, 0.4);
    entryPort = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryPort);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(entryPort), curPort);
    g_signal_connect( G_OBJECT( applyButton) , "clicked" , G_CALLBACK( update_port ) , entryPort);

    gtk_table_attach( GTK_TABLE(table), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    gtk_table_attach( GTK_TABLE(table), entryPort, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    gtk_table_attach( GTK_TABLE(table), applyButton, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    gtk_widget_show_all(ret);

    return ret;
}

void
record_path_changed( GtkFileChooser *chooser , GtkLabel *label UNUSED)
{
    gchar* path;

    path = gtk_file_chooser_get_uri( GTK_FILE_CHOOSER( chooser ));
    dbus_set_record_path( path );
}

GtkWidget*
create_recording_settings ()
{

    GtkWidget *ret;
    GtkWidget *label;
    GtkWidget *table;
    GtkWidget *savePathFrame;
    GtkWidget *folderChooser;
    gchar *dftPath;

    /* Get the path where to save audio files */
    dftPath = dbus_get_record_path ();

    // Main widget
    ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    // Recorded file saving path
    gnome_main_section_new_with_table (_("General"), &savePathFrame, &table, 1, 2);
    gtk_box_pack_start(GTK_BOX(ret), savePathFrame, FALSE, FALSE, 5);

    // label
    label = gtk_label_new(_("Recordings folder"));
    gtk_table_attach( GTK_TABLE(table), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    // gtk_misc_set_alignment(GTK_MISC(label), 0.08, 0.5);


    // folder chooser button
    folderChooser = gtk_file_chooser_button_new(_("Select a folder"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER( folderChooser), dftPath);
    g_signal_connect( G_OBJECT( folderChooser ) , "selection_changed" , G_CALLBACK( record_path_changed ) , NULL );
    gtk_table_attach(GTK_TABLE(table), folderChooser, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    gtk_widget_show_all(ret);

    return ret;
}

void save_configuration_parameters (void) {

    // Address book config
    addressbook_config_save_parameters ();
    hooks_save_parameters ();

    // History config
    dbus_set_history_limit (history_limit);

}

/**
 * Show configuration window with tabs
 */
    void
show_config_window ()
{
    GtkDialog * dialog;
    GtkWidget * notebook;
    GtkWidget * tab;
    guint result;

    dialogOpen = TRUE;

    dialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Preferences"),
                GTK_WINDOW(get_main_window()),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_CLOSE,
                GTK_RESPONSE_ACCEPT,
                NULL));

    // Set window properties
    gtk_dialog_set_has_separator(dialog, FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 0);

    // Create tabs container
    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX (dialog->vbox), notebook, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 10);
    gtk_widget_show(notebook);

    // General settings tab
    tab = create_general_settings();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("General Settings")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // Audio tab
    tab = create_audio_configuration();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Audio Settings")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // Recording tab
    tab = create_recording_settings();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Recordings")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // Addressbook tab
    tab = create_addressbook_settings();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Address Book")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // HookS tab
    tab = create_hooks_settings();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Hooks")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    gtk_notebook_set_current_page( GTK_NOTEBOOK( notebook) ,  0);

    result = gtk_dialog_run(dialog);

    save_configuration_parameters ();
    toolbar_update_buttons();

    dialogOpen = FALSE;

    gtk_widget_destroy(GTK_WIDGET(dialog));
}

/*
 * Show accounts tab in a different window
 */
    void
show_accounts_window( void )
{
    GtkDialog * dialog;
    GtkWidget * accountFrame;
    GtkWidget * tab;

    accDialogOpen = TRUE;

    dialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Accounts"),
                GTK_WINDOW(get_main_window()),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_CLOSE,
                GTK_RESPONSE_ACCEPT,
                NULL));

    // Set window properties
    gtk_dialog_set_has_separator(dialog, FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 500);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 0);

    gnome_main_section_new (_("Configured Accounts"), &accountFrame);
    gtk_box_pack_start( GTK_BOX( dialog->vbox ), accountFrame , TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(accountFrame), 10);
    gtk_widget_show(accountFrame);

    // Accounts tab
    tab = create_accounts_tab();

    gtk_container_add(GTK_CONTAINER(accountFrame) , tab);

    // Stun Frame, displayed only if at least 1 SIP account is configured
    gnome_main_section_new (_("Network Address Translation"), &stunFrame);
    gtk_box_pack_start( GTK_BOX( dialog->vbox ), stunFrame , TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(stunFrame), 10);
    gtk_widget_show(stunFrame);

    gtk_container_add(GTK_CONTAINER(stunFrame), create_stun_tab());

    if( account_list_get_sip_account_number() == 0 )
    {
        gtk_widget_set_sensitive(GTK_WIDGET(stunFrame), FALSE);
    }

    gtk_dialog_run( dialog );

    status_bar_display_account ();

    accDialogOpen=FALSE;

    gtk_widget_destroy(GTK_WIDGET(dialog));
    toolbar_update_buttons();
}

void history_load_configuration ()
{
    history_limit = dbus_get_history_limit ();
    history_enabled = TRUE;
    if (dbus_get_history_enabled () == 0)
        history_enabled = FALSE;
}

void config_window_set_stun_visible()
{
    gtk_widget_set_sensitive( GTK_WIDGET(stunFrame), TRUE );
}


