/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include <actions.h>
#include <mainwindow.h>
#include <accountlist.h>
#include <audioconf.h>
#include <accountconfigdialog.h>
#include <zrtpadvanceddialog.h>
#include <tlsadvanceddialog.h>

// From version 2.16, gtk provides the functionalities libsexy used to provide
#if GTK_CHECK_VERSION(2,16,0)
#else
#include <libsexy/sexy-icon-entry.h>
#endif

#define PW_HIDDEN		"*****"

#include <string.h>
#include <dbus/dbus.h>
#include <config.h>
#include <gtk/gtk.h>


/**
 * TODO: tidy this up
 * by storing these variables
 * in a private structure.
 * Local variables 
 */
GtkDialog * dialog;
GtkWidget * hbox;
GtkWidget * label;
GtkWidget * entryID;
GtkWidget * entryAlias;
GtkWidget * protocolComboBox;
GtkWidget * entryUsername;
GtkWidget * entryRouteSet;
GtkWidget * entryHostname;
GtkWidget * entryPassword;
GtkWidget * entryMailbox;
GtkWidget * entryUseragent;
GtkWidget * entryResolveNameOnlyOnce;
GtkWidget * expireSpinBox;
GtkListStore * credentialStore;
GtkWidget * deleteCredButton;
GtkWidget * treeViewCredential;
// GtkWidget * scrolledWindowCredential;
GtkWidget * advancedZrtpButton;
GtkWidget * keyExchangeCombo;
GtkWidget * useSipTlsCheckBox;

GtkWidget * localAddressEntry;
GtkWidget * publishedAddressEntry;
GtkWidget * localAddressLabel;
GtkWidget * localAddressCombo;
GtkWidget * useStunCheckBox;
GtkWidget * sameAsLocalRadioButton;
GtkWidget * publishedAddrRadioButton;
GtkWidget * sameAsLocalLabel;
GtkWidget * publishedAddrLabel;
GtkWidget * publishedPortSpinBox;
GtkWidget * localPortLabel;
GtkWidget * localPortSpinBox;
GtkWidget * publishedAddressLabel;
GtkWidget * publishedPortLabel;
GtkWidget * stunServerLabel;
GtkWidget * stunServerEntry;

GtkWidget * displayNameEntry;

GtkWidget * security_tab;
GtkWidget * advanced_tab;

GtkWidget * overrtp;

GHashTable * directIpCallsProperties = NULL;

gchar *current_username;


// Credentials
enum {
	COLUMN_CREDENTIAL_REALM,
	COLUMN_CREDENTIAL_USERNAME,
	COLUMN_CREDENTIAL_PASSWORD,
	COLUMN_CREDENTIAL_DATA,
	COLUMN_CREDENTIAL_COUNT
};

/*
 * Display / Hide the password
 */
static void show_password_cb (GtkWidget *widget, gpointer data)
{
	gtk_entry_set_visibility (GTK_ENTRY (data), !gtk_entry_get_visibility (GTK_ENTRY (data)));
}

/* Signal to protocolComboBox 'changed' */
void change_protocol_cb (account_t *currentAccount UNUSED) {

	gchar *protocol = gtk_combo_box_get_active_text (GTK_COMBO_BOX (protocolComboBox));

	// Only if tabs are not NULL
	if(security_tab && advanced_tab) {
	    if (g_strcasecmp (protocol, "IAX") == 0) {
		gtk_widget_hide (GTK_WIDGET(security_tab));
		gtk_widget_hide (GTK_WIDGET(advanced_tab));
	    }
	    else {
                gtk_widget_show (GTK_WIDGET(security_tab));
		gtk_widget_show (GTK_WIDGET(advanced_tab));
	    }
	}
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


        void
select_dtmf_type( void )
{

        DEBUG("DTMF selection changed\n");

        if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(overrtp) ) )
        {
                // dbus_set_audio_manager( ALSA );
                DEBUG("Selected DTMF over RTP");
        }
        else {

                // dbus_set_audio_manager( PULSEAUDIO );
                DEBUG("Selected DTMF over SIP");
        }

}

static GPtrArray* getNewCredential (GHashTable * properties) {

	GtkTreeIter iter;
	gboolean valid;
	gint row_count = 0;

	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(credentialStore), &iter);

	GPtrArray *credential_array = g_ptr_array_new ();

	gchar *username;
	gchar *realm;
	gchar *password;
	GHashTable * new_table;   

	DEBUG("shit");

	if(valid == FALSE) {
	  DEBUG("Gtk tree model iter is not valid")
	  return NULL;
	}

	gtk_tree_model_get (GTK_TREE_MODEL(credentialStore), &iter,
			      COLUMN_CREDENTIAL_REALM, &realm,
			      COLUMN_CREDENTIAL_USERNAME, &username,
			      COLUMN_CREDENTIAL_PASSWORD, &password,
			      -1);
	DEBUG("shit");

	g_hash_table_insert(properties, g_strdup(ACCOUNT_REALM), realm);

	// better use the current_username as it is the account username in the 
	// g_hash_table_insert(properties, g_strdup(ACCOUNT_AUTHENTICATION_USERNAME), username);
	g_hash_table_insert(properties, g_strdup(ACCOUNT_AUTHENTICATION_USERNAME), current_username);
	  
	// Do not change the password if nothing has been changed by the user
	if (g_strcasecmp (password, PW_HIDDEN) != 0)
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

static void update_credential_cb(GtkWidget *widget, gpointer data UNUSED)
{
	GtkTreeIter iter;
	gtk_tree_model_get_iter_from_string ((GtkTreeModel *) credentialStore, &iter, "0");
	gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "column"));
	// g_print ("set password to %s\n", (gchar *) gtk_entry_get_text(GTK_ENTRY(widget)));
	gtk_list_store_set (GTK_LIST_STORE (credentialStore), &iter, column, (gchar *) gtk_entry_get_text(GTK_ENTRY(widget)), -1);
}

