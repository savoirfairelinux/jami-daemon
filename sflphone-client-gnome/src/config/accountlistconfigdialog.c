/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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


#include <accountlistconfigdialog.h>
#include <dbus/dbus.h>
#include <accountconfigdialog.h>
#include <actions.h>
#include <utils.h>
#include <string.h>

#define CONTEXT_ID_REGISTRATION 0

GtkWidget *addButton;
GtkWidget *editButton;
GtkWidget *deleteButton;
GtkWidget *restoreButton;
GtkWidget *accountMoveDownButton;
GtkWidget *accountMoveUpButton;
GtkWidget * status_bar;
GtkListStore * accountStore;

GtkDialog * accountListDialog = NULL;

account_t * selectedAccount = NULL;      
// Account properties
enum {
	COLUMN_ACCOUNT_ALIAS,
	COLUMN_ACCOUNT_TYPE,
	COLUMN_ACCOUNT_STATUS,
	COLUMN_ACCOUNT_ACTIVE,
	COLUMN_ACCOUNT_DATA,
	COLUMN_ACCOUNT_COUNT
};

/**
 * Fills the treelist with accounts
 */
void account_list_config_dialog_fill() {

	if (accountListDialog == NULL) {
		DEBUG("Dialog is not opened");
		return;
	}

	GtkTreeIter iter;

	gtk_list_store_clear(accountStore);

	unsigned int i;
	for(i = 0; i < account_list_get_size(); i++) {
		account_t * a = account_list_get_nth (i);

		if (a) {
			gtk_list_store_append (accountStore, &iter);

			DEBUG("Filling accounts: Account is enabled :%s", g_hash_table_lookup(a->properties, ACCOUNT_ENABLED));

			gtk_list_store_set(accountStore, &iter,
					COLUMN_ACCOUNT_ALIAS, g_hash_table_lookup(a->properties, ACCOUNT_ALIAS),  // Name
					COLUMN_ACCOUNT_TYPE, g_hash_table_lookup(a->properties, ACCOUNT_TYPE),   // Protocol
					COLUMN_ACCOUNT_STATUS, account_state_name(a->state),      // Status
					COLUMN_ACCOUNT_ACTIVE, (g_strcasecmp(g_hash_table_lookup(a->properties, ACCOUNT_ENABLED),"true") == 0)? TRUE:FALSE,  // Enable/Disable
					COLUMN_ACCOUNT_DATA, a,   // Pointer
					-1);
		}
	}

}



/**
 * Call back when the user click on an account in the list
 */
	static void
select_account_cb(GtkTreeSelection *selection, GtkTreeModel *model)
{
	GtkTreeIter iter;
	GValue val;
	gchar *state;

	memset (&val, 0, sizeof(val));
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		selectedAccount = NULL;
		gtk_widget_set_sensitive(GTK_WIDGET(accountMoveUpButton), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(accountMoveDownButton), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(editButton), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(deleteButton), FALSE);                
		return;
	}

	// The Gvalue will be initialized in the following function
	gtk_tree_model_get_value(model, &iter, COLUMN_ACCOUNT_DATA, &val);

	selectedAccount = (account_t*)g_value_get_pointer(&val);
	g_value_unset(&val);

	if(selectedAccount != NULL) {
		gtk_widget_set_sensitive(GTK_WIDGET(editButton), TRUE);
		if (g_strcasecmp (selectedAccount->accountID, IP2IP) != 0) {
			gtk_widget_set_sensitive(GTK_WIDGET(accountMoveUpButton), TRUE);
			gtk_widget_set_sensitive(GTK_WIDGET(accountMoveDownButton), TRUE);
			gtk_widget_set_sensitive(GTK_WIDGET(deleteButton), TRUE);      

			/* Update status bar about current registration state */
			gtk_statusbar_pop (GTK_STATUSBAR (status_bar), CONTEXT_ID_REGISTRATION);

			if (selectedAccount->protocol_state_description != NULL  
				&& selectedAccount->protocol_state_code != 0) {

				gchar * response = g_strdup_printf(
					_("Server returned \"%s\" (%d)"),
					selectedAccount->protocol_state_description, 
					selectedAccount->protocol_state_code);
				gchar * message = g_strconcat(
					account_state_name(selectedAccount->state), 
					". ", 
					response,
					NULL);

				gtk_statusbar_push (GTK_STATUSBAR (status_bar), CONTEXT_ID_REGISTRATION, message);

				g_free(response);
				g_free(message);

			} else {
				state = (gchar*) account_state_name (selectedAccount->state);        
				gtk_statusbar_push (GTK_STATUSBAR (status_bar), CONTEXT_ID_REGISTRATION, state);        
			}
		}
		else {
			gtk_widget_set_sensitive(GTK_WIDGET(accountMoveUpButton), FALSE);
			gtk_widget_set_sensitive(GTK_WIDGET(accountMoveDownButton), FALSE);
			gtk_widget_set_sensitive(GTK_WIDGET(deleteButton), FALSE);      
		}
	}

	DEBUG("Selecting account in account window");
}

