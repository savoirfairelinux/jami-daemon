/*
 *  Copyright (C) 2007-2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#include <actions.h>
#include <mainwindow.h>
#include <accountlist.h>
#include <accountwindow.h>
#include <zrtpadvanceddialog.h>
#include <tlsadvanceddialog.h>

// From version 2.16, gtk provides the functionalities libsexy used to provide
#if GTK_CHECK_VERSION(2,16,0)
#else
#include <libsexy/sexy-icon-entry.h>
#endif

#include <string.h>
#include <dbus/dbus.h>
#include <config.h>
#include <gtk/gtk.h>


/** Local variables */
GtkDialog * dialog;
GtkWidget * hbox;
GtkWidget * label;
GtkWidget * entryID;
GtkWidget * entryAlias;
GtkWidget * entryProtocol;
GtkWidget * entryEnabled;
GtkWidget * entryUsername;
GtkWidget * entryHostname;
GtkWidget * entryPassword;
GtkWidget * entryMailbox;
GtkWidget * entryResolveNameOnlyOnce;
GtkWidget * entryExpire;
GtkListStore * credentialStore;
GtkWidget * deleteCredButton;
GtkWidget * treeViewCredential;
GtkWidget * scrolledWindowCredential;
GtkWidget * advancedZrtpButton;
GtkWidget * keyExchangeCombo;
        	
// Credentials
enum {
    COLUMN_CREDENTIAL_REALM,
    COLUMN_CREDENTIAL_USERNAME,
    COLUMN_CREDENTIAL_PASSWORD,
    COLUMN_CREDENTIAL_DATA,
    COLUMN_CREDENTIAL_COUNT
};

/* Signal to entryProtocol 'changed' */
	void
change_protocol (account_t * currentAccount UNUSED)
{
	(gchar *)gtk_combo_box_get_active_text(GTK_COMBO_BOX(entryProtocol));
}

	int
is_iax_enabled(void)
{
	int res = dbus_is_iax2_enabled();
	if(res == 1)
		return TRUE;
	else
		return FALSE;
}

static void update_credential_cb(GtkWidget *widget, gpointer data UNUSED)
{
    GtkTreeIter iter;
    gtk_tree_model_get_iter_from_string ((GtkTreeModel *) credentialStore, &iter, "0");
    gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "column"));
    gtk_list_store_set (GTK_LIST_STORE (credentialStore), &iter, column, (gchar *) gtk_entry_get_text(GTK_ENTRY(widget)), -1);
}