static GtkWidget* create_basic_tab (account_t **a)  {

	GtkWidget * frame;
	GtkWidget * table;
	account_t *currentAccount;
	GtkWidget * clearTextCheckbox;
#if GTK_CHECK_VERSION(2,16,0)
#else
	GtkWidget *image;
#endif

	// Default settings
	gchar *curAccountID = "";
	gchar *curAccountEnabled = "true";
	gchar *curAccountType = "SIP";
	gchar *curAlias = "";
	gchar *curUsername = "";
	gchar *curRouteSet = "";
	gchar *curHostname = "";
	gchar *curPassword = "";
	/* TODO: add curProxy, and add boxes for Proxy support */
	gchar *curMailbox = "";
	gchar *curUseragent = "";

	currentAccount = *a;

	int row = 0;

	DEBUG("Config: Create basic account tab");

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
		// curRouteSet = g_hash_table_lookup(currentAccount->properties, ACCOUNT_ROUTE);
		curMailbox = g_hash_table_lookup(currentAccount->properties, ACCOUNT_MAILBOX);
		curUseragent = g_hash_table_lookup(currentAccount->properties, ACCOUNT_USERAGENT);
	}

	gnome_main_section_new (_("Account Parameters"), &frame);
	gtk_widget_show(frame);

	if(strcmp(curAccountType, "SIP") == 0) {
	  table = gtk_table_new (9, 2,  FALSE/* homogeneous */);
	}
	else if(strcmp(curAccountType, "IAX") == 0) {
	  table = gtk_table_new (8, 2, FALSE);
	}

	gtk_table_set_row_spacings( GTK_TABLE(table), 10);
	gtk_table_set_col_spacings( GTK_TABLE(table), 10);
	gtk_widget_show (table);
	gtk_container_add( GTK_CONTAINER( frame) , table );

	label = gtk_label_new_with_mnemonic (_("_Alias"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	entryAlias = gtk_entry_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryAlias);
	gtk_entry_set_text(GTK_ENTRY(entryAlias), curAlias);
	gtk_table_attach ( GTK_TABLE( table ), entryAlias, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);


	row++;
	label = gtk_label_new_with_mnemonic (_("_Protocol"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	protocolComboBox = gtk_combo_box_new_text();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), protocolComboBox);
	gtk_combo_box_append_text(GTK_COMBO_BOX(protocolComboBox), "SIP");
	if( is_iax_enabled() ) gtk_combo_box_append_text(GTK_COMBO_BOX(protocolComboBox), "IAX");
	if(strcmp(curAccountType, "SIP") == 0)
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(protocolComboBox),0);
	}
	else if(strcmp(curAccountType, "IAX") == 0)
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(protocolComboBox),1);
	}
	else
	{
	        DEBUG("Config: Error: Account protocol not valid");
		/* Should never come here, add debug message. */
		gtk_combo_box_append_text(GTK_COMBO_BOX(protocolComboBox), _("Unknown"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(protocolComboBox),2);
	}
	gtk_table_attach ( GTK_TABLE( table ), protocolComboBox, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	/* Link signal 'changed' */
	g_signal_connect (G_OBJECT (GTK_COMBO_BOX(protocolComboBox)), "changed",
			G_CALLBACK (change_protocol_cb),
			currentAccount);

	row++;
	label = gtk_label_new_with_mnemonic (_("_Host name"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	entryHostname = gtk_entry_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryHostname);
	gtk_entry_set_text(GTK_ENTRY(entryHostname), curHostname);
	gtk_table_attach ( GTK_TABLE( table ), entryHostname, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	row++;
	label = gtk_label_new_with_mnemonic (_("_User name"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);	
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
	gtk_table_attach ( GTK_TABLE( table ), entryUsername, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	if(strcmp(curAccountType, "SIP") == 0) {
	  g_signal_connect(G_OBJECT (entryUsername), "changed", G_CALLBACK (update_credential_cb), NULL);
	  g_object_set_data (G_OBJECT (entryUsername), "column", GINT_TO_POINTER (COLUMN_CREDENTIAL_USERNAME));
	}

	// Route set can be update only for SIP account
	// TODO: uncomment this code and implement route 
	/*
	if(strcmp(curAccountType, "SIP") == 0) {
	  row++;
	  label = gtk_label_new_with_mnemonic(_("_Route (optional)"));
	  gtk_table_attach(GTK_TABLE( table ), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	  entryRouteSet = gtk_entry_new();
	  gtk_label_set_mnemonic_widget(GTK_LABEL(label), entryRouteSet);
	  gtk_entry_set_text(GTK_ENTRY(entryRouteSet), curRouteSet);
	  gtk_table_attach (GTK_TABLE(table), entryRouteSet, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	}
	*/

	row++;
	label = gtk_label_new_with_mnemonic (_("_Password"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
#if GTK_CHECK_VERSION(2,16,0)
	entryPassword = gtk_entry_new();
	GtkSettings *settings = gtk_settings_get_default ();
	//g_object_set (G_OBJECT (settings), "gtk-entry-password-hint-timeout", 600, NULL);
	gtk_entry_set_icon_from_stock (GTK_ENTRY (entryPassword), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_DIALOG_AUTHENTICATION);
#else
	entryPassword = sexy_icon_entry_new();
	image = gtk_image_new_from_stock( GTK_STOCK_DIALOG_AUTHENTICATION , GTK_ICON_SIZE_SMALL_TOOLBAR );
	sexy_icon_entry_set_icon( SEXY_ICON_ENTRY(entryPassword), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE(image) );
#endif
	gtk_entry_set_visibility(GTK_ENTRY(entryPassword), FALSE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryPassword);
	gtk_entry_set_text(GTK_ENTRY(entryPassword), curPassword);
	gtk_table_attach ( GTK_TABLE( table ), entryPassword, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	if(strcmp(curAccountType, "SIP") == 0) {
	  g_signal_connect (G_OBJECT (entryPassword), "changed", G_CALLBACK (update_credential_cb), NULL);
	  g_object_set_data (G_OBJECT (entryPassword), "column", GINT_TO_POINTER (COLUMN_CREDENTIAL_PASSWORD));
	}

	row++;
	clearTextCheckbox = gtk_check_button_new_with_mnemonic (_("Show password"));
	g_signal_connect (clearTextCheckbox, "toggled", G_CALLBACK (show_password_cb), entryPassword);
	gtk_table_attach (GTK_TABLE (table), clearTextCheckbox, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	row++;
	label = gtk_label_new_with_mnemonic (_("_Voicemail number"));
	gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
	entryMailbox = gtk_entry_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryMailbox);
	gtk_entry_set_text(GTK_ENTRY(entryMailbox), curMailbox);
	gtk_table_attach ( GTK_TABLE( table ), entryMailbox, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	row++;
	label = gtk_label_new_with_mnemonic (_("_User-agent"));
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	entryUseragent = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryUseragent);
	gtk_entry_set_text (GTK_ENTRY (entryUseragent), curUseragent);
	gtk_table_attach ( GTK_TABLE( table ), entryUseragent, 1, 2, row, row+1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	gtk_widget_show_all( table );
	gtk_container_set_border_width (GTK_CONTAINER(table), 10);

	*a = currentAccount;
	return frame;
}

static void fill_treeview_with_credential (GtkListStore * credentialStore, account_t * account) 
{
	GtkTreeIter iter;
	gtk_list_store_clear(credentialStore);

	/* This is the default, undeletable credential */
	/*
	// gtk_list_store_append (credentialStore, &iter);
	gchar * authentication_name = g_hash_table_lookup(account->properties, ACCOUNT_AUTHENTICATION_USERNAME);
	gchar * realm = g_hash_table_lookup(account->properties, ACCOUNT_REALM);        
	if (realm == NULL || (g_strcmp0(realm, "") == 0)) {
		realm = g_strdup("*");
	}

	if((authentication_name == NULL) || (g_strcmp0(authentication_name, "") == 0)) {
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
				// COLUMN_CREDENTIAL_PASSWORD, gtk_entry_get_text(GTK_ENTRY(entryPassword)),    
				COLUMN_CREDENTIAL_PASSWORD, PW_HIDDEN,    
				COLUMN_CREDENTIAL_DATA, account, 
				-1);
		g_signal_handlers_disconnect_by_func (G_OBJECT(entryUsername), G_CALLBACK(update_credential_cb), NULL);
	}

	*/
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
				COLUMN_CREDENTIAL_DATA, element, -1);
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

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			COLUMN_CREDENTIAL_REALM, "*",
			COLUMN_CREDENTIAL_USERNAME, _("Authentication"),
			COLUMN_CREDENTIAL_PASSWORD, _("Secret"),
			-1);
}

static void delete_credential_cb(GtkWidget *button, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeView *treeview = (GtkTreeView *)data;
	GtkTreeModel *model = gtk_tree_view_get_model (treeview);
	GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
	{
		GtkTreePath *path;
		path = gtk_tree_model_get_path (model, &iter);
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

		gtk_tree_path_free (path);
	}

}

static void cell_edited_cb(GtkCellRendererText *renderer, gchar *path_desc, gchar *text, gpointer data)
{
	GtkTreeModel *model = (GtkTreeModel *)data;
	GtkTreePath *path = gtk_tree_path_new_from_string (path_desc);
	GtkTreeIter iter;

	gint column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "column"));
	DEBUG("path desc in cell_edited_cb: %s\n", text);

	if(g_strcasecmp(path_desc, "0") == 0) {
		if(g_strcasecmp(text, gtk_entry_get_text (GTK_ENTRY(entryUsername))) != 0) {
			g_signal_handlers_disconnect_by_func (G_OBJECT(entryUsername), G_CALLBACK(update_credential_cb), NULL);
		}

		if (column == COLUMN_CREDENTIAL_PASSWORD) { 
			gtk_entry_set_text (GTK_ENTRY (entryPassword), text);
			text = PW_HIDDEN;	
		}
	}  

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, column, text, -1);
	gtk_tree_path_free (path);

}

static void editing_started_cb (GtkCellRenderer *cell, GtkCellEditable * editable, const gchar * path, gpointer data) {

	DEBUG("Editing started");
	DEBUG("path desc in editing_started_cb: %s\n", path);

	// If we are dealing the first row
	if (g_strcasecmp (path, "0") == 0)
	{
		gtk_entry_set_text (GTK_ENTRY (editable), gtk_entry_get_text (GTK_ENTRY (entryPassword)));
	}
}

static void show_advanced_zrtp_options_cb(GtkWidget *widget UNUSED, gpointer data)
{

    DEBUG("Advanced options for SRTP");
    if (g_strcasecmp(gtk_combo_box_get_active_text(GTK_COMBO_BOX(keyExchangeCombo)), (gchar *) "ZRTP") == 0) {
        show_advanced_zrtp_options((GHashTable *) data);
    }
    else {
        show_advanced_sdes_options((GHashTable *) data);
    }
}


static void show_advanced_tls_options_cb(GtkWidget *widget UNUSED, gpointer data)
{
	DEBUG("Advanced options for TLS");
	show_advanced_tls_options((GHashTable *) data);
}

static void key_exchange_changed_cb(GtkWidget *widget, gpointer data)
{

    DEBUG("Key exchange changed %s", gtk_combo_box_get_active_text(GTK_COMBO_BOX(keyExchangeCombo)));

    int isSdes = g_strcasecmp(gtk_combo_box_get_active_text(GTK_COMBO_BOX(keyExchangeCombo)), (gchar *) "SDES");
    int isZrtp = g_strcasecmp(gtk_combo_box_get_active_text(GTK_COMBO_BOX(keyExchangeCombo)), (gchar *) "ZRTP");

    if ((isSdes == 0) || (isZrtp == 0)) {
        gtk_widget_set_sensitive(GTK_WIDGET(advancedZrtpButton), TRUE);
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(advancedZrtpButton), FALSE);
        
    }
}


static void use_sip_tls_cb(GtkWidget *widget, gpointer data)
{

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		DEBUG("Using sips");
		gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);
		// Uncheck stun
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(useStunCheckBox), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(useStunCheckBox), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(sameAsLocalRadioButton), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(publishedAddrRadioButton), TRUE);
		gtk_widget_hide (stunServerLabel);
		gtk_widget_hide (stunServerEntry);



		if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sameAsLocalRadioButton))) {
			gtk_widget_show(publishedAddressEntry);
			gtk_widget_show(publishedPortSpinBox);
			gtk_widget_show(publishedAddressLabel);
			gtk_widget_show(publishedPortLabel);
		}

	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(useStunCheckBox), TRUE);

		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(useStunCheckBox))) {
			gtk_widget_set_sensitive(GTK_WIDGET(sameAsLocalRadioButton), FALSE);
			gtk_widget_set_sensitive(GTK_WIDGET(publishedAddrRadioButton), FALSE);
			gtk_widget_show(stunServerLabel);
			gtk_widget_show(stunServerEntry);
			gtk_widget_hide(publishedAddressEntry);
			gtk_widget_hide(publishedPortSpinBox);
			gtk_widget_hide(publishedAddressLabel);
			gtk_widget_hide(publishedPortLabel);
		}
		else {
			gtk_widget_set_sensitive(GTK_WIDGET(sameAsLocalRadioButton), TRUE);
			gtk_widget_set_sensitive(GTK_WIDGET(publishedAddrRadioButton), TRUE);
			gtk_widget_hide(stunServerLabel);
			gtk_widget_hide(stunServerEntry);
		}

	}   
}