static void enable_account_cb (GtkCellRendererToggle *rend UNUSED, gchar* path,  gpointer data ) {

	GtkTreeIter iter;
	GtkTreePath *treePath;    
	GtkTreeModel *model;
	gboolean enable;
	account_t* acc ;

	// The IP2IP profile can't be disabled
	if (g_strcasecmp (path, "0") == 0)
		return;

	// Get pointer on object
	treePath = gtk_tree_path_new_from_string(path);
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(data));
	gtk_tree_model_get_iter(model, &iter, treePath);
	gtk_tree_model_get(model, &iter,
			COLUMN_ACCOUNT_ACTIVE, &enable,
			COLUMN_ACCOUNT_DATA, &acc,
			-1);
	
	enable = !enable;

	DEBUG("Account is %d enabled", enable);
	// Store value
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			COLUMN_ACCOUNT_ACTIVE, enable,
			-1);

	// Modify account state
	gchar * registrationState;
	if (enable == TRUE) {
		registrationState = g_strdup("true");
	} else {
		registrationState = g_strdup("false");
	}
	DEBUG("Replacing with %s", registrationState);
	g_hash_table_replace( acc->properties , g_strdup(ACCOUNT_ENABLED), registrationState);

	dbus_send_register(acc->accountID, enable);

}

/**
 * Move account in list depending on direction and selected account
 */
static void account_move (gboolean moveUp, gpointer data) {

	GtkTreeIter iter;
	GtkTreeIter *iter2;
	GtkTreeView *treeView;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreePath *treePath;
	gchar *path;

	// Get view, model and selection of codec store
	treeView = GTK_TREE_VIEW (data);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW(treeView));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(treeView));

	// Find selected iteration and create a copy
	gtk_tree_selection_get_selected (GTK_TREE_SELECTION(selection), &model, &iter);
	iter2 = gtk_tree_iter_copy (&iter);

	// Find path of iteration
	path = gtk_tree_model_get_string_from_iter(GTK_TREE_MODEL(model), &iter);

	// The first real account in the list can't move up because of the IP2IP account
	// It can still move down though
	if (g_strcasecmp (path, "1") == 0 && moveUp)
		return;

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
	gtk_tree_view_scroll_to_cell (treeView, treePath, NULL, FALSE, 0, 0);

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
account_move_up_cb(GtkButton *button UNUSED, gpointer data)
{
	// Change tree view ordering and get indice changed
	account_move(TRUE, data);
}

/**
 * Called from move down account button signal
 */
	static void
account_move_down_cb(GtkButton *button UNUSED, gpointer data)
{
	// Change tree view ordering and get indice changed
	account_move(FALSE, data);
}

	static void 
help_contents_cb (GtkWidget * widget,
		gpointer data UNUSED)
{
	GError *error = NULL;

	//gboolean success = gtk_show_uri (NULL, "ghelp: sflphone.xml", GDK_CURRENT_TIME, &error);
	gnome_help_display ("sflphone.xml", "accounts", &error);

	if (error != NULL)
	{    
		g_warning ("%s", error->message);

		g_error_free (error);
	}    
}

	static void
close_dialog_cb (GtkWidget * widget,
		gpointer data UNUSED)
{
	gtk_dialog_response(GTK_DIALOG(accountListDialog), GTK_RESPONSE_ACCEPT);

}

void highlight_ip_profile (GtkTreeViewColumn *col, GtkCellRenderer *rend, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data) {

	GValue val;
	account_t *current;

	memset (&val, 0, sizeof(val));
	gtk_tree_model_get_value(tree_model, iter, COLUMN_ACCOUNT_DATA, &val);
	current = (account_t*) g_value_get_pointer(&val);

	g_value_unset (&val);

	if (current != NULL) {

		// Make the first line appear differently
		(g_strcasecmp (current->accountID, IP2IP) == 0) ? g_object_set (G_OBJECT (rend), "weight", PANGO_WEIGHT_THIN, 
																					 "style", PANGO_STYLE_ITALIC, 
																					 "stretch", PANGO_STRETCH_ULTRA_EXPANDED,
																					 "scale", 0.95,
																					 NULL) : 
														g_object_set (G_OBJECT (rend), "weight", PANGO_WEIGHT_MEDIUM, 
																					 "style", PANGO_STYLE_NORMAL, 
																					 "stretch", PANGO_STRETCH_NORMAL,
																					 "scale", 1.0,
																					 NULL) ; 
	}
}

