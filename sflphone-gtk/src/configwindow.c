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
GtkListStore * account_store;
GtkListStore * codec_store;
GtkListStore * rate_store;
GtkWidget * addButton;
GtkWidget * editButton;
GtkWidget * deleteButton;
GtkWidget * defaultButton;
GtkWidget * restoreButton;
GtkWidget * combo_box;

account_t * selectedAccount;

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

void
config_window_fill_codec_list()
{
  if(dialogOpen)
  {
    GtkTreeIter iter;
    int i;
    gtk_list_store_clear(codec_store);
    gchar * description = "Select a codec:";
    //gtk_list_store_append(codec_store, &iter);
    //gtk_list_store_set(codec_store, &iter, 0, description, -1);
    for(i=0; i<codec_list_get_size(); i++)
    {
      codec_t* c = codec_list_get_nth(i); 
      printf("%s\n",c->name);
      if(c)
      {
        gtk_list_store_append (codec_store, &iter);
        gtk_list_store_set(codec_store, &iter,0,c->name,-1);
      }
    }
  }
	gtk_combo_box_set_active(combo_box, 0);
}

void
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

static void
select_codec( GtkComboBox* wid)
{
	guint item = gtk_combo_box_get_active(wid);
	/* now we want this selected codec to be used as the preferred codec */
	/* ie first in the list in the user config */
	codec_set_prefered_order(item);
	dbus_set_prefered_codec(codec_list_get_nth(0)->name);  
}

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
	if(g_strcasecmp(current->accountID,account_list_get_default())==0)
		g_object_set (G_OBJECT(rend),"weight",800 ,NULL);
	else
		g_object_set(G_OBJECT(rend), "weight", 400, NULL);

}

void
default_codecs(GtkWidget* widget, gpointer data)
{
  int i=0;
  int j=0;
  gint * new_order;
  gchar ** default_list = (gchar**)dbus_default_codec_list();
  while(default_list[i] != NULL)
    {printf("%s\n", default_list[i]);
     i++;}
  i=0;
  while(default_list[i] != NULL)
  {
    if(g_strcasecmp(codec_list_get_nth(0)->name ,default_list[i])==0){
      printf("%s %s\n",codec_list_get_nth(0)->name, default_list[i]);
      new_order[i]=0;
   }
    else if(g_strcasecmp(codec_list_get_nth(1)->name ,default_list[i])==0){
      printf("%s %s\n",codec_list_get_nth(0)->name, default_list[0]);
      new_order[i] = 1;}	
    else{
      printf("%s %s\n",codec_list_get_nth(0)->name, default_list[0]);
      new_order[i] = 2;}
    printf("new_order[%i]=%i\n", i,j);
   i++;
  } 
  gtk_list_store_reorder(codec_store, new_order);  
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

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(account_store),
			2, GTK_SORT_ASCENDING);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("Alias",
			rend,
			"markup", 0,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);
	gtk_tree_view_column_set_cell_data_func(col, rend, bold_if_default_account, NULL,NULL);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("Protocol",
			rend,
			"markup", 1,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);
	gtk_tree_view_column_set_cell_data_func(col, rend, bold_if_default_account, NULL,NULL);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("Status",
			rend,
			"markup", 2,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);
	gtk_tree_view_column_set_cell_data_func(col, rend, bold_if_default_account, NULL,NULL);
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

GtkWidget*
create_audio_tab ()
{
	GtkWidget * ret;
	GtkWidget * label;
	GtkWidget * codecBox;
	GtkWidget * rate_box;
	GtkWidget * image; 
	GtkWidget * hbox1;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	

	ret = gtk_vbox_new(FALSE, 10);
        gtk_container_set_border_width (GTK_CONTAINER (ret), 10);
	
	label = gtk_label_new("Set your audio preferences.");
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
	gtk_combo_box_set_active(rate_box, 0);

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
	// Accounts tab
	tab = create_accounts_tab();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new("Accounts"));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);
	
	// Audio tab
	tab = create_audio_tab();	
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new("Audio Settings"));
	gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);	

	gtk_dialog_run (dialog);

	dialogOpen = FALSE;

	gtk_widget_destroy (GTK_WIDGET(dialog));
}