static local_interface_changed_cb(GtkWidget * widget, gpointer data UNUSED) {

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(sameAsLocalRadioButton))) {

		gchar *local_iface_name;
		gchar *local_iface_addr;
		local_iface_addr = g_malloc(36);

		local_iface_name = (gchar *) gtk_combo_box_get_active_text (GTK_COMBO_BOX (localAddressCombo));
		// sflphone_get_interface_addr_from_name((char *)local_interface);
		sflphone_get_interface_addr_from_name(local_iface_name, &local_iface_addr, 36);

		gtk_entry_set_text(GTK_ENTRY(localAddressEntry), local_iface_addr);
		gtk_entry_set_text (GTK_ENTRY(publishedAddressEntry), local_iface_addr);

		// gchar * local_port = (gchar *) gtk_entry_get_text(GTK_ENTRY(localPortSpinBox));
		// gtk_spin_button_set_value(GTK_SPIN_BUTTON(publishedPortSpinBox), g_ascii_strtod(local_port, NULL));
		g_free(local_iface_addr);
	}

}

static set_published_addr_manually_cb(GtkWidget * widget, gpointer data UNUSED)
{

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		DEBUG("Config: Showing manual publishing options");    
		gtk_widget_show(publishedPortLabel);            
		gtk_widget_show(publishedPortSpinBox);
		gtk_widget_show(publishedAddressLabel);                	
		gtk_widget_show(publishedAddressEntry);
	} else {
		DEBUG("Config: Hiding manual publishing options");   
		gtk_widget_hide(publishedPortLabel);            
		gtk_widget_hide(publishedPortSpinBox);
		gtk_widget_hide(publishedAddressLabel);                	
		gtk_widget_hide(publishedAddressEntry);
	}
}

