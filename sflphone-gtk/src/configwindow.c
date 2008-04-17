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

#include <accountlist.h>
#include <accountwindow.h>
#include <actions.h>
#include <config.h>
#include <dbus.h>
#include <mainwindow.h>

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>

/**
 * Local variables
 */
gboolean dialogOpen = FALSE;
gboolean ringtoneEnabled = TRUE;

GtkListStore *accountStore;
GtkWidget *codecTreeView;		// View used instead of store to get access to selection
								// instead of keeping selected codec as a variable
GtkListStore *inputAudioPluginStore;
GtkListStore *outputAudioPluginStore;
GtkListStore *outputAudioDeviceManagerStore;
GtkListStore *inputAudioDeviceManagerStore;

GtkWidget *addButton;
GtkWidget *editButton;
GtkWidget *deleteButton;
//GtkWidget *defaultButton;
GtkWidget *restoreButton;
GtkWidget *accountMoveDownButton;
GtkWidget *accountMoveUpButton;

GtkWidget *outputDeviceComboBox;
GtkWidget *inputDeviceComboBox;
GtkWidget *pluginComboBox;

GtkWidget *codecMoveUpButton;
GtkWidget *codecMoveDownButton;

GtkWidget* status;

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

// Codec properties ID
enum {
	COLUMN_CODEC_ACTIVE,
	COLUMN_CODEC_NAME,
	COLUMN_CODEC_FREQUENCY,
	COLUMN_CODEC_BITRATE,
	COLUMN_CODEC_BANDWIDTH,
	CODEC_COLUMN_COUNT
};

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
			  g_print("fill account list : %s\n" , (gchar*)g_hash_table_lookup(a->properties, ACCOUNT_ENABLED));
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
 * Fills the tree list with supported codecs
*/
void
config_window_fill_codec_list()
{
	if(dialogOpen)
	{
		GtkListStore *codecStore;
		GtkTreeIter iter;
		
		// Get model of view and clear it
		codecStore = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(codecTreeView)));
		gtk_list_store_clear(codecStore);

		// Insert codecs
		int i;
		for(i = 0; i < codec_list_get_size(); i++)
		{
			codec_t *c = codec_list_get_nth(i);
			printf("%s\n", c->name);
			if(c)
			{
				gtk_list_store_append(codecStore, &iter);
				gtk_list_store_set(codecStore, &iter,
						COLUMN_CODEC_ACTIVE,	c->is_active,									// Active
						COLUMN_CODEC_NAME,		c->name,										// Name
						COLUMN_CODEC_FREQUENCY,	g_strdup_printf("%d kHz", c->sample_rate/1000),	// Frequency (kHz)
						COLUMN_CODEC_BITRATE,	g_strdup_printf("%.1f kbps", c->_bitrate),		// Bitrate (kbps)
						COLUMN_CODEC_BANDWIDTH,	g_strdup_printf("%.1f kbps", c->_bandwidth),	// Bandwidth (kpbs)
						-1);
			}
		}
	}
}

/**
 * Fill store with input audio plugins
 */
void
config_window_fill_input_audio_plugin_list()
{
	GtkTreeIter iter;
	gchar** list;
	gchar* managerName;

	gtk_list_store_clear(inputAudioPluginStore);
	
	// Call dbus to retreive list
	list = dbus_get_input_audio_plugin_list();
	
	// For each API name included in list
	int c = 0;
	for(managerName = list[c]; managerName != NULL; managerName = list[c])
	{
		c++;
		gtk_list_store_append(inputAudioPluginStore, &iter);
		gtk_list_store_set(inputAudioPluginStore, &iter, 0 , managerName, -1);
	}
}

/**
 * Fill store with output audio plugins
 */