static GtkWidget * create_account_tab(account_t **a) 
{
	GtkWidget * frame;
	GtkWidget * table;
	account_t *currentAccount;
#if GTK_CHECK_VERSION(2,16,0)
#else
	GtkWidget *image;
#endif

	// Default settings
	gchar * curAccountID = "";
	gchar * curAccountEnabled = "TRUE";
	gchar * curAccountResolveOnce = "FALSE";
	gchar * curAccountType = "SIP";
	gchar * curAlias = "";
	gchar * curUsername = "";
	gchar * curHostname = "";
	gchar * curPassword = "";
	/* TODO: add curProxy, and add boxes for Proxy support */
	gchar * curMailbox = "";

	currentAccount = *a;

	// Load from SIP/IAX/Unknown ?
	if(currentAccount)
	{
		curAccountID = currentAccount->accountID;
		curAccountType = g_hash_table_lookup(currentAccount->properties, ACCOUNT_TYPE);
		curAccountEnabled = g_hash_table_lookup(currentAccount->properties, ACCOUNT_ENABLED);
		curAlias = g_hash_table_lookup(currentAccount->properties, ACCOUNT_ALIAS);
		curHostname = g_hash_table_lookup(currentAccount->properties, ACCOUNT_HOSTNAME);
		curPassword = g_hash_table_lookup(currentAccount->properties, ACCOUNT_PASSWORD);
		curUsername = g_hash_table_lookup(currentAccount->properties, ACCOUNT_USERNAME);
		curMailbox = g_hash_table_lookup(currentAccount->properties, ACCOUNT_MAILBOX);
		curAccountResolveOnce = g_hash_table_lookup(currentAccount->properties, ACCOUNT_RESOLVE_ONCE);
	}
	else
	{
		currentAccount = g_new0(account_t, 1);
		currentAccount->properties = g_hash_table_new(NULL, g_str_equal);
		curAccountID = "new";
	}

	gnome_main_section_new (_("Account Parameters"), &frame);
	gtk_widget_show(frame);

	table = gtk_table_new (9, 2  ,  FALSE/* homogeneous */);
	gtk_table_set_row_spacings( GTK_TABLE(table), 10);
	gtk_table_set_col_spacings( GTK_TABLE(table), 10);
	gtk_widget_show(table);
	gtk_container_add( GTK_CONTAINER( frame) , table );

#ifdef DEBUG
	label = gtk_label_new_with_mnemonic ("ID:");
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	entryID = gtk_entry_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryID);
	gtk_entry_set_text(GTK_ENTRY(entryID), curAccountID);
	gtk_widget_set_sensitive( GTK_WIDGET(entryID), FALSE);
	gtk_table_attach ( GTK_TABLE( table ), entryID, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
#endif

	entryEnabled = gtk_check_button_new_with_mnemonic(_("_Enable this account"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entryEnabled),
			g_strcasecmp(curAccountEnabled,"TRUE") == 0 ? TRUE: FALSE);
	gtk_table_attach ( GTK_TABLE( table ), entryEnabled, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_set_sensitive( GTK_WIDGET( entryEnabled ) , TRUE );

	label = gtk_label_new_with_mnemonic (_("_Alias"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	entryAlias = gtk_entry_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryAlias);
	gtk_entry_set_text(GTK_ENTRY(entryAlias), curAlias);
	gtk_table_attach ( GTK_TABLE( table ), entryAlias, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	label = gtk_label_new_with_mnemonic (_("_Protocol"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	entryProtocol = gtk_combo_box_new_text();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryProtocol);
	gtk_combo_box_append_text(GTK_COMBO_BOX(entryProtocol), "SIP");
	if( is_iax_enabled() ) gtk_combo_box_append_text(GTK_COMBO_BOX(entryProtocol), "IAX");
	if(strcmp(curAccountType, "SIP") == 0)
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(entryProtocol),0);
	}
	else if(strcmp(curAccountType, "IAX") == 0)
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(entryProtocol),1);
	}
	else
	{
		/* Should never come here, add debug message. */
		gtk_combo_box_append_text(GTK_COMBO_BOX(entryProtocol), _("Unknown"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(entryProtocol),2);
	}
	gtk_table_attach ( GTK_TABLE( table ), entryProtocol, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	/* Link signal 'changed' */
	g_signal_connect (G_OBJECT (GTK_COMBO_BOX(entryProtocol)), "changed",
			G_CALLBACK (change_protocol),
			currentAccount);

	label = gtk_label_new_with_mnemonic (_("_Host name"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	entryHostname = gtk_entry_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryHostname);
	gtk_entry_set_text(GTK_ENTRY(entryHostname), curHostname);
	gtk_table_attach ( GTK_TABLE( table ), entryHostname, 1, 2, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	label = gtk_label_new_with_mnemonic (_("_User name"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);	
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
#if GTK_CHECK_VERSION(2,16,0)
	entryUsername = gtk_entry_new();
	gtk_entry_set_icon_from_pixbuf (GTK_ENTRY (entryUsername), GTK_ENTRY_ICON_PRIMARY, gdk_pixbuf_new_from_file(ICONS_DIR "/stock_person.svg", NULL));
#else
	entryUsername = sexy_icon_entry_new();
	image = gtk_image_new_from_file( ICONS_DIR "/stock_person.svg" );
	sexy_icon_entry_set_icon( SEXY_ICON_ENTRY(entryUsername), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE(image) );
#endif
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryUsername);
	gtk_entry_set_text(GTK_ENTRY(entryUsername), curUsername);
	gtk_table_attach ( GTK_TABLE( table ), entryUsername, 1, 2, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    g_signal_connect(G_OBJECT (entryUsername), "changed", G_CALLBACK (update_credential_cb), NULL);
    g_object_set_data (G_OBJECT (entryUsername), "column", GINT_TO_POINTER (COLUMN_CREDENTIAL_USERNAME));

	label = gtk_label_new_with_mnemonic (_("_Password"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 7, 8, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
#if GTK_CHECK_VERSION(2,16,0)
	entryPassword = gtk_entry_new();
    GtkSettings *settings = gtk_settings_get_default ();
    g_object_set (G_OBJECT (settings), "gtk-entry-password-hint-timeout", 600, NULL);
	gtk_entry_set_icon_from_stock (GTK_ENTRY (entryPassword), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_DIALOG_AUTHENTICATION);
#else
	entryPassword = sexy_icon_entry_new();
	image = gtk_image_new_from_stock( GTK_STOCK_DIALOG_AUTHENTICATION , GTK_ICON_SIZE_SMALL_TOOLBAR );
	sexy_icon_entry_set_icon( SEXY_ICON_ENTRY(entryPassword), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE(image) );
#endif
	gtk_entry_set_visibility(GTK_ENTRY(entryPassword), FALSE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryPassword);
	gtk_entry_set_text(GTK_ENTRY(entryPassword), curPassword);
	gtk_table_attach ( GTK_TABLE( table ), entryPassword, 1, 2, 7, 8, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    g_signal_connect(G_OBJECT (entryPassword), "changed", G_CALLBACK (update_credential_cb), NULL);
    g_object_set_data (G_OBJECT (entryPassword), "column", GINT_TO_POINTER (COLUMN_CREDENTIAL_PASSWORD));
    
	label = gtk_label_new_with_mnemonic (_("_Voicemail number"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 8, 9, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	entryMailbox = gtk_entry_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryMailbox);
	gtk_entry_set_text(GTK_ENTRY(entryMailbox), curMailbox);
	gtk_table_attach ( GTK_TABLE( table ), entryMailbox, 1, 2, 8, 9, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	gtk_widget_show_all( table );
	gtk_container_set_border_width (GTK_CONTAINER(table), 10);

	*a = currentAccount;
	return frame;
}

static void fill_treeview_with_credential(GtkListStore * credentialStore, account_t * account) 
{
        GtkTreeIter iter;
        gtk_list_store_clear(credentialStore);
        gtk_list_store_append (credentialStore, &iter);

        /* This is the default, undeletable credential */
        gchar * authentication_name = g_hash_table_lookup(account->properties, ACCOUNT_AUTHENTICATION_USERNAME);
        gchar * realm = g_hash_table_lookup(account->properties, ACCOUNT_REALM);        
        if (realm == NULL || (g_strcmp0(realm, "") == 0)) {
            realm = g_strdup("*");
        }
        
        if((authentication_name == NULL) || (g_strcmp0(authentication_name, "") == 0)) {
            DEBUG("DEFAULT");
            gtk_list_store_set(credentialStore, &iter,
                    COLUMN_CREDENTIAL_REALM, realm, 
                    COLUMN_CREDENTIAL_USERNAME, gtk_entry_get_text(GTK_ENTRY(entryUsername)),
                    COLUMN_CREDENTIAL_PASSWORD, gtk_entry_get_text(GTK_ENTRY(entryPassword)),    
                    COLUMN_CREDENTIAL_DATA, account, 
                    -1);
        } else {
            gtk_list_store_set(credentialStore, &iter,
                    COLUMN_CREDENTIAL_REALM, g_hash_table_lookup(account->properties, ACCOUNT_REALM), 
                    COLUMN_CREDENTIAL_USERNAME, g_hash_table_lookup(account->properties, ACCOUNT_AUTHENTICATION_USERNAME),
                    COLUMN_CREDENTIAL_PASSWORD, gtk_entry_get_text(GTK_ENTRY(entryPassword)),    
                    COLUMN_CREDENTIAL_DATA, account, 
                    -1);
            g_signal_handlers_disconnect_by_func (G_OBJECT(entryUsername), G_CALLBACK(update_credential_cb), NULL);
        }
        
        if(account->credential_information == NULL) {
            DEBUG("No credential defined");
            return;
        }
        
        unsigned int i;
        for(i = 0; i < account->credential_information->len; i++)
        {	                    
            GHashTable * element = g_ptr_array_index(account->credential_information, i);               
            gtk_list_store_append (credentialStore, &iter);
            gtk_list_store_set(credentialStore, &iter,
                    COLUMN_CREDENTIAL_REALM, g_hash_table_lookup(element, ACCOUNT_REALM), 
                    COLUMN_CREDENTIAL_USERNAME, g_hash_table_lookup(element, ACCOUNT_USERNAME), 
                    COLUMN_CREDENTIAL_PASSWORD, g_hash_table_lookup(element, ACCOUNT_PASSWORD), 
                    COLUMN_CREDENTIAL_DATA, element, // Pointer
                    -1);
        }
}

static select_credential_cb(GtkTreeSelection *selection, GtkTreeModel *model)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    if(gtk_tree_selection_get_selected (selection, NULL, &iter)) {
        path = gtk_tree_model_get_path (model, &iter);
        if(gtk_tree_path_get_indices (path)[0] == 0) {
            gtk_widget_set_sensitive(GTK_WIDGET(deleteCredButton), FALSE);
        } else {
            gtk_widget_set_sensitive(GTK_WIDGET(deleteCredButton), TRUE);
        }
    }
}

static void add_credential_cb (GtkWidget *button, gpointer data)
{
    GtkTreeIter iter;
    GtkTreeModel *model = (GtkTreeModel *)data;
    GtkRequisition requisitionTreeView;
    GtkRequisition oldRequisitionTreeView;
 
    gtk_widget_size_request(GTK_WIDGET(treeViewCredential), &oldRequisitionTreeView);
           
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        COLUMN_CREDENTIAL_REALM, "*",
                        COLUMN_CREDENTIAL_USERNAME, _("Authentication"),
                        COLUMN_CREDENTIAL_PASSWORD, _("Secret"),
                        -1);
                        
    gtk_widget_size_request(GTK_WIDGET(treeViewCredential), &requisitionTreeView);
    gtk_widget_set_size_request(GTK_WIDGET(scrolledWindowCredential), oldRequisitionTreeView.width, requisitionTreeView.height + 20);
}

static void delete_credential_cb(GtkWidget *button, gpointer data)
{
    GtkTreeIter iter;
    GtkTreeView *treeview = (GtkTreeView *)data;
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
    GtkRequisition requisitionTreeView;
    GtkRequisition oldRequisitionTreeView;
  
    gtk_widget_size_request(GTK_WIDGET(treeViewCredential), &oldRequisitionTreeView);
          
    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        GtkTreePath *path;
        path = gtk_tree_model_get_path (model, &iter);
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        
        gtk_tree_path_free (path);
    }

    gtk_widget_size_request(GTK_WIDGET(treeViewCredential), &requisitionTreeView);
    gtk_widget_set_size_request(GTK_WIDGET(scrolledWindowCredential), oldRequisitionTreeView.width, requisitionTreeView.height + 20);

}

static void cell_edited_cb(GtkCellRendererText *renderer, gchar *path_desc, gchar *text, gpointer data)
{
    GtkTreeModel *model = (GtkTreeModel *)data;
    GtkTreePath *path = gtk_tree_path_new_from_string (path_desc);
    GtkTreeIter iter;
     
    gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "column"));
    
    if(g_strcasecmp(path_desc, "0") == 0) {
        if(g_strcasecmp(text, gtk_entry_get_text(GTK_ENTRY(entryUsername))) != 0) {
            g_signal_handlers_disconnect_by_func (G_OBJECT(entryUsername), G_CALLBACK(update_credential_cb), NULL);
        }
    }  
    
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, text, -1);
    gtk_tree_path_free (path);

}

static void editing_started_cb (GtkCellRenderer *cell, GtkCellEditable * editable, const gchar * path, gpointer data)
{
    DEBUG("Editing started");
    gtk_entry_set_visibility(GTK_ENTRY(editable), FALSE);
}

static void show_advanced_zrtp_options_cb(GtkWidget *widget UNUSED, gpointer data)
{
    DEBUG("Advanced options for ZRTP");
    show_advanced_zrtp_options((GHashTable *) data);
}

static void show_advanced_tls_options_cb(GtkWidget *widget UNUSED, gpointer data)
{
    DEBUG("Advanced options for TLS");
    show_advanced_tls_options((GHashTable *) data);
}

static void key_exchange_changed_cb(GtkWidget *widget, gpointer data)
{
    DEBUG("Key exchange changed");
    if (g_strcasecmp(gtk_combo_box_get_active_text(GTK_COMBO_BOX(keyExchangeCombo)), (gchar *) "ZRTP") == 0) {
        gtk_widget_set_sensitive(GTK_WIDGET(advancedZrtpButton), TRUE);
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(advancedZrtpButton), FALSE);
        
    }
}

GtkWidget * create_advanced_tab(account_t **a)
{
	GtkWidget * frame;
	GtkWidget * table;
	GtkWidget * label;
	GtkWidget * ret;
	GtkWidget * hbox;
	GtkWidget * editButton;
    GtkWidget * addButton;
	GtkCellRenderer * renderer;
    GtkTreeViewColumn * treeViewColumn;
    GtkTreeSelection * treeSelection;
    GtkRequisition requisitionTable;
    GtkRequisition requisitionTreeView;    

	
	ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);
    
	account_t * currentAccount;
	currentAccount = *a;
	
    gchar * curSRTPEnabled = NULL;
    gchar * curKeyExchange = NULL;
    gchar * curAccountResolveOnce = NULL;
    gchar * curAccountExpire = NULL;
       
	// Load from SIP/IAX/Unknown ?
	if(currentAccount) {
		curAccountResolveOnce = g_hash_table_lookup(currentAccount->properties, ACCOUNT_RESOLVE_ONCE);
		if (curAccountResolveOnce == NULL) {
		    curAccountResolveOnce = "FALSE";
		}
		curAccountExpire = g_hash_table_lookup(currentAccount->properties, ACCOUNT_REGISTRATION_EXPIRE);
		if (curAccountExpire == NULL) {
		    curAccountExpire = "600";
		}		
		
        curKeyExchange = g_hash_table_lookup(currentAccount->properties, ACCOUNT_KEY_EXCHANGE);
		if (curKeyExchange == NULL) {
		    curKeyExchange = "none";
		}		
		      		  
        curSRTPEnabled = g_hash_table_lookup(currentAccount->properties, ACCOUNT_SRTP_ENABLED);
        if (curSRTPEnabled == NULL) {
            curSRTPEnabled == "FALSE";
        }
	} 

    gnome_main_section_new_with_table (_("Registration Options"), &frame, &table, 2, 3);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic (_("Registration _expire"));
	gtk_table_attach_defaults( GTK_TABLE( table ), label, 0, 1, 0, 1);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	entryExpire = gtk_entry_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryExpire);
	gtk_entry_set_text(GTK_ENTRY(entryExpire), curAccountExpire);
	gtk_table_attach_defaults( GTK_TABLE( table ), entryExpire, 1, 2, 0, 1);

	entryResolveNameOnlyOnce = gtk_check_button_new_with_mnemonic(_("_Conform to RFC 3263"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entryResolveNameOnlyOnce),
			g_strcasecmp(curAccountResolveOnce,"FALSE") == 0 ? TRUE: FALSE);
	gtk_table_attach_defaults( GTK_TABLE( table ), entryResolveNameOnlyOnce, 0, 2, 1, 2);
	gtk_widget_set_sensitive( GTK_WIDGET( entryResolveNameOnlyOnce ) , TRUE );

    gtk_widget_show_all( table );
	gtk_container_set_border_width (GTK_CONTAINER(table), 10);

    gtk_widget_size_request(GTK_WIDGET(table), &requisitionTable);
    	
    /* Credentials tree view */
    gnome_main_section_new_with_table (_("Credential informations"), &frame, &table, 1, 1);
	gtk_container_set_border_width (GTK_CONTAINER(table), 10);
	gtk_table_set_row_spacings(GTK_TABLE(table), 10);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);
	
    scrolledWindowCredential = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindowCredential), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledWindowCredential), GTK_SHADOW_IN);
    gtk_table_attach_defaults (GTK_TABLE(table), scrolledWindowCredential, 0, 1, 0, 1);
    
    credentialStore = gtk_list_store_new(COLUMN_CREDENTIAL_COUNT,
            G_TYPE_STRING,  // Realm
            G_TYPE_STRING,  // Username
            G_TYPE_STRING,  // Password
            G_TYPE_POINTER  // Pointer to the Object
            );
            
    treeViewCredential = gtk_tree_view_new_with_model(GTK_TREE_MODEL(credentialStore));
    treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW (treeViewCredential));
    g_signal_connect(G_OBJECT (treeSelection), "changed", G_CALLBACK (select_credential_cb), credentialStore);
    
    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "editable", TRUE, "editable-set", TRUE, NULL);
    g_signal_connect(G_OBJECT (renderer), "edited", G_CALLBACK(cell_edited_cb), credentialStore);
    g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_CREDENTIAL_REALM));
    treeViewColumn = gtk_tree_view_column_new_with_attributes ("Realm",
            renderer,
            "markup", COLUMN_CREDENTIAL_REALM,
            NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW(treeViewCredential), treeViewColumn);

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "editable", TRUE, "editable-set", TRUE, NULL);
    g_signal_connect(G_OBJECT (renderer), "edited", G_CALLBACK(cell_edited_cb), credentialStore);
    g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_CREDENTIAL_USERNAME));
    treeViewColumn = gtk_tree_view_column_new_with_attributes (_("Authentication name"),
            renderer,
            "markup", COLUMN_CREDENTIAL_USERNAME,
            NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW(treeViewCredential), treeViewColumn);

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "editable", TRUE, "editable-set", TRUE, NULL);
    g_signal_connect(G_OBJECT (renderer), "edited", G_CALLBACK(cell_edited_cb), credentialStore);
    g_signal_connect (renderer, "editing-started", G_CALLBACK (editing_started_cb), NULL);
    g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_CREDENTIAL_PASSWORD));
    treeViewColumn = gtk_tree_view_column_new_with_attributes (_("Password"),
            renderer,
            "markup", COLUMN_CREDENTIAL_PASSWORD,
            NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW(treeViewCredential), treeViewColumn);
    
    gtk_container_add(GTK_CONTAINER(scrolledWindowCredential), treeViewCredential);
    
    fill_treeview_with_credential(credentialStore, *a);
        
    /* Dynamically resize the window to fit the scrolled window */
    gtk_widget_size_request(GTK_WIDGET(treeViewCredential), &requisitionTreeView);
    gtk_widget_set_size_request(GTK_WIDGET(scrolledWindowCredential), requisitionTable.width, requisitionTreeView.height + 20);
        
    /* Credential Buttons */    
    hbox = gtk_hbox_new(FALSE, 10);
    gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 2, 1, 2);
    
    addButton = gtk_button_new_from_stock (GTK_STOCK_ADD);
    g_signal_connect (addButton, "clicked", G_CALLBACK (add_credential_cb), credentialStore);
    gtk_box_pack_start(GTK_BOX(hbox), addButton, FALSE, FALSE, 0);
        
    deleteCredButton = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
    g_signal_connect (deleteCredButton, "clicked", G_CALLBACK (delete_credential_cb), treeViewCredential);
    gtk_box_pack_start(GTK_BOX(hbox), deleteCredButton, FALSE, FALSE, 0);
 
 	 /* SRTP Section */
    gnome_main_section_new_with_table (_("Security"), &frame, &table, 2, 3);
	gtk_container_set_border_width (GTK_CONTAINER(table), 10);
	gtk_table_set_row_spacings (GTK_TABLE(table), 10);
    gtk_table_set_col_spacings( GTK_TABLE(table), 10);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    GtkWidget * useSipTlsCheckBox;
	useSipTlsCheckBox = gtk_check_button_new_with_mnemonic(_("Use TLS transport (sips)"));
	//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(useSipTlsCheckBox),
		//	g_strcasecmp(curAccountResolveOnce,"FALSE") == 0 ? TRUE: FALSE);
	gtk_table_attach_defaults(GTK_TABLE(table), useSipTlsCheckBox, 0, 2, 0, 1);
	//gtk_widget_set_sensitive(GTK_WIDGET(useSipTlsCheckBox), TRUE );
	
	GtkWidget * sipTlsAdvancedButton;
	sipTlsAdvancedButton = gtk_button_new_from_stock(GTK_STOCK_EDIT);
    gtk_table_attach_defaults(GTK_TABLE(table), sipTlsAdvancedButton, 2, 3, 0, 1);
    g_signal_connect(G_OBJECT(sipTlsAdvancedButton), "clicked", G_CALLBACK(show_advanced_tls_options_cb), currentAccount->properties);
       	    
    label = gtk_label_new_with_mnemonic (_("SRTP key exchange"));
 	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    keyExchangeCombo = gtk_combo_box_new_text();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), keyExchangeCombo);
    gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), "ZRTP");
    //gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), "SDES");
    gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), _("Disabled"));      
    
    advancedZrtpButton = gtk_button_new_from_stock(GTK_STOCK_PREFERENCES);
    g_signal_connect(G_OBJECT(advancedZrtpButton), "clicked", G_CALLBACK(show_advanced_zrtp_options_cb), currentAccount->properties);
        
    if (g_strcmp0(curSRTPEnabled, "FALSE") == 0)
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo), 1);
        gtk_widget_set_sensitive(GTK_WIDGET(advancedZrtpButton), FALSE);
    } else {
        if (strcmp(curKeyExchange, "1") == 0) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo),0);
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo), 1);
            gtk_widget_set_sensitive(GTK_WIDGET(advancedZrtpButton), FALSE);
        }
    }
    
	g_signal_connect (G_OBJECT (GTK_COMBO_BOX(keyExchangeCombo)), "changed", G_CALLBACK (key_exchange_changed_cb), currentAccount);
    
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), keyExchangeCombo, 1, 2, 1, 2);    
    gtk_table_attach_defaults(GTK_TABLE(table), advancedZrtpButton, 2, 3, 1, 2);
	
    gtk_widget_show_all(ret);
    
	return ret;
}

