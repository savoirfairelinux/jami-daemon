/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <accountlist.h>
#include <accountwindow.h>
#include <actions.h>
#include <config.h>
#include <dbus.h>
#include <mainwindow.h>

#include <gtk/gtk.h>

/**
 * Local variables
 */
gboolean  dialogOpen = FALSE;

GtkListStore * accountStore;
GtkListStore * codecStore;
GtkListStore * audioManagerStore;
GtkListStore * outputAudioDeviceManagerStore;
GtkListStore * inputAudioDeviceManagerStore;

GtkWidget * addButton;
GtkWidget * editButton;
GtkWidget * deleteButton;
GtkWidget * defaultButton;
GtkWidget * restoreButton;

account_t * selectedAccount;

//GtkListStore * rate_store;
//GtkWidget * combo_box;

/**
 * Fills the treelist with accounts
 */
void
config_window_fill_account_list()
{
	if(dialogOpen)
	{
		GtkTreeIter iter;

		gtk_list_store_clear(accountStore);
		int i;
		for(i = 0; i < account_list_get_size(); i++)
		{
			account_t * a = account_list_get_nth (i);
			if (a)
			{
				gtk_list_store_append (accountStore, &iter);
				gtk_list_store_set(accountStore, &iter,
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
 * Fills the tree list with supported codecs
 */
void
config_window_fill_codec_list()
{
	if(dialogOpen)
	{
		GtkTreeIter iter;
		int i;
		gtk_list_store_clear(codecStore);

		for(i = 0; i < codec_list_get_size(); i++)
		{
			codec_t* c = codec_list_get_nth(i);
			printf("%s\n", c->name);
			if(c)
			{
				gtk_list_store_append(codecStore, &iter);
				gtk_list_store_set(codecStore, &iter,
						0, c->is_active,		// Active
						1, c->name,				// Name
						2, c->sample_rate,		// Sample rate
						3, "bandwith",			// Bandwith
						4, "frequency",			// Frequency
						-1);
			}
		}
	}
}

/**
 * SEE deprecated
 */
/*void
config_window_fill_rate_list()
{
  if(dialogOpen)
  {
    GtkTreeIter iter;
    int i=0;
    gchar** ratelist = (gchar**)dbus_get_sample_rate_list();
    while(ratelist[i]!=NULL)
    {
	printf("%s\n", ratelist[i]);
        gtk_list_store_append (rate_store, &iter);
        gtk_list_store_set(rate_store, &iter,0,ratelist[i],-1);
        i++;
    }
  }
}*/

/**
 * TODO
 */
void
config_window_fill_output_audio_device_list()
{
	
}

/**
 * TODO
 */
void
config_window_fill_input_audio_device_list()
{
	
}

/**
 * Delete an account
 */
static void
delete_account(GtkWidget *widget, gpointer data)
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
edit_account(GtkWidget *widget, gpointer data)
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
add_account(GtkWidget *widget, gpointer data)
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

/**
 * Call back when the user click on an account in the list
 */
static void
select_account(GtkTreeSelection *selection, GtkTreeModel *model)
{
	GtkTreeIter iter;
	GValue val;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		selectedAccount = NULL;
		return;
	}

	val.g_type = G_TYPE_POINTER;
	gtk_tree_model_get_value(model, &iter, 3, &val);

	selectedAccount = (account_t*)g_value_get_pointer(&val);
	g_value_unset(&val);

	if(selectedAccount)
	{
		gtk_widget_set_sensitive( GTK_WIDGET(editButton), TRUE);
		gtk_widget_set_sensitive( GTK_WIDGET(deleteButton), TRUE); 
		gtk_widget_set_sensitive( GTK_WIDGET(defaultButton), TRUE);
	}
	g_print("select");
}

static void
codec_active_toggled(GtkCellRendererToggle * renderer, gchar * path, GtkListStore * store)
{
	GtkTreeIter iter;
	GValue val;
}

/**
 * TODO Change for tree model and tree selection as in precedent function
 */
static void
select_codec(GtkTreeSelection * selection, GtkTreeModel * model)
{
	GtkTreeIter iter;
	
	if(!gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		return;
	}
	//guint item = gtk_combo_box_get_active(wid);
	/* now we want this selected codec to be used as the preferred codec */
	/* ie first in the list in the user config */
	//codec_set_prefered_order(item);
	//dbus_set_prefered_codec(codec_list_get_nth(0)->name);  
}

/**
 * Move codec in list depending on direction and selected codec
 */
static void
moveCodec(gboolean moveUp, GtkListStore * codecListStore)
{
	// TODO
	printf("Button pressed");
}

/**
 * Called from move up codec button signal
 */
static void
moveCodecUp(GtkListStore * codecListStore)
{
	moveCodec(TRUE, codecListStore);
	printf("Button pressed");
}

/**
 * Called from move up codec button signal
 */
static void
moveCodecDown(GtkListStore * codecListStore)
{
	moveCodec(FALSE, codecListStore);
	printf("Button pressed");
}

/**
 * Select default account that is rendered in bold
 */
void
bold_if_default_account(GtkTreeViewColumn *col,
			GtkCellRenderer *rend,
			GtkTreeModel *tree_model,
			GtkTreeIter *iter,
			gpointer data)
{
	GValue val;
	val.g_type = G_TYPE_POINTER;
	gtk_tree_model_get_value(tree_model, iter, 3, &val);
	account_t* current = (account_t*) g_value_get_pointer(&val);
	g_value_unset(&val);
	if(g_strcasecmp(current->accountID, account_list_get_default()) == 0)
		g_object_set(G_OBJECT(rend), "weight", 800, NULL);
	else
		g_object_set(G_OBJECT(rend), "weight", 400, NULL);
}

/**
 * TODO
 */
void
default_codecs(GtkWidget* widget, gpointer data)
{
	int i = 0;
	int j = 0;
	gint * new_order;
	gchar ** default_list = (gchar**)dbus_default_codec_list();
	
	while(default_list[i] != NULL)
	{
		printf("%s\n", default_list[i]);
		i++;
	}
	i = 0;
	while(default_list[i] != NULL)
	{
		if(g_strcasecmp(codec_list_get_nth(0)->name, default_list[i]) == 0)
		{
			printf("%s %s\n",codec_list_get_nth(0)->name, default_list[i]);
			new_order[i] = 0;
		}
		else if(g_strcasecmp(codec_list_get_nth(1)->name, default_list[i]) == 0)
		{
			printf("%s %s\n",codec_list_get_nth(0)->name, default_list[0]);
			new_order[i] = 1;
		}	
		else
		{
			printf("%s %s\n",codec_list_get_nth(0)->name, default_list[0]);
			new_order[i] = 2;
		}
		printf("new_order[%i]=%i\n", i,j);
		i++;
	} 
	gtk_list_store_reorder(codecStore, new_order);  
}

/**
 * Create table widget for codecs
 */
GtkWidget*
create_codec_table()
{
	GtkWidget * ret;
	GtkWidget * scrolledWindow;
	GtkWidget * buttonBox;
	GtkWidget * moveUpButton;
	GtkWidget * moveDownButton;
	
	GtkWidget * treeView;
	GtkCellRenderer * renderer;
	GtkTreeSelection * treeSelection;
	GtkTreeViewColumn * treeViewColumn;
	GtkTreeIter iter;
	
	ret = gtk_hbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 10);
	
	scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_SHADOW_IN);
	
	gtk_box_pack_start(GTK_BOX(ret), scrolledWindow, TRUE, TRUE, 0);
	codecStore = gtk_list_store_new(5,
			G_TYPE_BOOLEAN,		// Active
			G_TYPE_STRING,		// Name
			G_TYPE_INT,			// Bit rate
			G_TYPE_STRING,		// Bandwith
			G_TYPE_STRING		// Frequency
			);
	
	treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(codecStore));
	
	treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
	g_signal_connect(G_OBJECT(treeSelection), "changed",
			G_CALLBACK (select_codec),
			codecStore);			// TODO ONLY for disabling or enabling move buttons
	
	// Active column
	renderer = gtk_cell_renderer_toggle_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes("", renderer, "active", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), treeViewColumn);
	g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(codec_active_toggled), codecStore);
	
	// TODO toggle on clicked
	
	// Name column
	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes("Name", renderer, "markup", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), treeViewColumn);
	
	// Bit rate column
	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes("Bit rate", renderer, "text", 2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), treeViewColumn);
	
	// Bandwith column
	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes("Bandwith", renderer, "text", 3, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), treeViewColumn);
	
	// Frequency column
	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes("Frequency", renderer, "text", 4, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), treeViewColumn);
	
	g_object_unref(G_OBJECT(codecStore));
	gtk_container_add(GTK_CONTAINER(scrolledWindow), treeView);
	
	// Create button box
	buttonBox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(buttonBox), 10);
	gtk_box_pack_start(GTK_BOX(ret), buttonBox, FALSE, FALSE, 0);
	
	moveUpButton = gtk_button_new_with_label("Move up");
	gtk_box_pack_start(GTK_BOX(buttonBox), moveUpButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(moveUpButton), "clicked", G_CALLBACK(moveCodecUp), codecStore);
	
	moveDownButton = gtk_button_new_with_label("Move down");
	gtk_box_pack_start(GTK_BOX(buttonBox), moveDownButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(moveDownButton), "clicked", G_CALLBACK(moveCodecDown), codecStore);
	
	config_window_fill_codec_list();

	return ret;
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
	GtkWidget *label;

    GValue val;
    val.g_type = G_TYPE_POINTER;

	selectedAccount = NULL;

	ret = gtk_vbox_new(FALSE, 10); 
	gtk_container_set_border_width (GTK_CONTAINER (ret), 10);

	label = gtk_label_new("This is the list of accounts previously setup.");

	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	gtk_box_pack_start(GTK_BOX(ret), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(ret), scrolledWindow, TRUE, TRUE, 0);

	accountStore = gtk_list_store_new(4,
			G_TYPE_STRING,  // Name
			G_TYPE_STRING,  // Protocol
			G_TYPE_STRING,  // Status
			G_TYPE_POINTER  // Pointer to the Object
			);

	treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(accountStore));

	treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW (treeView));
	g_signal_connect(G_OBJECT (treeSelection), "changed",
			G_CALLBACK (select_account),
			accountStore);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(accountStore),
			2, GTK_SORT_ASCENDING);

	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes ("Alias",
			renderer,
			"markup", 0,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(treeView), treeViewColumn);
	gtk_tree_view_column_set_cell_data_func(treeViewColumn, renderer, bold_if_default_account, NULL,NULL);

	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes ("Protocol",
			renderer,
			"markup", 1,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(treeView), treeViewColumn);
	gtk_tree_view_column_set_cell_data_func(treeViewColumn, renderer, bold_if_default_account, NULL,NULL);

	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes ("Status",
			renderer,
			"markup", 2,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(treeView), treeViewColumn);
	gtk_tree_view_column_set_cell_data_func(treeViewColumn, renderer, bold_if_default_account, NULL,NULL);
	g_object_unref(G_OBJECT(accountStore));
	gtk_container_add(GTK_CONTAINER(scrolledWindow), treeView);

	/* The buttons to press! */
	buttonBox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(buttonBox), 10); //GAIM_HIG_BOX_SPACE
	gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_START);
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
			G_CALLBACK(delete_account), NULL);
	gtk_box_pack_start(GTK_BOX(buttonBox), deleteButton, FALSE, FALSE, 0);
	gtk_widget_show(deleteButton);
	
	defaultButton = gtk_button_new_with_mnemonic("Set as Default");
	g_signal_connect_swapped(G_OBJECT(defaultButton), "clicked", 
			G_CALLBACK(default_account), NULL);
	gtk_box_pack_start(GTK_BOX(buttonBox), defaultButton, FALSE, FALSE, 0);
	gtk_widget_show(defaultButton);

	gtk_widget_show_all(ret);

	config_window_fill_account_list();

	return ret;
}