void highlight_registration (GtkTreeViewColumn *col, GtkCellRenderer *rend, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data) {

	GValue val;
	account_t *current;
	GdkColor green = {0, 255, 0, 0};

	memset (&val, 0, sizeof(val));
	gtk_tree_model_get_value(tree_model, iter, COLUMN_ACCOUNT_DATA, &val);
	current = (account_t*) g_value_get_pointer(&val);

	g_value_unset (&val);

	if (current != NULL) {
		if (g_strcasecmp (current->accountID, IP2IP) != 0) {
			// Color the account state: green -> registered, otherwise red
			(current->state == ACCOUNT_STATE_REGISTERED) ? g_object_set (G_OBJECT (rend), "foreground", "Dark Green", NULL) :
													g_object_set (G_OBJECT (rend), "foreground", "Dark Red", NULL);
		}
		else 
			g_object_set (G_OBJECT (rend), "foreground", "Black", NULL);
	}

}

/**
 * Account settings tab
 */
GtkWidget* create_account_list(GtkDialog * dialog) {

	GtkWidget *table, *scrolledWindow, *buttonBox;
	GtkCellRenderer *renderer;
	GtkTreeView * treeView;
	GtkTreeViewColumn *treeViewColumn;
	GtkTreeSelection *treeSelection;
	GtkRequisition requisition;

	selectedAccount = NULL;

	table = gtk_table_new (1, 2, FALSE/* homogeneous */);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10); 
	gtk_container_set_border_width (GTK_CONTAINER (table), 10);    

	scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_SHADOW_IN);
	gtk_table_attach (GTK_TABLE(table), scrolledWindow, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	accountStore = gtk_list_store_new(COLUMN_ACCOUNT_COUNT,
			G_TYPE_STRING,  // Name
			G_TYPE_STRING,  // Protocol
			G_TYPE_STRING,  // Status
			G_TYPE_BOOLEAN, // Enabled / Disabled
			G_TYPE_POINTER  // Pointer to the Object
			);

	account_list_config_dialog_fill();

	treeView = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL(accountStore)));
	treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW (treeView));
	g_signal_connect(G_OBJECT (treeSelection), "changed",
			G_CALLBACK (select_account_cb),
			accountStore);

	renderer = gtk_cell_renderer_toggle_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes("Enabled", renderer, "active", COLUMN_ACCOUNT_ACTIVE , NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), treeViewColumn);
	g_signal_connect (G_OBJECT(renderer) , "toggled" , G_CALLBACK (enable_account_cb), (gpointer)treeView );

	// gtk_cell_renderer_toggle_set_activatable (renderer, FALSE); 

	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes ("Alias",
			renderer,
			"markup", COLUMN_ACCOUNT_ALIAS,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(treeView), treeViewColumn);

	// A double click on the account line opens the window to edit the account
	g_signal_connect( G_OBJECT( treeView ) , "row-activated" , G_CALLBACK( edit_account_cb ) , NULL );
	gtk_tree_view_column_set_cell_data_func (treeViewColumn, renderer, highlight_ip_profile, NULL, NULL);

	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes (_("Protocol"),
			renderer,
			"markup", COLUMN_ACCOUNT_TYPE,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(treeView), treeViewColumn);
	gtk_tree_view_column_set_cell_data_func (treeViewColumn, renderer, highlight_ip_profile, NULL, NULL);

	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes (_("Status"),
			renderer,
			"markup", COLUMN_ACCOUNT_STATUS,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(treeView), treeViewColumn);
	// Highlight IP profile
	gtk_tree_view_column_set_cell_data_func (treeViewColumn, renderer, highlight_ip_profile, NULL, NULL);
	// Highlight account registration state 
	gtk_tree_view_column_set_cell_data_func (treeViewColumn, renderer, highlight_registration, NULL, NULL);

	g_object_unref(G_OBJECT(accountStore));

	gtk_container_add (GTK_CONTAINER(scrolledWindow), GTK_WIDGET (treeView));

	/* The buttons to press! */    
	buttonBox = gtk_vbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(buttonBox), 10);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_START);
	gtk_table_attach (GTK_TABLE(table), buttonBox, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	accountMoveUpButton = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
	gtk_widget_set_sensitive(GTK_WIDGET(accountMoveUpButton), FALSE);
	gtk_box_pack_start(GTK_BOX(buttonBox), accountMoveUpButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(accountMoveUpButton), "clicked", G_CALLBACK(account_move_up_cb), treeView);

	accountMoveDownButton = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
	gtk_widget_set_sensitive(GTK_WIDGET(accountMoveDownButton), FALSE);
	gtk_box_pack_start(GTK_BOX(buttonBox), accountMoveDownButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(accountMoveDownButton), "clicked", G_CALLBACK(account_move_down_cb), treeView);

	addButton = gtk_button_new_from_stock (GTK_STOCK_ADD);
	g_signal_connect_swapped(G_OBJECT(addButton), "clicked",
			G_CALLBACK(add_account_cb), NULL);
	gtk_box_pack_start(GTK_BOX(buttonBox), addButton, FALSE, FALSE, 0);

	editButton = gtk_button_new_from_stock (GTK_STOCK_EDIT);
	gtk_widget_set_sensitive(GTK_WIDGET(editButton), FALSE);    
	g_signal_connect_swapped(G_OBJECT(editButton), "clicked",
			G_CALLBACK(edit_account_cb), NULL);
	gtk_box_pack_start(GTK_BOX(buttonBox), editButton, FALSE, FALSE, 0);

	deleteButton = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	gtk_widget_set_sensitive(GTK_WIDGET(deleteButton), FALSE);    
	g_signal_connect_swapped(G_OBJECT(deleteButton), "clicked",
			G_CALLBACK(delete_account_cb), NULL);
	gtk_box_pack_start(GTK_BOX(buttonBox), deleteButton, FALSE, FALSE, 0);

	/* help and close buttons */    
	GtkWidget * buttonHbox = gtk_hbutton_box_new();
	gtk_table_attach(GTK_TABLE(table), buttonHbox, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 10);

	GtkWidget * helpButton = gtk_button_new_from_stock (GTK_STOCK_HELP);
	g_signal_connect_swapped(G_OBJECT(helpButton), "clicked",
			G_CALLBACK(help_contents_cb), NULL);
	gtk_box_pack_start(GTK_BOX(buttonHbox), helpButton, FALSE, FALSE, 0);

	GtkWidget * closeButton = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	g_signal_connect_swapped(G_OBJECT(closeButton), "clicked",  G_CALLBACK(close_dialog_cb), NULL);
	gtk_box_pack_start(GTK_BOX(buttonHbox), closeButton, FALSE, FALSE, 0);

	gtk_widget_show_all(table);
	// account_list_config_dialog_fill();

	/* Resize the scrolledWindow for a better view */
	gtk_widget_size_request(GTK_WIDGET(treeView), &requisition);
	gtk_widget_set_size_request(GTK_WIDGET(scrolledWindow), requisition.width + 20, requisition.height);
	GtkRequisition requisitionButton;
	gtk_widget_size_request(GTK_WIDGET(deleteButton), &requisitionButton);
	gtk_widget_set_size_request(GTK_WIDGET(closeButton), requisitionButton.width, -1);
	gtk_widget_set_size_request(GTK_WIDGET(helpButton), requisitionButton.width, -1);    

	gtk_widget_show_all(table);

	return table;
}

	void