static use_stun_cb(GtkWidget *widget, gpointer data UNUSED)
{
	gchar *local_interface;
	gchar *local_address;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {

		DEBUG("Config: Showing stun options, hiding Local/Published info");
		gtk_widget_show (stunServerLabel);
		gtk_widget_show (stunServerEntry);
		gtk_widget_set_sensitive (sameAsLocalRadioButton, FALSE);
		gtk_widget_set_sensitive (publishedAddrRadioButton, FALSE);

		gtk_widget_hide (publishedAddressLabel);
		gtk_widget_hide (publishedPortLabel);
		gtk_widget_hide (publishedAddressEntry);
		gtk_widget_hide (publishedPortSpinBox);

	} else {

	        DEBUG("Config: hiding stun options, showing Local/Published info");

		gtk_widget_hide (stunServerLabel);
		gtk_widget_hide (stunServerEntry);
		gtk_widget_set_sensitive (sameAsLocalRadioButton, TRUE);
		gtk_widget_set_sensitive (publishedAddrRadioButton, TRUE);

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (publishedAddrRadioButton))) {
			gtk_widget_show (publishedAddressLabel);
			gtk_widget_show (publishedPortLabel);
			gtk_widget_show (publishedAddressEntry);
			gtk_widget_show (publishedPortSpinBox);
		}
	}
}


static same_as_local_cb(GtkWidget * widget, gpointer data UNUSED)
{

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		DEBUG("Same as local");
		gchar * local_interface;
		gchar * local_address;

		local_interface = (gchar *) gtk_combo_box_get_active_text(GTK_COMBO_BOX(localAddressCombo));
		// sflphone_get_interface_addr_from_name((char *)local_interface);
		local_address = dbus_get_address_from_interface_name(local_interface);

		gtk_entry_set_text(GTK_ENTRY(publishedAddressEntry), local_address);

		gchar * local_port = (gchar *) gtk_entry_get_text(GTK_ENTRY(localPortSpinBox));
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(publishedPortSpinBox), g_ascii_strtod(local_port, NULL));
	} 

}



GtkWidget* create_credential_widget (account_t **a) {

	GtkWidget *frame, *table, *scrolledWindowCredential, *addButton;
	GtkCellRenderer * renderer;
	GtkTreeViewColumn * treeViewColumn;
	GtkTreeSelection * treeSelection;

	/* Credentials tree view */
	gnome_main_section_new_with_table (_("Credential"), &frame, &table, 1, 1);
	gtk_container_set_border_width (GTK_CONTAINER(table), 10);
	gtk_table_set_row_spacings(GTK_TABLE(table), 10);

	scrolledWindowCredential = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledWindowCredential), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledWindowCredential), GTK_SHADOW_IN);
	gtk_table_attach_defaults (GTK_TABLE (table), scrolledWindowCredential, 0, 1, 0, 1);

	credentialStore = gtk_list_store_new(COLUMN_CREDENTIAL_COUNT,
			G_TYPE_STRING,  // Realm
			G_TYPE_STRING,  // Username
			G_TYPE_STRING,  // Password
			G_TYPE_POINTER  // Pointer to the Objectc
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
	g_signal_connect (G_OBJECT (renderer), "edited", G_CALLBACK (cell_edited_cb), credentialStore);
	g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_CREDENTIAL_USERNAME));
	treeViewColumn = gtk_tree_view_column_new_with_attributes (_("Authentication name"),
			renderer,
			"markup", COLUMN_CREDENTIAL_USERNAME,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(treeViewCredential), treeViewColumn);

	renderer = gtk_cell_renderer_text_new();
	g_object_set (renderer, "editable", TRUE, "editable-set", TRUE, NULL);
	g_signal_connect (G_OBJECT (renderer), "edited", G_CALLBACK (cell_edited_cb), credentialStore);
	g_signal_connect (renderer, "editing-started", G_CALLBACK (editing_started_cb), NULL);
	g_object_set_data (G_OBJECT (renderer), "column", GINT_TO_POINTER (COLUMN_CREDENTIAL_PASSWORD));
	treeViewColumn = gtk_tree_view_column_new_with_attributes (_("Password"),
			renderer,
			"markup", COLUMN_CREDENTIAL_PASSWORD,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(treeViewCredential), treeViewColumn);

	gtk_container_add(GTK_CONTAINER(scrolledWindowCredential), treeViewCredential);

	DEBUG("Credential pas ok");
	fill_treeview_with_credential(credentialStore, *a);
	DEBUG("Credential ok");

	/* Credential Buttons */    
	hbox = gtk_hbox_new(FALSE, 10);
	gtk_table_attach_defaults(GTK_TABLE(table), hbox, 0, 3, 1, 2);

	addButton = gtk_button_new_from_stock (GTK_STOCK_ADD);
	g_signal_connect (addButton, "clicked", G_CALLBACK (add_credential_cb), credentialStore);
	gtk_box_pack_start(GTK_BOX(hbox), addButton, FALSE, FALSE, 0);

	deleteCredButton = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	g_signal_connect (deleteCredButton, "clicked", G_CALLBACK (delete_credential_cb), treeViewCredential);
	gtk_box_pack_start(GTK_BOX(hbox), deleteCredButton, FALSE, FALSE, 0);

	/* Dynamically resize the window to fit the scrolled window */
	GtkRequisition requisitionTable;
	GtkRequisition requisitionTreeView;
	gtk_widget_size_request (GTK_WIDGET(treeViewCredential), &requisitionTreeView);
	gtk_widget_size_request (GTK_WIDGET(table), &requisitionTable);
	gtk_widget_set_size_request (GTK_WIDGET(scrolledWindowCredential), 400, 120);
	// same_as_local_cb (sameAsLocalRadioButton, NULL);
	// set_published_addr_manually_cb (publishedAddrRadioButton, NULL);

	return frame;
}