/**
 * Audio settings tab
 */
GtkWidget*
create_audio_tab ()
{
	GtkWidget * ret;
	
	GtkWidget * deviceLabel;
	GtkWidget * deviceBox;
	GtkWidget * deviceTable;
	GtkWidget * codecLabel;
	GtkWidget * codecBox;
	
	GtkWidget * attributeBox;
	GtkWidget * titleLabel;
	GtkWidget * comboBox;
	GtkWidget * refreshButton;
	GtkCellRenderer * renderer;
	GtkTreeIter iter;
	
	// Main widget
	ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);
    
    // Device section label
    deviceLabel = gtk_label_new("Devices");
	gtk_label_set_line_wrap(GTK_LABEL(deviceLabel), TRUE);
	gtk_misc_set_alignment(GTK_MISC(deviceLabel), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(deviceLabel), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(ret), deviceLabel, FALSE, FALSE, 0);
	gtk_widget_show(deviceLabel);
	
    // Main device widget
	deviceBox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(ret), deviceBox, FALSE, FALSE, 0);
	gtk_widget_show(codecBox);
	
	// TODO Create margin instead of column
    
    // Main device widget
	deviceTable = gtk_table_new(4, 3, FALSE);
	gtk_table_set_col_spacing(GTK_TABLE(deviceTable), 0, 40);
	gtk_box_set_spacing(GTK_BOX(deviceTable), 0); 				// SEE GAIM_HIG_BOX_SPACE
	gtk_box_pack_start(GTK_BOX(deviceBox), deviceTable, TRUE, TRUE, 0);
	gtk_widget_show(deviceTable);
	
	// Device : Audio manager
	// Create title label
	titleLabel = gtk_label_new("Audio manager:");
    gtk_misc_set_alignment(GTK_MISC(titleLabel), 0, 0.5);
	gtk_table_attach(GTK_TABLE(deviceTable), titleLabel, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(titleLabel);	
	// Set choices of audio managers
	audioManagerStore = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_list_store_append(audioManagerStore, &iter);
	gtk_list_store_set(audioManagerStore, &iter, 0 , "ALSA", -1);
	comboBox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(audioManagerStore));
	gtk_combo_box_set_active(GTK_COMBO_BOX(comboBox), 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(titleLabel), comboBox);
  	// Set rendering
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(comboBox), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(comboBox), renderer, "text", 0, NULL);
	gtk_table_attach(GTK_TABLE(deviceTable), comboBox, 2, 3, 0, 1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(comboBox);
	// Create refresh button
	refreshButton = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
	gtk_table_attach(GTK_TABLE(deviceTable), refreshButton, 3, 4, 0, 1, GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(refreshButton);
	
	// Device : Output device
	// Create title label
	titleLabel = gtk_label_new("Output peripheral:");
    gtk_misc_set_alignment(GTK_MISC(titleLabel), 0, 0.5);
    gtk_table_attach(GTK_TABLE(deviceTable), titleLabel, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(titleLabel);
	// Set choices of output devices
	outputAudioDeviceManagerStore = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_list_store_append(outputAudioDeviceManagerStore, &iter);
	gtk_list_store_set(outputAudioDeviceManagerStore, &iter, 0 , "Default", -1);
	comboBox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(outputAudioDeviceManagerStore));
	gtk_combo_box_set_active(GTK_COMBO_BOX(comboBox), 0);
  	gtk_label_set_mnemonic_widget(GTK_LABEL(titleLabel), comboBox);
  	// Set rendering
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(comboBox), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(comboBox), renderer, "text", 0, NULL);
	gtk_table_attach(GTK_TABLE(deviceTable), comboBox, 2, 3, 1, 2, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(comboBox);
	// Create refresh button
	refreshButton = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
	gtk_table_attach(GTK_TABLE(deviceTable), refreshButton, 3, 4, 1, 2, GTK_EXPAND, GTK_SHRINK, 0, 0);
	
	// Device : Input device
	// Create title label
	titleLabel = gtk_label_new("Input peripheral:");
    gtk_misc_set_alignment(GTK_MISC(titleLabel), 0, 0.5);
    gtk_table_attach(GTK_TABLE(deviceTable), titleLabel, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(titleLabel);
	// Set choices of output devices
	inputAudioDeviceManagerStore = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_list_store_append(inputAudioDeviceManagerStore, &iter);
	gtk_list_store_set(inputAudioDeviceManagerStore, &iter, 0 , "Default", -1);
	comboBox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(inputAudioDeviceManagerStore));
	gtk_combo_box_set_active(GTK_COMBO_BOX(comboBox), 0);
  	gtk_label_set_mnemonic_widget(GTK_LABEL(titleLabel), comboBox);
  	// Set rendering
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(comboBox), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(comboBox), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(deviceTable), comboBox, 2, 3, 2, 3, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(comboBox);
	// Create refresh button
	refreshButton = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
    gtk_table_attach(GTK_TABLE(deviceTable), refreshButton, 3, 4, 2, 3, GTK_EXPAND, GTK_SHRINK, 0, 0);

    // Codec section label
    codecLabel = gtk_label_new("Codecs");
    gtk_label_set_line_wrap(GTK_LABEL(codecLabel), TRUE);
    gtk_misc_set_alignment(GTK_MISC(codecLabel), 0, 0.5);
    gtk_label_set_justify(GTK_LABEL(codecLabel), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(ret), codecLabel, FALSE, FALSE, 0);
    gtk_widget_show(codecLabel);

    // Main codec widget
	codecBox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(ret), codecBox, FALSE, FALSE, 0);
	gtk_widget_show(codecBox);
	
	// Codec : List
	GtkWidget * codecTable;
	codecTable = create_codec_table();
	gtk_box_pack_start(GTK_BOX(codecBox), codecTable, TRUE, TRUE, 0);
	gtk_widget_show(codecTable);

	// Show all
	gtk_widget_show_all(ret);

	return ret;
}