void
config_window_fill_output_audio_plugin_list()
{
	GtkTreeIter iter;
	gchar** list;
	gchar* managerName;

	gtk_list_store_clear(outputAudioPluginStore);
	
	// Call dbus to retreive list
	list = dbus_get_output_audio_plugin_list();
	
	// For each API name included in list
	int c = 0;
	for(managerName = list[c]; managerName != NULL; managerName = list[c])
	{
		c++;
		gtk_list_store_append(outputAudioPluginStore, &iter);
		gtk_list_store_set(outputAudioPluginStore, &iter, 0 , managerName, -1);
	}
}
/**
 * Fill output audio device store
 */
void
config_window_fill_output_audio_device_list()
{
	GtkTreeIter iter;
	gchar** list;
	gchar** audioDevice;
	int index;

	gtk_list_store_clear(outputAudioDeviceManagerStore);
	
	// Call dbus to retreive list
	list = dbus_get_audio_output_device_list();
	
	// For each device name included in list
	int c = 0;
	for(audioDevice = list; *list ; list++)
	{
		index = dbus_get_audio_device_index( *list );
		gtk_list_store_append(outputAudioDeviceManagerStore, &iter);
		gtk_list_store_set(outputAudioDeviceManagerStore, &iter, 0, *list, 1, index, -1);
		c++;
	}
}

/**
 * Select active output audio device
 */
void
select_active_output_audio_device()
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	gchar** devices;
	int currentDeviceIndex;
	int deviceIndex;

	// Select active output device on server
	devices = dbus_get_current_audio_devices_index();
	currentDeviceIndex = atoi(devices[0]);
	printf(_("audio device index for output = %d\n"), currentDeviceIndex);
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(outputDeviceComboBox));
	
	// Find the currently set output device
	gtk_tree_model_get_iter_first(model, &iter);
	do {
		gtk_tree_model_get(model, &iter, 1, &deviceIndex, -1);
		if(deviceIndex == currentDeviceIndex)
		{
			// Set current iteration the active one
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(outputDeviceComboBox), &iter);
			return;
		}
	} while(gtk_tree_model_iter_next(model, &iter));

	// No index was found, select first one
	g_print("Warning : No active output device found");
	gtk_combo_box_set_active(GTK_COMBO_BOX(outputDeviceComboBox), 0);
}

/**
 * Fill input audio device store
 */
void
config_window_fill_input_audio_device_list()
{
	GtkTreeIter iter;
	gchar** list;
	gchar** audioDevice;
	int index ;
	gtk_list_store_clear(inputAudioDeviceManagerStore);
	
	// Call dbus to retreive list
	list = dbus_get_audio_input_device_list();
	
	// For each device name included in list
	//int c = 0;
	for(audioDevice = list; *list; list++)
	{
		index = dbus_get_audio_device_index( *list );
		gtk_list_store_append(inputAudioDeviceManagerStore, &iter);
		gtk_list_store_set(inputAudioDeviceManagerStore, &iter, 0, *list, 1, index, -1);
		//c++;
	}
}

/**
 * Select active input audio device
 */
void
select_active_input_audio_device()
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	gchar** devices;
	int currentDeviceIndex;
	int deviceIndex;

	// Select active input device on server
	devices = dbus_get_current_audio_devices_index();
	currentDeviceIndex = atoi(devices[1]);
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(inputDeviceComboBox));
	
	// Find the currently set input device
	gtk_tree_model_get_iter_first(model, &iter);
	do {
		gtk_tree_model_get(model, &iter, 1, &deviceIndex, -1);
		if(deviceIndex == currentDeviceIndex)
		{
			// Set current iteration the active one
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(inputDeviceComboBox), &iter);
			return;
		}
	} while(gtk_tree_model_iter_next(model, &iter));

	// No index was found, select first one
	g_print("Warning : No active input device found");
	gtk_combo_box_set_active(GTK_COMBO_BOX(inputDeviceComboBox), 0);
}