GtkWidget* create_security_widget (account_t **a) {

	GtkWidget *frame, *table, *sipTlsAdvancedButton, *label;
	gchar *curSRTPEnabled = NULL, *curKeyExchange = NULL, *curTLSEnabled = NULL;

	// Load from SIP/IAX/Unknown ?
	if((*a)) {	
		curKeyExchange = g_hash_table_lookup ((*a)->properties, ACCOUNT_KEY_EXCHANGE);
		if (curKeyExchange == NULL) {
			curKeyExchange = "none";
		}		

		curSRTPEnabled = g_hash_table_lookup ((*a)->properties, ACCOUNT_SRTP_ENABLED);
		if (curSRTPEnabled == NULL) {
			curSRTPEnabled = "false";
		}

		curTLSEnabled = g_hash_table_lookup ((*a)->properties, TLS_ENABLE);
		if (curTLSEnabled == NULL) {
			curTLSEnabled = "false";
		}
	}

	gnome_main_section_new_with_table (_("Security"), &frame, &table, 2, 3);
	gtk_container_set_border_width (GTK_CONTAINER(table), 10);
	gtk_table_set_row_spacings (GTK_TABLE(table), 10);
	gtk_table_set_col_spacings (GTK_TABLE(table), 10);

	/* TLS subsection */
	sipTlsAdvancedButton = gtk_button_new_from_stock (GTK_STOCK_EDIT);
	gtk_table_attach_defaults (GTK_TABLE (table), sipTlsAdvancedButton, 2, 3, 0, 1);
	gtk_widget_set_sensitive (GTK_WIDGET (sipTlsAdvancedButton), FALSE);    
	g_signal_connect (G_OBJECT (sipTlsAdvancedButton), "clicked", G_CALLBACK (show_advanced_tls_options_cb), (*a)->properties);

	useSipTlsCheckBox = gtk_check_button_new_with_mnemonic(_("Use TLS transport (sips)"));
	g_signal_connect (useSipTlsCheckBox, "toggled", G_CALLBACK(use_sip_tls_cb), sipTlsAdvancedButton);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(useSipTlsCheckBox), (g_strcmp0(curTLSEnabled, "true") == 0) ? TRUE:FALSE);
	gtk_table_attach_defaults(GTK_TABLE(table), useSipTlsCheckBox, 0, 2, 0, 1);

	/* ZRTP subsection */
	label = gtk_label_new_with_mnemonic (_("SRTP key exchange"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	keyExchangeCombo = gtk_combo_box_new_text();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), keyExchangeCombo);
	gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), "ZRTP");
	gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), "SDES");
	gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), _("Disabled"));      

	advancedZrtpButton = gtk_button_new_from_stock(GTK_STOCK_PREFERENCES);
	g_signal_connect(G_OBJECT(advancedZrtpButton), "clicked", G_CALLBACK(show_advanced_zrtp_options_cb), (*a)->properties);

	if (g_strcmp0(curSRTPEnabled, "false") == 0)
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo), 2);
		gtk_widget_set_sensitive(GTK_WIDGET(advancedZrtpButton), FALSE);
	} else {
		if (strcmp(curKeyExchange, ZRTP) == 0) {
			gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo),0);
		} 
		else if (strcmp(curKeyExchange, SDES) == 0) {
			gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo),1);
		}
		else {
			gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo), 2);
			gtk_widget_set_sensitive(GTK_WIDGET(advancedZrtpButton), FALSE);
		}
	}

	g_signal_connect (G_OBJECT (GTK_COMBO_BOX(keyExchangeCombo)), "changed", G_CALLBACK (key_exchange_changed_cb), *a);

	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(table), keyExchangeCombo, 1, 2, 1, 2);    
	gtk_table_attach_defaults(GTK_TABLE(table), advancedZrtpButton, 2, 3, 1, 2);

	gtk_widget_show_all(table);

	return frame;
}


GtkWidget * create_security_tab (account_t **a)
{
	GtkWidget * frame;
	GtkWidget * ret;
	GtkWidget * hbox;


	ret = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

	// Credentials frame
	frame = create_credential_widget (a);
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

	// Security frame
	frame = create_security_widget (a);
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

	gtk_widget_show_all(ret);

	return ret;
	}

GtkWidget* create_registration_expire (account_t **a) {

    GtkWidget *table, *frame, *label;

    gchar *resolve_once=NULL, *account_expire=NULL;

    if (*a) {
        resolve_once = g_hash_table_lookup ((*a)->properties, ACCOUNT_RESOLVE_ONCE);
	account_expire = g_hash_table_lookup ((*a)->properties, ACCOUNT_REGISTRATION_EXPIRE);
    }

    gnome_main_section_new_with_table (_("Registration"), &frame, &table, 2, 3);
    gtk_container_set_border_width (GTK_CONTAINER(table), 10);
    gtk_table_set_row_spacings (GTK_TABLE (table), 5);	
    
    label = gtk_label_new_with_mnemonic (_("Registration expire"));
    gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    expireSpinBox = gtk_spin_button_new_with_range (1, 65535, 1);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), expireSpinBox);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (expireSpinBox), g_ascii_strtod (account_expire, NULL));
    gtk_table_attach_defaults (GTK_TABLE (table), expireSpinBox, 1, 2, 0, 1);
	

    entryResolveNameOnlyOnce = gtk_check_button_new_with_mnemonic (_("_Comply with RFC 3263"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (entryResolveNameOnlyOnce),
				  g_strcasecmp (resolve_once,"false") == 0 ? TRUE: FALSE);
    gtk_table_attach_defaults (GTK_TABLE (table), entryResolveNameOnlyOnce, 0, 2, 1, 2);
    gtk_widget_set_sensitive (GTK_WIDGET (entryResolveNameOnlyOnce ) , TRUE );

    return frame;
}