/**
 * Main video tab creation
 * Temporary code that was in audio tab before
 */
/*GtkWidget*
create_video_tab ()
{
	GtkWidget * ret;
	GtkWidget * label;
	GtkWidget * codecBox;
	GtkWidget * rate_box;
	//GtkWidget * image; 
	GtkWidget * hbox1;
	//GtkTreeIter iter;
	GtkCellRenderer *renderer;

	ret = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width (GTK_CONTAINER (ret), 10);
	
	label = gtk_label_new("Set your video preferences.");
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_box_pack_start(GTK_BOX(ret), label, FALSE, FALSE, 0);
    gtk_widget_show(label);
	
	codecBox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(codecBox), 10); //GAIM_HIG_BOX_SPACE
	gtk_button_box_set_layout(GTK_BUTTON_BOX(codecBox), GTK_BUTTONBOX_SPREAD);
	gtk_box_pack_start(GTK_BOX(ret), codecBox, FALSE, FALSE, 0);
	gtk_widget_show (codecBox);
 
	hbox1 = gtk_label_new("Codec:");
	gtk_misc_set_alignment(GTK_MISC(hbox1), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(codecBox), hbox1, FALSE, FALSE, 0);
	gtk_widget_show(hbox1);
	codec_store = gtk_list_store_new(1, G_TYPE_STRING);
	
  	combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL (codec_store));
	gtk_label_set_mnemonic_widget(GTK_LABEL(hbox1), combo_box);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo_box), renderer, "text",0,NULL);
	gtk_box_pack_start(GTK_BOX(codecBox), combo_box, FALSE, FALSE,0);

	
	g_signal_connect (G_OBJECT (combo_box), "changed",
                        G_CALLBACK (select_codec),
                        NULL);
	gtk_widget_show(combo_box);

	restoreButton = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
	gtk_box_pack_start(GTK_BOX(codecBox), restoreButton, FALSE, FALSE,10);
	//g_signal_connect(G_OBJECT(restoreButton), "clicked", G_CALLBACK(default_codecs), NULL);
	gtk_widget_show(restoreButton);
 		
	codecBox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(codecBox), 10); //GAIM_HIG_BOX_SPACE
	gtk_button_box_set_layout(GTK_BUTTON_BOX(codecBox), GTK_BUTTONBOX_SPREAD);
	gtk_box_pack_start(GTK_BOX(ret), codecBox, FALSE, FALSE, 0);
	gtk_widget_show (codecBox);
 
	hbox1 = gtk_label_new("Sample Rate:");
        gtk_misc_set_alignment(GTK_MISC(hbox1), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(codecBox), hbox1, FALSE, FALSE, 0);
	gtk_widget_show(hbox1);
	rate_store = gtk_list_store_new(1, G_TYPE_STRING);
	
  	rate_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(rate_store));
	gtk_label_set_mnemonic_widget(GTK_LABEL(hbox1), rate_box);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(rate_box), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(rate_box), renderer, "text",0,NULL);
	gtk_box_pack_start(GTK_BOX(codecBox), rate_box, FALSE, FALSE,0);

	
	//g_signal_connect (G_OBJECT (combo_box), "changed",
          //              G_CALLBACK (select_codec),
            //            NULL);
	gtk_widget_show(rate_box);
	restoreButton = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
	gtk_box_pack_start(GTK_BOX(codecBox), restoreButton, FALSE, FALSE,0);
	//g_signal_connect(G_OBJECT(restoreButton), "clicked", G_CALLBACK(default_codecs), NULL);
	gtk_widget_show(restoreButton);
		
	
	gtk_widget_show_all(ret);
	config_window_fill_codec_list();
	config_window_fill_rate_list();
	gtk_combo_box_set_active(GTK_COMBO_BOX(rate_box), 0);
	//gtk_combo_box_set_active(rate_box, 0);

	return ret;
}*/

/**
 * Show configuration window with tabs
 */
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
	gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 400);
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 0);

	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX (dialog->vbox), notebook, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 10);
	gtk_widget_show(notebook);

	/* Create tabs */
	// Accounts tab
	tab = create_accounts_tab();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new("Accounts"));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);
	
	// Audio tab
	tab = create_audio_tab();	
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new("Audio Settings"));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

	// Video tab
/*	tab = create_video_tab();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new("Video Settings"));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);
*/
	gtk_dialog_run(dialog);

	dialogOpen = FALSE;

	gtk_widget_destroy(GTK_WIDGET(dialog));
}