void
update_combo_box( gchar* plugin )
{
	// set insensitive the devices widget if the selected plugin is default
	if( g_strcasecmp( plugin , "default" ) == 0)
	{
	  gtk_widget_set_sensitive( GTK_WIDGET ( outputDeviceComboBox ) , FALSE );
	  gtk_widget_set_sensitive( GTK_WIDGET ( inputDeviceComboBox ) , FALSE );
	}
	else
	{
	  gtk_widget_set_sensitive( GTK_WIDGET ( outputDeviceComboBox ) , TRUE );
	  gtk_widget_set_sensitive( GTK_WIDGET ( inputDeviceComboBox ) , TRUE );
	}
}
/**
 * Select the output audio plugin by calling the server
 */
static void
select_output_audio_plugin(GtkComboBox* widget, gpointer data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	int comboBoxIndex;
	gchar* pluginName;
	
	comboBoxIndex = gtk_combo_box_get_active(widget);
	
	if(comboBoxIndex >= 0)
	{
		model = gtk_combo_box_get_model(widget);
		gtk_combo_box_get_active_iter(widget, &iter);
		gtk_tree_model_get(model, &iter, 0, &pluginName, -1);	
		dbus_set_output_audio_plugin(pluginName);
		update_combo_box( pluginName);
	}
}

/**
 * Select active output audio plugin
 */
void
select_active_output_audio_plugin()
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	gchar* plugin;
	gchar* tmp;

	// Select active output device on server
	plugin = dbus_get_current_audio_output_plugin();
	tmp = plugin;
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(pluginComboBox));
	  
	// Find the currently alsa plugin
	gtk_tree_model_get_iter_first(model, &iter);
	do {
		gtk_tree_model_get(model, &iter, 0, &plugin , -1);
		if( g_strcasecmp( tmp , plugin ) == 0 )
		{
			// Set current iteration the active one
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(pluginComboBox), &iter);
			update_combo_box( plugin );
			return;
		}
	} while(gtk_tree_model_iter_next(model, &iter));

	// No index was found, select first one
	g_print("Warning : No active output device found\n");
	gtk_combo_box_set_active(GTK_COMBO_BOX(pluginComboBox), 0);
	update_combo_box("default");
}


/**
 * Set the audio output device on the server with its index
 */
static void
select_audio_output_device(GtkComboBox* comboBox, gpointer data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	int comboBoxIndex;
	int deviceIndex;
	
	comboBoxIndex = gtk_combo_box_get_active(comboBox);
	
	if(comboBoxIndex >= 0)
	{
		model = gtk_combo_box_get_model(comboBox);
		gtk_combo_box_get_active_iter(comboBox, &iter);
		gtk_tree_model_get(model, &iter, 1, &deviceIndex, -1);
		
		dbus_set_audio_output_device(deviceIndex);
	}
}

/**
 * Set the audio input device on the server with its index
 */
static void
select_audio_input_device(GtkComboBox* comboBox, gpointer data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	int comboBoxIndex;
	int deviceIndex;
	
	comboBoxIndex = gtk_combo_box_get_active(comboBox);
	
	if(comboBoxIndex >= 0)
	{
		model = gtk_combo_box_get_model(comboBox);
		gtk_combo_box_get_active_iter(comboBox, &iter);
		gtk_tree_model_get(model, &iter, 1, &deviceIndex, -1);
		
		dbus_set_audio_input_device(deviceIndex);
	}
}

/**
 * Refresh all audio settings
 */
static void
detect_all_audio_settings()
{
	// Update lists
	config_window_fill_output_audio_device_list();
	config_window_fill_input_audio_device_list();
	
	// Select active device in combo box
	//select_active_output_audio_device();
	//select_active_input_audio_device();
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

int 
is_ringtone_enabled( void )
{
  return dbus_is_ringtone_enabled();  
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
ringtone_enabled( void )
{
  dbus_ringtone_enabled();  
}

void
ringtone_changed( GtkFileChooser *chooser , GtkLabel *label)
{
  gchar* tone = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( chooser ));
  dbus_set_ringtone_choice( tone );
}

gchar*
get_ringtone_choice( void )
{
  return dbus_get_ringtone_choice();
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
		gtk_widget_set_sensitive(GTK_WIDGET(accountMoveUpButton), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(accountMoveDownButton), FALSE);
		return;
	}

	val.g_type = G_TYPE_POINTER;
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
	g_print("select");
}