GtkWidget* create_network (account_t **a) {
  
    GtkWidget *table, *frame, *label;
    gchar *local_interface, *local_port;

    if (*a) {
        local_interface = g_hash_table_lookup ((*a)->properties, LOCAL_INTERFACE);
	local_port = g_hash_table_lookup ((*a)->properties, LOCAL_PORT);
    }

    gnome_main_section_new_with_table (_("Network Interface"), &frame, &table, 2, 3);
    gtk_container_set_border_width (GTK_CONTAINER(table), 10);
    gtk_table_set_row_spacings( GTK_TABLE(table), 5);

    /**
     * Retreive the list of IP interface from the 
     * the daemon and build the combo box.
     */

    GtkListStore * ipInterfaceListStore; 
    GtkTreeIter iter;

    ipInterfaceListStore =  gtk_list_store_new( 1, G_TYPE_STRING );
    label = gtk_label_new_with_mnemonic (_("Local address"));    
    gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);

    GtkTreeIter current_local_iface_iter = iter;   
    gchar ** iface_list = NULL;
    // iface_list = (gchar**) dbus_get_all_ip_interface();
    iface_list = (gchar**) dbus_get_all_ip_interface_by_name();
    gchar ** iface = NULL;

    // flag to determine if local_address is found 
    gboolean iface_found = FALSE;

    gchar *local_iface_addr;
    gchar *local_iface_name; 

    local_iface_addr= g_malloc(36);
    
    if (iface_list != NULL) {

        // fill the iterface combo box
        for (iface = iface_list; *iface; iface++) {         
	    DEBUG("Interface %s", *iface);            
	    gtk_list_store_append(ipInterfaceListStore, &iter );
	    gtk_list_store_set(ipInterfaceListStore, &iter, 0, *iface, -1 );

	    // set the current local address
	    if (!iface_found && (g_strcmp0(*iface, local_interface) == 0)) {
	        DEBUG("Setting active local address combo box");
		current_local_iface_iter = iter;
		iface_found = TRUE;
	  }
	}
	    
	if(!iface_found) {
	  DEBUG("Did not find local ip address, take fisrt in the list");
	  gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ipInterfaceListStore), &current_local_iface_iter);
	}
	
    }
  
    
    localAddressCombo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ipInterfaceListStore));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), localAddressCombo);
    gtk_table_attach ( GTK_TABLE( table ), localAddressCombo, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    g_object_unref(G_OBJECT(ipInterfaceListStore));	

 
    GtkCellRenderer * ipInterfaceCellRenderer;
    ipInterfaceCellRenderer = gtk_cell_renderer_text_new();

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(localAddressCombo), ipInterfaceCellRenderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(localAddressCombo), ipInterfaceCellRenderer, "text", 0, NULL);
    gtk_combo_box_set_active_iter(GTK_COMBO_BOX(localAddressCombo), &current_local_iface_iter);


    // Fill the text entry with the ip address of local interface selected
    localAddressEntry = gtk_entry_new();
    local_iface_name = (gchar *) gtk_combo_box_get_active_text (GTK_COMBO_BOX (localAddressCombo));
    sflphone_get_interface_addr_from_name(local_iface_name, &local_iface_addr, 36);
    gtk_entry_set_text(GTK_ENTRY(localAddressEntry), local_iface_addr);
    gtk_widget_set_sensitive(localAddressEntry, FALSE); 
    gtk_table_attach ( GTK_TABLE( table ), localAddressEntry, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    g_free(local_iface_addr);
    
    // Local port widget
    label = gtk_label_new_with_mnemonic (_("Local port"));
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
    gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
    localPortSpinBox = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), localPortSpinBox);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(localPortSpinBox), g_ascii_strtod(local_port, NULL));

    gtk_table_attach_defaults(GTK_TABLE(table), localPortSpinBox, 1, 2, 1, 2);

    return frame;
}

GtkWidget* create_published_address (account_t **a) {

	GtkWidget *table, *frame, *label;
	gchar *use_tls, *published_address, *published_port, *local_address, *stun_enable, *stun_server, *published_sameas_local;

	// Get the user configuration
	if (*a) {

		use_tls = g_hash_table_lookup ((*a)->properties,  TLS_ENABLE);
		published_sameas_local = g_hash_table_lookup ((*a)->properties,  PUBLISHED_SAMEAS_LOCAL);

		if (g_strcasecmp (published_sameas_local, "true") == 0) {
			published_address = dbus_get_address_from_interface_name (g_hash_table_lookup ((*a)->properties, LOCAL_INTERFACE));
			published_port = g_hash_table_lookup ((*a)->properties,  LOCAL_PORT);
		}
		else {
			published_address = g_hash_table_lookup ((*a)->properties,  PUBLISHED_ADDRESS);
			published_port = g_hash_table_lookup ((*a)->properties,  PUBLISHED_PORT);
		}

		stun_enable = g_hash_table_lookup ((*a)->properties,  ACCOUNT_SIP_STUN_ENABLED);
		stun_server = g_hash_table_lookup ((*a)->properties,  ACCOUNT_SIP_STUN_SERVER);
		published_sameas_local = g_hash_table_lookup ((*a)->properties,  PUBLISHED_SAMEAS_LOCAL);
	}

	gnome_main_section_new_with_table (_("Published address"), &frame, &table, 2, 3);
	gtk_container_set_border_width (GTK_CONTAINER(table), 10);
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);

	useStunCheckBox = gtk_check_button_new_with_mnemonic(_("Using STUN"));
	gtk_table_attach_defaults(GTK_TABLE(table), useStunCheckBox, 0, 1, 0, 1);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(useStunCheckBox), 
			g_strcasecmp(stun_enable, "true") == 0 ? TRUE: FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET(useStunCheckBox),
			g_strcasecmp(use_tls,"true") == 0 ? FALSE: TRUE);

	stunServerLabel = gtk_label_new_with_mnemonic (_("STUN server URL"));
	gtk_table_attach_defaults(GTK_TABLE(table), stunServerLabel, 0, 1, 1, 2);
	gtk_misc_set_alignment(GTK_MISC (stunServerLabel), 0, 0.5);
	stunServerEntry = gtk_entry_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL (stunServerLabel), stunServerEntry);
	gtk_entry_set_text(GTK_ENTRY(stunServerEntry), stun_server);
	gtk_table_attach_defaults(GTK_TABLE(table), stunServerEntry, 1, 2, 1, 2);

	sameAsLocalRadioButton = gtk_radio_button_new_with_mnemonic_from_widget(NULL, _("Same as local parameters"));
	gtk_table_attach_defaults(GTK_TABLE(table), sameAsLocalRadioButton, 0, 2, 3, 4);

	publishedAddrRadioButton = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(sameAsLocalRadioButton), _("Set published address and port:"));
	gtk_table_attach_defaults(GTK_TABLE(table), publishedAddrRadioButton, 0, 2, 4, 5);

	if (g_strcasecmp (published_sameas_local, "true") == 0) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sameAsLocalRadioButton), TRUE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (publishedAddrRadioButton), FALSE);
	} else {
	        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sameAsLocalRadioButton), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (publishedAddrRadioButton), TRUE);
	}

	publishedAddressLabel = gtk_label_new_with_mnemonic (_("Published address"));
	gtk_table_attach_defaults( GTK_TABLE(table), publishedAddressLabel, 0, 1, 5, 6);
	gtk_misc_set_alignment(GTK_MISC (publishedAddressLabel), 0, 0.5);
	publishedAddressEntry = gtk_entry_new();
	gtk_label_set_mnemonic_widget (GTK_LABEL (publishedAddressLabel), publishedAddressEntry);

	gtk_entry_set_text(GTK_ENTRY(publishedAddressEntry), published_address);
	gtk_table_attach_defaults( GTK_TABLE(table), publishedAddressEntry, 1, 2, 5, 6);

	publishedPortLabel = gtk_label_new_with_mnemonic(_("Published port"));
	gtk_table_attach_defaults(GTK_TABLE(table), publishedPortLabel, 0, 1, 6, 7);
	gtk_misc_set_alignment(GTK_MISC (publishedPortLabel), 0, 0.5);
	publishedPortSpinBox = gtk_spin_button_new_with_range(1, 65535, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL (publishedPortLabel), publishedPortSpinBox);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(publishedPortSpinBox), g_ascii_strtod(published_port, NULL));

	gtk_table_attach_defaults(GTK_TABLE(table), publishedPortSpinBox, 1, 2, 6, 7);

	// This will trigger a signal, and the above two
	// widgets need to be instanciated before that.
	g_signal_connect(localAddressCombo, "changed", G_CALLBACK(local_interface_changed_cb), localAddressCombo);   

	g_signal_connect(useStunCheckBox, "toggled", G_CALLBACK(use_stun_cb), useStunCheckBox);	

	g_signal_connect(sameAsLocalRadioButton, "toggled", G_CALLBACK(same_as_local_cb), sameAsLocalRadioButton);   
	g_signal_connect(publishedAddrRadioButton, "toggled", G_CALLBACK(set_published_addr_manually_cb), publishedAddrRadioButton);

	set_published_addr_manually_cb(publishedAddrRadioButton, NULL);

	return frame;
}