show_account_list_config_dialog(void)
{
	GtkWidget * accountFrame;
	GtkWidget * tab;

	accountListDialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Accounts"),
				GTK_WINDOW(get_main_window()),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				NULL));

	/* Set window properties */
	gtk_dialog_set_has_separator(accountListDialog, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(accountListDialog), 0);
	gtk_window_set_resizable(GTK_WINDOW(accountListDialog), FALSE);

	gnome_main_section_new (_("Configured Accounts"), &accountFrame);
	gtk_box_pack_start( GTK_BOX(accountListDialog->vbox ), accountFrame , TRUE, TRUE, 0);
	gtk_widget_show(accountFrame);

	/* Accounts tab */
	tab = create_account_list(accountListDialog);
	gtk_widget_show(tab);    
	gtk_container_add(GTK_CONTAINER(accountFrame), tab);

	/* Status bar for the account list */
	status_bar = gtk_statusbar_new();
	gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (status_bar), FALSE);    
	gtk_widget_show(status_bar);
	gtk_box_pack_start(GTK_BOX(accountListDialog->vbox ), status_bar, TRUE, TRUE, 0);

	int number_accounts = account_list_get_registered_accounts ();
	if (number_accounts) {
		gchar * message = g_strdup_printf(n_("There is %d active account",
					"There are %d active accounts", number_accounts),
				number_accounts);
		gtk_statusbar_push (GTK_STATUSBAR (status_bar), CONTEXT_ID_REGISTRATION, message);
		g_free(message);
	} else {
		gtk_statusbar_push (GTK_STATUSBAR (status_bar), CONTEXT_ID_REGISTRATION, _("You have no active account"));        
	}

	gtk_dialog_run(accountListDialog);

	status_bar_display_account ();

	gtk_widget_destroy(GTK_WIDGET(accountListDialog));

	accountListDialog = NULL;

	update_actions ();
}


/**
 * Delete an account
 */
static void delete_account_cb (void) {
	
	if(selectedAccount != NULL) {
		dbus_remove_account(selectedAccount->accountID);
	}
}


/**
 * Edit an account
 */
static void edit_account_cb (void) {

	if(selectedAccount != NULL) {
		show_account_window (selectedAccount);
	} 
}

/**
 * Add an account
 */
static void add_account_cb (void) {

	show_account_window (NULL);
}