/**
 * Toggle move buttons on if a codec is selected, off elsewise
 */
static void
select_codec(GtkTreeSelection *selection, GtkTreeModel *model)
{
	GtkTreeIter iter;
	
	if(!gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_widget_set_sensitive(GTK_WIDGET(codecMoveUpButton), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(codecMoveDownButton), FALSE);
	}
	else
	{
		gtk_widget_set_sensitive(GTK_WIDGET(codecMoveUpButton), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(codecMoveDownButton), TRUE);
	}
}

/**
 * Toggle active value of codec on click and update changes to the deamon
 * and in configuration files
 */
static void
codec_active_toggled(GtkCellRendererToggle *renderer, gchar *path, gpointer data)
{
	GtkTreeIter iter;
	GtkTreePath *treePath;
	GtkTreeModel *model;
	gboolean active;
	char* name;
	
	// Get path of clicked codec active toggle box
	treePath = gtk_tree_path_new_from_string(path);
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(data));
	gtk_tree_model_get_iter(model, &iter, treePath);

	// Get active value and name at iteration
	gtk_tree_model_get(model, &iter,
			COLUMN_CODEC_ACTIVE, &active,
			COLUMN_CODEC_NAME, &name,
			-1);
	
	printf("%s\n", name);

	// Toggle active value
	active = !active;
	
	// Store value
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			COLUMN_CODEC_ACTIVE, active,
			-1);

	gtk_tree_path_free(treePath);

	// Modify codec queue to represent change	
	if(active)
		codec_set_active(name);
	else
		codec_set_inactive(name);
	
	// Perpetuate changes to the deamon
	codec_list_update_to_daemon();
}

static void
enable_account(GtkCellRendererToggle *rend , gchar* path,  gpointer data )
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
 * Move codec in list depending on direction and selected codec and
 * update changes in the daemon list and the configuration files
 */
static void
codec_move(gboolean moveUp, gpointer data)
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
	
	// Perpetuate changes in codec queue
	if(moveUp)
		codec_list_move_codec_up(indice);
	else
		codec_list_move_codec_down(indice);
	
	// Perpetuate changes to the deamon
	codec_list_update_to_daemon();
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
}

/**
 * Called from move up codec button signal
 */
static void
codec_move_up(GtkButton *button, gpointer data)
{
  // Change tree view ordering and get indice changed
  codec_move(TRUE, data);
}

/**
 * Called from move down codec button signal
 */
static void
codec_move_down(GtkButton *button, gpointer data)
{
  // Change tree view ordering and get indice changed
  codec_move(FALSE, data);
}

/**
 * Called from move up account button signal
 */
static void
account_move_up(GtkButton *button, gpointer data)
{
  // Change tree view ordering and get indice changed
  account_move(TRUE, data);
}

/**
 * Called from move down account button signal
 */
static void
account_move_down(GtkButton *button, gpointer data)
{
  // Change tree view ordering and get indice changed
  account_move(FALSE, data);
}

