/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#include <accountlist.h>
#include <accountwindow.h>
#include <actions.h>
#include <config.h>
#include <dbus.h>
#include <mainwindow.h>

#include <gtk/gtk.h>

/** Local variables */
gboolean  dialogOpen = FALSE;
GtkListStore *account_store;
GtkWidget * addButton;
GtkWidget * editButton;
GtkWidget * deleteButton;
GtkWidget * defaultButton;

account_t * selectedAccount;
account_t * defaultAccount;

/** Fills the treelist with accounts */
	void 
config_window_fill_account_list ()
{
	if(dialogOpen)
	{
		GtkTreeIter iter;

		gtk_list_store_clear(account_store);
		int i;
		for( i = 0; i < account_list_get_size(); i++)
		{
			account_t  * a = account_list_get_nth (i);
			if (a)
			{
				gtk_list_store_append (account_store, &iter);

				gtk_list_store_set(account_store, &iter,
						0, g_hash_table_lookup(a->properties, ACCOUNT_ALIAS),  // Name
						1, g_hash_table_lookup(a->properties, ACCOUNT_TYPE),   // Protocol
						2, account_state_name(a->state),      // Status
						3, a,                                 // Pointer
						-1);

			}
		} 

		gtk_widget_set_sensitive( GTK_WIDGET(editButton),   FALSE);
		gtk_widget_set_sensitive( GTK_WIDGET(deleteButton), FALSE);
		gtk_widget_set_sensitive( GTK_WIDGET(defaultButton), FALSE);
	}
}
/**
 * Delete an account
 */
	static void 
delete_account( GtkWidget *widget, gpointer   data )
{
	if(selectedAccount)
	{
		dbus_remove_account(selectedAccount->accountID);
	}
}

/**
 * Edit an account
 */
	static void 
edit_account( GtkWidget *widget, gpointer   data )
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
add_account( GtkWidget *widget, gpointer   data )
{
	show_account_window(NULL);
}

/*
 * Should mark the account as default
 */
void
default_account(GtkWidget *widget, gpointer data)
{
	// set account as default	
	if(selectedAccount)
	{
		account_list_set_default(selectedAccount->accountID);
		dbus_set_default_account(selectedAccount->accountID);
	}	
}

/* Call back when the user click on an account in the list */
static void 
select_account(GtkTreeSelection *sel, GtkTreeModel *model) 
{
	GtkTreeIter  iter;
	GValue val;

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
	{
		selectedAccount = NULL;
		return;
	}

	val.g_type = G_TYPE_POINTER;
	gtk_tree_model_get_value (model, &iter, 3, &val);

	selectedAccount = (account_t*) g_value_get_pointer(&val);
	g_value_unset(&val);

	if(selectedAccount)
	{
		gtk_widget_set_sensitive( GTK_WIDGET(editButton),   TRUE);
		gtk_widget_set_sensitive( GTK_WIDGET(deleteButton), TRUE); 
		gtk_widget_set_sensitive( GTK_WIDGET(defaultButton), TRUE);
	}
	g_print("select");

}



	GtkWidget *
create_accounts_tab()
{
	GtkWidget *ret;
	GtkWidget *sw;
	GtkWidget *view;
	GtkWidget *bbox;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkWidget *label;

	GtkTreeIter iter;
        GValue val;
        val.g_type = G_TYPE_POINTER;
	account_t* current;
	//GValue id;
	//val.g_type = G_TYPE;

	selectedAccount = NULL;

	ret = gtk_vbox_new(FALSE, 10); 
	gtk_container_set_border_width (GTK_CONTAINER (ret), 10);

	label = gtk_label_new("This is the list of accounts previously setup.");

	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start(GTK_BOX(ret), label, FALSE, TRUE, 0);
	gtk_widget_show(label);

	sw = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

	gtk_box_pack_start(GTK_BOX(ret), sw, TRUE, TRUE, 0);
	account_store = gtk_list_store_new (4, 
			G_TYPE_STRING,  // Name
			G_TYPE_STRING,  // Protocol
			G_TYPE_STRING,  // Status
			G_TYPE_POINTER  // Pointer to the Object
			);

	view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(account_store));

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	g_signal_connect (G_OBJECT (sel), "changed",
			G_CALLBACK (select_account),
			account_store);

	gtk_tree_model_get_value(GTK_TREE_MODEL(account_store), &iter, 3, &val);
	//current = (account_t*) g_value_get_pointer(&val);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(account_store),
			2, GTK_SORT_ASCENDING);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("Alias",
			rend,
			"markup", 0,
			NULL);
	//if(current->accountID == account_list_get_default())
	g_object_set(G_OBJECT(rend), "weight", "bold", NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("Protocol",
			rend,
			"markup", 1,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("Status",
			rend,
			"markup", 2,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);
	g_object_unref(G_OBJECT(account_store));
	gtk_container_add(GTK_CONTAINER(sw), view);

	/* The buttons to press! */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 10); //GAIM_HIG_BOX_SPACE
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_START);
	gtk_box_pack_start(GTK_BOX(ret), bbox, FALSE, FALSE, 0);
	gtk_widget_show (bbox); 

	addButton = gtk_button_new_from_stock (GTK_STOCK_ADD);
	g_signal_connect_swapped(G_OBJECT(addButton), "clicked",
			G_CALLBACK(add_account), NULL);
	gtk_box_pack_start(GTK_BOX(bbox), addButton, FALSE, FALSE, 0);
	gtk_widget_show(addButton);

	editButton = gtk_button_new_from_stock (GTK_STOCK_EDIT);
	g_signal_connect_swapped(G_OBJECT(editButton), "clicked",
			G_CALLBACK(edit_account), NULL);
	gtk_box_pack_start(GTK_BOX(bbox), editButton, FALSE, FALSE, 0);
	gtk_widget_show(editButton);
	
	deleteButton = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	g_signal_connect_swapped(G_OBJECT(deleteButton), "clicked",
			G_CALLBACK(delete_account), NULL);
	gtk_box_pack_start(GTK_BOX(bbox), deleteButton, FALSE, FALSE, 0);
	gtk_widget_show(deleteButton);
	
	defaultButton = gtk_button_new_with_mnemonic("Set as Default");
	g_signal_connect_swapped(G_OBJECT(defaultButton), "clicked", 
			G_CALLBACK(default_account), NULL);
	gtk_box_pack_start(GTK_BOX(bbox), defaultButton, FALSE, FALSE, 0);
	gtk_widget_show(defaultButton);

	gtk_widget_show_all(ret);

	config_window_fill_account_list();

	return ret;
}

	void
show_config_window ()
{
	GtkDialog * dialog;
	GtkWidget * notebook;
	GtkWidget * tab;

	dialogOpen = TRUE;

	dialog = GTK_DIALOG(gtk_dialog_new_with_buttons ("Preferences",
				GTK_WINDOW(get_main_window()),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_CLOSE,
				GTK_RESPONSE_ACCEPT,
				NULL));

	gtk_dialog_set_has_separator(dialog, FALSE);
	gtk_window_set_default_size( GTK_WINDOW(dialog), 400, 400);
	gtk_container_set_border_width (GTK_CONTAINER(dialog), 0);

	notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (dialog->vbox), notebook, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER(notebook), 10);
	gtk_widget_show(notebook);

	/* Create tabs */ 
	tab = create_accounts_tab();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new("Accounts"));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);
	
	gtk_dialog_run (dialog);

	dialogOpen = FALSE;

	gtk_widget_destroy (GTK_WIDGET(dialog));
}