GtkWidget* create_advanced_tab (account_t **a) {

	// Build the advanced tab, to appear on the account configuration panel
        DEBUG("Config: Build advanced tab")

	GtkWidget *ret, *frame;

	ret = gtk_vbox_new (FALSE, 10);
	gtk_container_set_border_width (GTK_CONTAINER(ret), 10);

	frame = create_registration_expire (a);
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

	frame = create_network (a);
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

	frame = create_published_address (a);
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

	gtk_widget_show_all (ret);

	use_stun_cb (GTK_WIDGET (useStunCheckBox), NULL);

	set_published_addr_manually_cb(GTK_WIDGET (publishedAddrRadioButton), NULL);

	return ret;
}

GtkWidget* create_codecs_configuration (account_t **a) {

        // Main widget
        GtkWidget *ret, *codecs, *dtmf, *box, *frame, *sipinfo, *table;
        account_t *currentAccount = *a;
        gchar *currentDtmfType = "";
        gboolean dtmf_are_rtp = TRUE;
	gpointer p;

        ret = gtk_vbox_new(FALSE, 10);
        gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

        box = codecs_box (a);

        // Box for the codecs
        gnome_main_section_new (_("Codecs"), &codecs);
        gtk_box_pack_start (GTK_BOX(ret), codecs, FALSE, FALSE, 0);
        gtk_widget_set_size_request (GTK_WIDGET (codecs), -1, 200);
        gtk_widget_show (codecs);
        gtk_container_add (GTK_CONTAINER (codecs) , box);

	// Add DTMF type selection for SIP account only
	p = g_hash_table_lookup(currentAccount->properties, g_strdup(ACCOUNT_TYPE));
	if(g_strcmp0(p, "SIP") == 0) {
	
	  // Box for dtmf
	  gnome_main_section_new_with_table (_("DTMF"), &dtmf, &table, 1, 2);
	  gtk_box_pack_start (GTK_BOX(ret), dtmf, FALSE, FALSE, 0);
	  gtk_widget_show (dtmf);


	  currentDtmfType = g_hash_table_lookup (currentAccount->properties, g_strdup(ACCOUNT_DTMF_TYPE));
	  if (g_strcasecmp(currentDtmfType, OVERRTP) != 0) {
            dtmf_are_rtp = FALSE;
	  }

	  overrtp = gtk_radio_button_new_with_label( NULL, _("RTP") );
	  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(overrtp), dtmf_are_rtp);
	  gtk_table_attach ( GTK_TABLE( table ), overrtp, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	  sipinfo = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(overrtp),  _("SIP"));
	  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(sipinfo), !dtmf_are_rtp);
	  g_signal_connect(G_OBJECT(sipinfo), "clicked", G_CALLBACK(select_dtmf_type), NULL);
	  gtk_table_attach ( GTK_TABLE( table ), sipinfo, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	}

        gtk_widget_show_all(ret);

        return ret;

}