GtkWidget*
create_codec_table()
{
	GtkWidget *ret;
	GtkWidget *scrolledWindow;
	GtkWidget *buttonBox;
	
	GtkListStore *codecStore;
	GtkCellRenderer *renderer;
	GtkTreeSelection *treeSelection;
	GtkTreeViewColumn *treeViewColumn;
	
	ret = gtk_hbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 10);
	
	scrolledWindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_SHADOW_IN);
	
	gtk_box_pack_start(GTK_BOX(ret), scrolledWindow, TRUE, TRUE, 0);
	codecStore = gtk_list_store_new(CODEC_COLUMN_COUNT,
			G_TYPE_BOOLEAN,		// Active
			G_TYPE_STRING,		// Name
			G_TYPE_STRING,		// Frequency
			G_TYPE_STRING,		// Bit rate
			G_TYPE_STRING		// Bandwith
			);
	
	// Create codec tree view with list store
	codecTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(codecStore));
	
	// Get tree selection manager
	treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(codecTreeView));
	g_signal_connect(G_OBJECT(treeSelection), "changed",
			G_CALLBACK (select_codec),
			codecStore);
	
	// Active column
	renderer = gtk_cell_renderer_toggle_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes("", renderer, "active", COLUMN_CODEC_ACTIVE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);

	// Toggle codec active property on clicked
	g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(codec_active_toggled), (gpointer)codecTreeView);
	
	// Name column
	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "markup", COLUMN_CODEC_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);
	
	// Bit rate column
	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes(_("Frequency"), renderer, "text", COLUMN_CODEC_FREQUENCY, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);
	
	// Bandwith column
	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes(_("Bitrate"), renderer, "text", COLUMN_CODEC_BITRATE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);
	
	// Frequency column
	renderer = gtk_cell_renderer_text_new();
	treeViewColumn = gtk_tree_view_column_new_with_attributes(_("Bandwidth"), renderer, "text", COLUMN_CODEC_BANDWIDTH, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(codecTreeView), treeViewColumn);
	
	g_object_unref(G_OBJECT(codecStore));
	gtk_container_add(GTK_CONTAINER(scrolledWindow), codecTreeView);
	
	// Create button box
	buttonBox = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(buttonBox), 10);
	gtk_box_pack_start(GTK_BOX(ret), buttonBox, FALSE, FALSE, 0);
	
	codecMoveUpButton = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
	gtk_widget_set_sensitive(GTK_WIDGET(codecMoveUpButton), FALSE);
	gtk_box_pack_start(GTK_BOX(buttonBox), codecMoveUpButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(codecMoveUpButton), "clicked", G_CALLBACK(codec_move_up), codecTreeView);
	
	codecMoveDownButton = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
	gtk_widget_set_sensitive(GTK_WIDGET(codecMoveDownButton), FALSE);
	gtk_box_pack_start(GTK_BOX(buttonBox), codecMoveDownButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(codecMoveDownButton), "clicked", G_CALLBACK(codec_move_down), codecTreeView);
	
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
	gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonBox), GTK_BUTTONBOX_CENTER);
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
	GtkWidget *ret;
	
	GtkWidget *deviceFrame;
	GtkWidget *deviceBox;
	GtkWidget *deviceTable;
	GtkWidget *codecFrame;
	GtkWidget *codecBox;
	GtkWidget *enableTone;
	GtkWidget *fileChooser;
	
	GtkWidget *titleLabel;
	GtkWidget *refreshButton;
	GtkCellRenderer *renderer;
	
	GtkWidget *codecTable;
	
	// Main widget
	ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);
    
    // Device section label
    deviceFrame = gtk_frame_new(_("Devices"));
    gtk_box_pack_start(GTK_BOX(ret), deviceFrame, FALSE, FALSE, 0);
    gtk_widget_show( deviceFrame );

	
    // Main device widget
    deviceBox = gtk_hbox_new(FALSE, 10);
    gtk_box_pack_start(GTK_BOX(deviceFrame), deviceBox, FALSE, FALSE, 0);
    gtk_widget_show( deviceBox );

    gtk_container_add( GTK_CONTAINER(deviceFrame) , deviceBox);

    // Main device widget
	deviceTable = gtk_table_new(4, 3, FALSE);
	gtk_table_set_col_spacing(GTK_TABLE(deviceTable), 0, 40);
	gtk_box_set_spacing(GTK_BOX(deviceTable), 0);
	gtk_box_pack_start(GTK_BOX(deviceBox), deviceTable, TRUE, TRUE, 0);
	gtk_widget_show(deviceTable);
	
	// Device : Audio manager
	// Create title label
	/*
	titleLabel = gtk_label_new("Alsa plug-IN:");
	gtk_misc_set_alignment(GTK_MISC(titleLabel), 0, 0.5);
	gtk_table_attach(GTK_TABLE(deviceTable), titleLabel, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(titleLabel);
	// Set choices of audio managers
	inputAudioPluginStore = gtk_list_store_new(1, G_TYPE_STRING);
	config_window_fill_input_audio_plugin_list();
	comboBox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(inputAudioPluginStore));
	gtk_combo_box_set_active(GTK_COMBO_BOX(comboBox), 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(titleLabel), comboBox);
	g_signal_connect(G_OBJECT(comboBox), "changed", G_CALLBACK(select_input_audio_plugin), comboBox);
	
  	// Set rendering
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(comboBox), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(comboBox), renderer, "text", 0, NULL);
	gtk_table_attach(GTK_TABLE(deviceTable), comboBox, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(comboBox);
	*/
	// Create title label
	titleLabel = gtk_label_new(_("ALSA plugin"));
	gtk_misc_set_alignment(GTK_MISC(titleLabel), 0, 0.5);
	gtk_table_attach(GTK_TABLE(deviceTable), titleLabel, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(titleLabel);
	// Set choices of audio managers
	outputAudioPluginStore = gtk_list_store_new(1, G_TYPE_STRING);
	config_window_fill_output_audio_plugin_list();
	pluginComboBox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(outputAudioPluginStore));
	select_active_output_audio_plugin();
	gtk_label_set_mnemonic_widget(GTK_LABEL(titleLabel), pluginComboBox);
	g_signal_connect(G_OBJECT(pluginComboBox), "changed", G_CALLBACK(select_output_audio_plugin), pluginComboBox);
	
  	// Set rendering
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(pluginComboBox), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(pluginComboBox), renderer, "text", 0, NULL);
	gtk_table_attach(GTK_TABLE(deviceTable), pluginComboBox, 2, 3, 0, 1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(pluginComboBox);
	
	// Device : Output device
	// Create title label
	titleLabel = gtk_label_new(_("Output peripheral"));
    gtk_misc_set_alignment(GTK_MISC(titleLabel), 0, 0.5);
    gtk_table_attach(GTK_TABLE(deviceTable), titleLabel, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
    gtk_widget_show(titleLabel);
	// Set choices of output devices
	outputAudioDeviceManagerStore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	config_window_fill_output_audio_device_list();
	outputDeviceComboBox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(outputAudioDeviceManagerStore));
	select_active_output_audio_device();
  	gtk_label_set_mnemonic_widget(GTK_LABEL(titleLabel), outputDeviceComboBox);
	g_signal_connect(G_OBJECT(outputDeviceComboBox), "changed", G_CALLBACK(select_audio_output_device), outputDeviceComboBox);

	// Set rendering
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(outputDeviceComboBox), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(outputDeviceComboBox), renderer, "text", 0, NULL);
	gtk_table_attach(GTK_TABLE(deviceTable), outputDeviceComboBox, 2, 3, 1, 2, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(outputDeviceComboBox);
	
	// Device : Input device
	// Create title label
	titleLabel = gtk_label_new(_("Input peripheral"));
    gtk_misc_set_alignment(GTK_MISC(titleLabel), 0, 0.5);
    gtk_table_attach(GTK_TABLE(deviceTable), titleLabel, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(titleLabel);
	// Set choices of output devices
	inputAudioDeviceManagerStore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	config_window_fill_input_audio_device_list();
	inputDeviceComboBox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(inputAudioDeviceManagerStore));
	select_active_input_audio_device();
	gtk_label_set_mnemonic_widget(GTK_LABEL(titleLabel), inputDeviceComboBox);
	g_signal_connect(G_OBJECT(inputDeviceComboBox), "changed", G_CALLBACK(select_audio_input_device), inputDeviceComboBox);

	// Set rendering
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(inputDeviceComboBox), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(inputDeviceComboBox), renderer, "text", 0, NULL);
    gtk_table_attach(GTK_TABLE(deviceTable), inputDeviceComboBox, 2, 3, 2, 3, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
	gtk_widget_show(inputDeviceComboBox);
	
	// Create detect button
	refreshButton = gtk_button_new_with_label(_("Detect all"));
	gtk_button_set_image(GTK_BUTTON(refreshButton), gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_BUTTON));
	gtk_table_attach(GTK_TABLE(deviceTable), refreshButton, 2, 3, 3, 4, GTK_EXPAND, GTK_EXPAND, 0, 0);
	// Set event on selection
	g_signal_connect(G_OBJECT(refreshButton), "clicked", G_CALLBACK(detect_all_audio_settings), NULL);
	
	//select_active_output_audio_plugin();
    // Codec section label
    codecFrame = gtk_frame_new(_("Codecs"));
    gtk_misc_set_alignment(GTK_MISC(codecFrame), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(ret), codecFrame, FALSE, FALSE, 0);
    gtk_widget_show(codecFrame);

    // Main codec widget
	codecBox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(codecFrame), codecBox, FALSE, FALSE, 0);
	gtk_widget_show(codecBox);
	
	gtk_container_add( GTK_CONTAINER( codecFrame ) , codecBox );
	// Codec : List
	codecTable = create_codec_table();
	gtk_widget_set_size_request(GTK_WIDGET(codecTable), -1, 150);
	gtk_box_pack_start(GTK_BOX(codecBox), codecTable, TRUE, TRUE, 0);
	gtk_widget_show(codecTable);

    // check button to enable ringtones
	GtkWidget* box = gtk_hbox_new( TRUE , 1);
	gtk_box_pack_start( GTK_BOX(ret) , box , FALSE , FALSE , 1);
	enableTone = gtk_check_button_new_with_mnemonic( _("_Enable ringtones"));
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(enableTone), dbus_is_ringtone_enabled() );
      gtk_box_pack_start( GTK_BOX(box) , enableTone , TRUE , TRUE , 1);
      g_signal_connect(G_OBJECT( enableTone) , "clicked" , G_CALLBACK( ringtone_enabled ) , NULL);
    // file chooser button
	fileChooser = gtk_file_chooser_button_new(_("Choose a ringtone"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER( fileChooser) , g_get_home_dir());	
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER( fileChooser) , get_ringtone_choice());	
	g_signal_connect( G_OBJECT( fileChooser ) , "selection_changed" , G_CALLBACK( ringtone_changed ) , NULL );
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name( filter , _("Audio Files") );
	gtk_file_filter_add_pattern(filter , "*.wav" );
	gtk_file_filter_add_pattern(filter , "*.ul" );
	gtk_file_filter_add_pattern(filter , "*.au" );
	gtk_file_chooser_add_filter( GTK_FILE_CHOOSER( fileChooser ) , filter);
	gtk_box_pack_start( GTK_BOX(box) , fileChooser , TRUE , TRUE , 1);

	// Show all
	gtk_widget_show_all(ret);

	return ret;
}