static GPtrArray * getNewCredential(GHashTable * properties)
{
    GtkTreeIter iter;
    gboolean valid;
    gint row_count = 0;

    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(credentialStore), &iter);

    GPtrArray * credential_array = g_ptr_array_new ();
        
    gchar *username;
    gchar *realm;
    gchar *password;
    GHashTable * new_table;   
     
    gtk_tree_model_get (GTK_TREE_MODEL(credentialStore), &iter,
                        COLUMN_CREDENTIAL_REALM, &realm,
                        COLUMN_CREDENTIAL_USERNAME, &username,
                        COLUMN_CREDENTIAL_PASSWORD, &password,
                        -1);

    g_hash_table_insert(properties, g_strdup(ACCOUNT_REALM), realm);
    g_hash_table_insert(properties, g_strdup(ACCOUNT_AUTHENTICATION_USERNAME), username);
    g_hash_table_insert(properties, g_strdup(ACCOUNT_PASSWORD), password);
  
    valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(credentialStore), &iter);
                                     
    while (valid) {        
        gtk_tree_model_get (GTK_TREE_MODEL(credentialStore), &iter,
                            COLUMN_CREDENTIAL_REALM, &realm,
                            COLUMN_CREDENTIAL_USERNAME, &username,
                            COLUMN_CREDENTIAL_PASSWORD, &password,
                            -1);

        DEBUG ("Row %d: %s %s %s", row_count, username, password, realm);

        new_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        g_hash_table_insert(new_table, g_strdup(ACCOUNT_REALM), realm);
        g_hash_table_insert(new_table, g_strdup(ACCOUNT_USERNAME), username);
        g_hash_table_insert(new_table, g_strdup(ACCOUNT_PASSWORD), password);

        g_ptr_array_add (credential_array, new_table);

        row_count ++;

        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL(credentialStore), &iter);
    }

    return credential_array;
}

	void