void show_account_window (account_t * a) {

    GtkWidget * notebook;
    GtkWidget *tab, *codecs_tab, *ip_tab; 
    gint response;
    account_t *currentAccount;
  
    // In case the published address is same than local, 
    // we must resolve published address from interface name 
    gchar * local_interface;
    gchar * published_address;
  
    currentAccount = a;   

    if (currentAccount == NULL) {
      currentAccount = g_new0(account_t, 1);
      currentAccount->properties = dbus_account_details(NULL);
      currentAccount->accountID = "new";    
      sflphone_fill_codec_list_per_account (&currentAccount);
      DEBUG("Config: Account is NULL. Will fetch default values");      
    }

    dialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Account settings"),
						     GTK_WINDOW(get_main_window()),
						     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_STOCK_CANCEL,
						     GTK_RESPONSE_CANCEL,
						     GTK_STOCK_APPLY,				
						     GTK_RESPONSE_ACCEPT,
						     NULL));
    
    gtk_dialog_set_has_separator(dialog, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER(dialog), 0);
    
    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX (dialog->vbox), notebook, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 10);
    gtk_widget_show(notebook);

    // We do not need the global settings for the IP2IP account
    if (g_strcasecmp (currentAccount->accountID, IP2IP) != 0) {
      
      /* General Settings */
      tab = create_basic_tab(&currentAccount);
      
      gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Basic")));
      gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);


    }
    /* Codecs */
    codecs_tab = create_codecs_configuration (&currentAccount);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), codecs_tab, gtk_label_new(_("Codecs")));
    gtk_notebook_page_num (GTK_NOTEBOOK (notebook), codecs_tab);    

    // Get current protocol for this account protocol
    gchar *currentProtocol = "SIP";
    if(protocolComboBox)
        currentProtocol = (gchar *)gtk_combo_box_get_active_text(GTK_COMBO_BOX(protocolComboBox));

    // Do not need advanced or security one for the IP2IP account
    if (g_strcasecmp (currentAccount->accountID, IP2IP) != 0) {

	/* Advanced */
	advanced_tab = create_advanced_tab(&currentAccount);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), advanced_tab, gtk_label_new(_("Advanced")));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook), advanced_tab);
	
	/* Security */
	security_tab = create_security_tab (&currentAccount);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), security_tab, gtk_label_new(_("Security")));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook),security_tab);

    }
    else {

      /* Custom tab for the IP to IP profile */
      ip_tab = create_direct_ip_calls_tab (&currentAccount);
      gtk_notebook_prepend_page (GTK_NOTEBOOK (notebook), ip_tab, gtk_label_new(_("Network")));
      gtk_notebook_page_num (GTK_NOTEBOOK (notebook), ip_tab);
    }

    // Emit signal to hide advanced and security tabs in case of IAX
    if(protocolComboBox)
        g_signal_emit_by_name (GTK_WIDGET(protocolComboBox), "changed", NULL);

    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook) ,  0);

    /**************/
    /* Run dialog */
    /**************/
    response = gtk_dialog_run (GTK_DIALOG (dialog));
    
    // Update protocol in case it changed
    gchar *proto = NULL;
    if(protocolComboBox)
        proto = (gchar *) gtk_combo_box_get_active_text(GTK_COMBO_BOX(protocolComboBox));
    else
      proto = "SIP";

    // If cancel button is pressed
    if(response == GTK_RESPONSE_CANCEL) {
      gtk_widget_destroy (GTK_WIDGET(dialog));
      return;
    }

    gchar *key = g_strdup(ACCOUNT_USERNAME);

    // If accept button is 
    if (g_strcasecmp (currentAccount->accountID, IP2IP) != 0) {

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

      // Variable used to update credentials
      current_username = (gchar *)g_hash_table_lookup(currentAccount->properties, g_strdup(ACCOUNT_USERNAME));

    }

    if (proto && strcmp (proto, "SIP") == 0) {
      
      if (g_strcasecmp (currentAccount->accountID, IP2IP) != 0) {

	g_hash_table_replace(currentAccount->properties,
			   g_strdup(ACCOUNT_RESOLVE_ONCE),
			   g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(entryResolveNameOnlyOnce)) ? "false": "true"));

	g_hash_table_replace(currentAccount->properties,
			   g_strdup(ACCOUNT_REGISTRATION_EXPIRE),
			   g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(expireSpinBox))));
	
	/*
	// TODO: uncomment this code and implement route 
	g_hash_table_replace(currentAccount->properties,
			     g_strdup(ACCOUNT_ROUTE),
			     g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryRouteSet))));
	*/
	
	g_hash_table_replace(currentAccount->properties, 
			     g_strdup(ACCOUNT_USERAGENT), 
			     g_strdup(gtk_entry_get_text (GTK_ENTRY(entryUseragent))));

	g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SIP_STUN_ENABLED), 
			     g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(useStunCheckBox)) ? "true":"false"));
	
	g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SIP_STUN_SERVER), 
			     g_strdup(gtk_entry_get_text(GTK_ENTRY(stunServerEntry))));
	
	g_hash_table_replace(currentAccount->properties, g_strdup(PUBLISHED_SAMEAS_LOCAL), g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sameAsLocalRadioButton)) ? "true":"false"));	

	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sameAsLocalRadioButton))) {
	  
	  g_hash_table_replace(currentAccount->properties,
			       g_strdup(PUBLISHED_PORT),
			       g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(publishedPortSpinBox))));

	  g_hash_table_replace(currentAccount->properties,
			       g_strdup(PUBLISHED_ADDRESS),
			       g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(publishedAddressEntry))));
	}
	else {
	  
	  g_hash_table_replace(currentAccount->properties,
			       g_strdup(PUBLISHED_PORT),
			       g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(localPortSpinBox))));
	  local_interface = g_strdup((gchar *)gtk_combo_box_get_active_text(GTK_COMBO_BOX(localAddressCombo)));
	  
	  published_address = dbus_get_address_from_interface_name(local_interface);
	      
	  g_hash_table_replace(currentAccount->properties,
			       g_strdup(PUBLISHED_ADDRESS),
			       published_address);
	}

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(overrtp))) {
	  g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_DTMF_TYPE), g_strdup(OVERRTP));
	}
	else {
	  g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_DTMF_TYPE), g_strdup(SIPINFO));
	}
	
	gchar* keyExchange = (gchar *)gtk_combo_box_get_active_text(GTK_COMBO_BOX(keyExchangeCombo));
	
	if (g_strcasecmp(keyExchange, "ZRTP") == 0) {
	  g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("true"));
	  g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_KEY_EXCHANGE), g_strdup(ZRTP));
	}
	
	else if(g_strcasecmp(keyExchange, "SDES") == 0) {
	  g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("true"));
	  g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_KEY_EXCHANGE), g_strdup(SDES));
	}
	
	else {
	  g_hash_table_replace(currentAccount->properties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("false"));
	}
	
	g_hash_table_replace(currentAccount->properties, g_strdup(TLS_ENABLE), 
			     g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(useSipTlsCheckBox)) ? "true":"false"));
      }
      
      g_hash_table_replace(currentAccount->properties,
			   g_strdup(LOCAL_INTERFACE),
			   g_strdup((gchar *)gtk_combo_box_get_active_text(GTK_COMBO_BOX(localAddressCombo))));
	  
      g_hash_table_replace(currentAccount->properties,
			   g_strdup(LOCAL_PORT),
			   g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(localPortSpinBox))));

    }
    
    if (currentProtocol && strcmp(currentProtocol, "SIP") == 0) {

      /* Set new credentials if any */
      DEBUG("Config: Setting credentials"); 
      if (g_strcasecmp (currentAccount->accountID, IP2IP) != 0) {
      
	/* This hack is necessary because of the way the 
	 * configuration file is made (.ini at that time).
	 * and deleting account per account is too much 
	 * of a trouble. 
	 */
	dbus_delete_all_credential(currentAccount);
      
	DEBUG("Config: Get new credentials");
	GPtrArray * credential = getNewCredential(currentAccount->properties);         
	currentAccount->credential_information = credential;
	if(currentAccount->credential_information != NULL) {
	  int i;
	  for(i = 0; i < currentAccount->credential_information->len; i++) {
	    DEBUG("Create new credential");
	    dbus_set_credential(currentAccount, i);
	  }
	  // dbus_set_number_of_credential(currentAccount, currentAccount->credential_information->len);
	}
      }
    }

    /** @todo Verify if it's the best condition to check */
    if (g_strcasecmp(currentAccount->accountID, "new") == 0) {
      dbus_add_account(currentAccount);
    }
    else {
      dbus_set_account_details(currentAccount);
    }
    
    // Perpetuate changes to the deamon
    codec_list_update_to_daemon (currentAccount);
    
    gtk_widget_destroy (GTK_WIDGET(dialog));
} 

GtkWidget* create_direct_ip_calls_tab (account_t **a) {

	GtkWidget *ret, *frame, *label;
	gchar *description;

	ret = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

	description = g_markup_printf_escaped(_("This profile is used when you want to reach a remote peer simply by typing a sip URI such as <b>sip:remotepeer</b>. The settings you define here will also be used if no account can be matched to an incoming or outgoing call."));
	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), description);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);    
	gtk_box_pack_start (GTK_BOX (ret), label, FALSE, FALSE, 0);

	GtkRequisition requisition;
	gtk_widget_size_request (GTK_WIDGET (ret), &requisition);
	gtk_widget_set_size_request (GTK_WIDGET (label), 350, -1);        
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	frame = create_network (a);
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

	frame = create_security_widget (a);
	gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

	gtk_widget_show_all (ret);
	return ret;

}