GtkWidget*
create_general_settings ()
{
  GtkWidget *ret;

  GtkWidget *notifFrame;
  GtkWidget *notifBox;
  GtkWidget *notifAll;
  GtkWidget *notifIncoming;
  GtkWidget *notifMails;

  GtkWidget *trayFrame;
  GtkWidget *trayBox;
  GtkWidget *trayItem;

  // Main widget
  ret = gtk_vbox_new(FALSE, 10);
  gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

  // Notifications Frame
  notifFrame = gtk_frame_new(_("Notifications"));
  gtk_box_pack_start(GTK_BOX(ret), notifFrame, FALSE, FALSE, 0);
  gtk_widget_show( notifFrame );

  notifBox = gtk_vbox_new(FALSE, 10);
  gtk_box_pack_start(GTK_BOX(notifFrame), notifBox, FALSE, FALSE, 0);
  gtk_widget_show( notifBox );
  gtk_container_add( GTK_CONTAINER(notifFrame) , notifBox);
  
  notifAll = gtk_radio_button_new_with_label( NULL, _("Enable All"));
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(notifAll), TRUE );
  gtk_box_pack_start( GTK_BOX(notifBox) , notifAll , TRUE , TRUE , 1);
  //TODO callback

  notifIncoming = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(notifAll) , _("Only Incoming Calls"));
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(notifIncoming), FALSE );
  gtk_box_pack_start( GTK_BOX(notifBox) , notifIncoming , TRUE , TRUE , 1);
  //TODO callback

  notifMails = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(notifAll) , _("Only Voice Mails"));
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(notifMails), FALSE );
  gtk_box_pack_start( GTK_BOX(notifBox) , notifMails , TRUE , TRUE , 1);
  //TODO callback

  // System Tray option frame
  trayFrame = gtk_frame_new(_("System Tray Icon"));
  gtk_box_pack_start(GTK_BOX(ret), trayFrame, FALSE, FALSE, 0);
  gtk_widget_show( trayFrame );

  trayBox = gtk_vbox_new(FALSE, 10);
  gtk_box_pack_start(GTK_BOX(trayFrame), trayBox, FALSE, FALSE, 0);
  gtk_widget_show( trayBox );
  gtk_container_add( GTK_CONTAINER(trayFrame) , trayBox);
  
  GtkWidget* trayItem1 = gtk_radio_button_new_with_label(NULL,  _("Popup Main Window On Incoming Call"));
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(trayItem1), dbus_popup_mode() );
  gtk_box_pack_start( GTK_BOX(trayBox) , trayItem1 , TRUE , TRUE , 1);
  g_signal_connect(G_OBJECT( trayItem1 ) , "clicked" , G_CALLBACK( set_popup_mode ) , NULL);

  trayItem = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(trayItem1), _("Never Popup Main Window"));
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(trayItem), !dbus_popup_mode() );
  gtk_box_pack_start( GTK_BOX(trayBox) , trayItem , TRUE , TRUE , 1);
  
  trayItem = gtk_check_button_new_with_label(_("Start Hidden"));
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(trayItem), dbus_is_start_hidden() );
  gtk_box_pack_start( GTK_BOX(trayBox) , trayItem , TRUE , TRUE , 1);
  g_signal_connect(G_OBJECT( trayItem ) , "clicked" , G_CALLBACK( start_hidden ) , NULL);

  gtk_widget_show_all(ret);
  return ret;
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

	dialogOpen = TRUE;

	dialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Preferences"),
				GTK_WINDOW(get_main_window()),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_CLOSE,
				GTK_RESPONSE_ACCEPT,
				NULL));

	// Set window properties
      	gtk_dialog_set_has_separator(dialog, FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 400);
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
	tab = create_audio_tab();	
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Audio Settings")));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

	gtk_dialog_run(dialog);
	//gtk_widget_show( GTK_WIDGET(dialog) );
	//g_signal_connect_swapped( dialog , "response" , G_CALLBACK( gtk_widget_destroy ), dialog );

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

  dialogOpen = TRUE;

  dialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Accounts"),
                              GTK_WINDOW(get_main_window()),
                              GTK_DIALOG_DESTROY_WITH_PARENT,
                              GTK_STOCK_CLOSE,
                              GTK_RESPONSE_ACCEPT,
                              NULL));

        // Set window properties
        gtk_dialog_set_has_separator(dialog, FALSE);
        gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 400);
        gtk_container_set_border_width(GTK_CONTAINER(dialog), 0);

	accountFrame = gtk_frame_new( _("Accounts previously setup"));
	gtk_box_pack_start( GTK_BOX( dialog->vbox ), accountFrame , TRUE, TRUE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(accountFrame), 10);
        gtk_widget_show(accountFrame);

	// Accounts tab
        tab = create_accounts_tab();

	gtk_container_add(GTK_CONTAINER(accountFrame) , tab);

      gtk_dialog_run( dialog );
      dialogOpen=FALSE;
      gtk_widget_destroy(GTK_WIDGET(dialog));
}