show_account_window (account_t * a)
{

	GtkWidget * notebook;
	GtkWidget * tab; 
	gint response;
	account_t *currentAccount;

	currentAccount = a;   

	dialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Account settings"),
				GTK_WINDOW(get_main_window()),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	    		GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL,
				GTK_STOCK_APPLY,				
				GTK_RESPONSE_ACCEPT,
				NULL));

	gtk_dialog_set_has_separator(dialog, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER(dialog), 0);

	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX (dialog->vbox), notebook, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 10);
	gtk_widget_show(notebook);

	/* General Settings */
	tab = create_account_tab(&currentAccount);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Basic")));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);
	
	/* Advanced */
	tab = create_advanced_tab(&currentAccount);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Advanced")));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);
	
	gtk_notebook_set_current_page( GTK_NOTEBOOK( notebook) ,  0);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if(response == GTK_RESPONSE_ACCEPT)
	{
		gchar* proto = (gchar *)gtk_combo_box_get_active_text(GTK_COMBO_BOX(entryProtocol));

		g_hash_table_replace(currentAccount->properties,
				g_strdup(ACCOUNT_ENABLED),
				g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(entryEnabled)) ? "TRUE": "FALSE"));
		g_hash_table_replace(currentAccount->properties,
				g_strdup(ACCOUNT_RESOLVE_ONCE),
				g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(entryResolveNameOnlyOnce)) ? "FALSE": "TRUE"));
		g_hash_table_replace(currentAccount->properties,
				g_strdup(ACCOUNT_ALIAS),
				g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryAlias))));
		g_hash_table_replace(currentAccount->properties,
				g_strdup(ACCOUNT_TYPE),
				g_strdup(proto));
		g_hash_table_replace(currentAccount->properties,
				g_strdup(ACCOUNT_HOSTNAME),
				g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryHostname))));
		g_hash_table_replace(currentAccount->properties,
				g_strdup(ACCOUNT_USERNAME),
				g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryUsername))));
		g_hash_table_replace(currentAccount->properties,
				g_strdup(ACCOUNT_PASSWORD),
				g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryPassword))));
		g_hash_table_replace(currentAccount->properties,
				g_strdup(ACCOUNT_MAILBOX),
				g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryMailbox))));
		g_hash_table_replace(currentAccount->properties,
				g_strdup(ACCOUNT_REGISTRATION_EXPIRE),
				g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryExpire))));				
        
		if (strcmp(proto, "SIP") == 0) {
			guint i, size;
			account_t * account;
			gchar * stun_srv;
			gchar * stun_enable;
			gboolean flag = FALSE;

			size = account_list_get_size();
			// If a SIP account already exists, fetch its information about STUN
			for(i=0; i<size; i++){
				account = account_list_get_nth(i);
				if( strcmp(g_hash_table_lookup(account->properties, ACCOUNT_TYPE), "SIP" ) == 0 )
				{
					stun_srv = g_hash_table_lookup(account->properties, ACCOUNT_SIP_STUN_SERVER);
					stun_enable = g_hash_table_lookup(account->properties, ACCOUNT_SIP_STUN_ENABLED);
					g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SIP_STUN_SERVER),
							g_strdup(stun_srv));
					g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SIP_STUN_ENABLED),
							g_strdup(stun_enable));
					flag = TRUE;
					break;
				}
			}

			// Otherwise set a default value
			if(!flag)
			{
				g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SIP_STUN_SERVER), (gchar*)"");
				g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SIP_STUN_ENABLED), "FALSE");
			}
			
			gchar* keyExchange = (gchar *)gtk_combo_box_get_active_text(GTK_COMBO_BOX(keyExchangeCombo));
            if (g_strcasecmp(keyExchange, "ZRTP") == 0) {
                g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("TRUE"));
                g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_KEY_EXCHANGE), g_strdup("1"));
            } else {
                g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("FALSE"));
            }
    		
			config_window_set_stun_visible();
		}

	    /* Set new credentials if any */
	    
        DEBUG("Setting credentials"); 

        /* This hack is necessary because of the way the 
        * configuration file is made (.ini at that time).
        * and deleting account per account is too much 
        * of a trouble. 
        */
        dbus_delete_all_credential(currentAccount);
        
        GPtrArray * credential = getNewCredential(currentAccount->properties);         
        currentAccount->credential_information = credential;
        if(currentAccount->credential_information != NULL) {
            int i;
            for(i = 0; i < currentAccount->credential_information->len; i++) {
                dbus_set_credential(currentAccount, i);
            }
            dbus_set_number_of_credential(currentAccount, currentAccount->credential_information->len);
        }
        
		/** @todo Verify if it's the best condition to check */
		if (currentAccount->accountID == NULL) {
			dbus_add_account(currentAccount);
		}
		else {
			dbus_set_account_details(currentAccount);
		}
		

	} 
	
	gtk_widget_destroy (GTK_WIDGET(dialog));

}
