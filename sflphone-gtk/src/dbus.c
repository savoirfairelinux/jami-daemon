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
#include <calllist.h>
#include <calltab.h>
#include <callmanager-glue.h>
#include <configurationmanager-glue.h>
#include <instance-glue.h>
#include <configwindow.h>
#include <mainwindow.h>
#include <marshaller.h>
#include <sliders.h>
#include <statusicon.h>
#include <assistant.h>

#include <dbus.h>
#include <actions.h>
#include <string.h>
#include <dbus/dbus-glib.h>

DBusGConnection * connection;
DBusGProxy * callManagerProxy;
DBusGProxy * configurationManagerProxy;
DBusGProxy * instanceProxy;

static void  
incoming_call_cb (DBusGProxy *proxy,
                  const gchar* accountID,
                  const gchar* callID,
                  const gchar* from,
                  void * foo  )
{
  g_print ("Incoming call! %s\n",callID);
  call_t * c = g_new0 (call_t, 1);
  c->accountID = g_strdup(accountID);
  c->callID = g_strdup(callID);
  c->from = g_strdup(from);
  c->state = CALL_STATE_INCOMING;
 #if GTK_CHECK_VERSION(2,10,0) 
  status_tray_icon_blink( TRUE );
 #endif
  notify_incoming_call( c );
  sflphone_incoming_call (c);
}


static void  
volume_changed_cb (DBusGProxy *proxy,
                  const gchar* device,
                  const gdouble value,
                  void * foo  )
{
  g_print ("Volume of %s changed to %f. \n",device, value);
  set_slider(device, value);
}

static void  
voice_mail_cb (DBusGProxy *proxy,
                  const gchar* accountID,
                  const guint nb,
                  void * foo  )
{
  g_print ("%d Voice mail waiting! \n",nb);
  sflphone_notify_voice_mail (accountID , nb);
}

static void  
incoming_message_cb (DBusGProxy *proxy,
                  const gchar* accountID,
                  const gchar* msg,
                  void * foo  )
{
  g_print ("Message %s! \n",msg);
  
}

static void  
call_state_cb (DBusGProxy *proxy,
                  const gchar* callID,
                  const gchar* state,
                  void * foo  )
{
  g_print ("Call %s state %s\n",callID, state);
  call_t * c = call_list_get(current_calls, callID);
  if(c)
  {
    if ( strcmp(state, "HUNGUP") == 0 )
    {
      if(c->state==CALL_STATE_CURRENT)
      {
	// peer hung up, the conversation was established, so _start has been initialized with the current time value
	g_print("call state current\n");
	(void) time(&c->_stop);
	update_call_tree( history, c );
      }
      stop_notification();
      sflphone_hung_up (c);
      update_call_tree( history, c );
    }
    else if ( strcmp(state, "UNHOLD") == 0 )
    {
      sflphone_current (c);
    }
    else if ( strcmp(state, "HOLD") == 0 )
    {
      sflphone_hold (c);
    }
    else if ( strcmp(state, "RINGING") == 0 )
    {
      sflphone_ringing (c);
    }
    else if ( strcmp(state, "CURRENT") == 0 )
    {
      sflphone_current (c);
    }
    else if ( strcmp(state, "FAILURE") == 0 )
    {
      sflphone_fail (c);
    }
    else if ( strcmp(state, "BUSY") == 0 )
    {
      sflphone_busy (c);
    }
  } 
  else 
  { //The callID is unknow, threat it like a new call
    if ( strcmp(state, "RINGING") == 0 )
    {
      g_print ("New ringing call! %s\n",callID);
      call_t * c = g_new0 (call_t, 1);
      c->accountID = g_strdup("1");
      c->callID = g_strdup(callID);
      c->from = g_strdup("\"\" <>");
      c->state = CALL_STATE_RINGING;
      sflphone_incoming_call (c);
    }
  }
}

static void  
accounts_changed_cb (DBusGProxy *proxy,
                  void * foo  )
{
  g_print ("Accounts changed\n");
  sflphone_fill_account_list(TRUE);
  config_window_fill_account_list();
}

static void  
error_alert(DBusGProxy *proxy,
		  int errCode,
                  void * foo  )
{
  g_print ("Error notifying : (%i)\n" , errCode);
  sflphone_throw_exception( errCode );
}

gboolean 
dbus_connect ()
{
  GError *error = NULL;
  
  g_type_init ();

  error = NULL;
  connection = dbus_g_bus_get (DBUS_BUS_SESSION,
                               &error);
  if (connection == NULL)
  {
    g_printerr ("Failed to open connection to bus: %s\n",
                error->message);
    g_error_free (error);
    return FALSE;
  }

  /* Create a proxy object for the "bus driver" (name "org.freedesktop.DBus") */
  
  instanceProxy = dbus_g_proxy_new_for_name (connection,
                                     "org.sflphone.SFLphone",
                                     "/org/sflphone/SFLphone/Instance",
                                     "org.sflphone.SFLphone.Instance");
  if (!instanceProxy) 
  {
    g_printerr ("Failed to get proxy to Instance\n");
    return FALSE;
  }
  
  g_print ("DBus connected to Instance\n");
  
  
  callManagerProxy = dbus_g_proxy_new_for_name (connection,
                                     "org.sflphone.SFLphone",
                                     "/org/sflphone/SFLphone/CallManager",
                                     "org.sflphone.SFLphone.CallManager");
  if (!callManagerProxy) 
  {
    g_printerr ("Failed to get proxy to CallManagers\n");
    return FALSE;
  }
  
  g_print ("DBus connected to CallManager\n");
  /* Incoming call */
  dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_STRING_STRING, 
    G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (callManagerProxy, 
    "incomingCall", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (callManagerProxy,
    "incomingCall", G_CALLBACK(incoming_call_cb), NULL, NULL);

  /* Register a marshaller for STRING,STRING */
  dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_STRING, 
    G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (callManagerProxy, 
    "callStateChanged", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (callManagerProxy,
    "callStateChanged", G_CALLBACK(call_state_cb), NULL, NULL);

  dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_INT, 
    G_TYPE_NONE, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (callManagerProxy, 
    "voiceMailNotify", G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (callManagerProxy,
    "voiceMailNotify", G_CALLBACK(voice_mail_cb), NULL, NULL);
  
  dbus_g_proxy_add_signal (callManagerProxy, 
    "incomingMessage", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (callManagerProxy,
    "incomingMessage", G_CALLBACK(incoming_message_cb), NULL, NULL);
    
  dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_DOUBLE, 
    G_TYPE_NONE, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (callManagerProxy, 
    "volumeChanged", G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (callManagerProxy,
    "volumeChanged", G_CALLBACK(volume_changed_cb), NULL, NULL);
    
  configurationManagerProxy = dbus_g_proxy_new_for_name (connection,
                                  "org.sflphone.SFLphone",
                                  "/org/sflphone/SFLphone/ConfigurationManager",
                                  "org.sflphone.SFLphone.ConfigurationManager");
  if (!configurationManagerProxy) 
  {
    g_printerr ("Failed to get proxy to ConfigurationManager\n");
    return FALSE;
  }
  g_print ("DBus connected to ConfigurationManager\n");
  dbus_g_proxy_add_signal (configurationManagerProxy, 
    "accountsChanged", G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (configurationManagerProxy,
    "accountsChanged", G_CALLBACK(accounts_changed_cb), NULL, NULL);
   
  dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__INT,
          G_TYPE_NONE, G_TYPE_INT , G_TYPE_INVALID);
  dbus_g_proxy_add_signal (configurationManagerProxy, 
    "errorAlert", G_TYPE_INT , G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (configurationManagerProxy,
    "errorAlert", G_CALLBACK(error_alert), NULL, NULL);
  return TRUE;
}

void
dbus_clean ()
{
    g_object_unref (callManagerProxy);
    g_object_unref (configurationManagerProxy);
}


void
dbus_hold (const call_t * c)
{
  GError *error = NULL;
  org_sflphone_SFLphone_CallManager_hold ( callManagerProxy, c->callID, &error);
  if (error) 
  {
    g_printerr ("Failed to call hold() on CallManager: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called hold() on CallManager\n");
  }

}

void
dbus_unhold (const call_t * c)
{
  GError *error = NULL;
  org_sflphone_SFLphone_CallManager_unhold ( callManagerProxy, c->callID, &error);
  if (error) 
  {
    g_printerr ("Failed to call unhold() on CallManager: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called unhold() on CallManager\n");
  
  }
}

void
dbus_hang_up (const call_t * c)
{
  GError *error = NULL;
  org_sflphone_SFLphone_CallManager_hang_up ( callManagerProxy, c->callID, &error);
  if (error) 
  {
    g_printerr ("Failed to call hang_up() on CallManager: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called hang_up() on CallManager\n");
  }

}

void
dbus_transfert (const call_t * c)
{
  GError *error = NULL;
  org_sflphone_SFLphone_CallManager_transfert ( callManagerProxy, c->callID, c->to, &error);
  if (error) 
  {
    g_printerr ("Failed to call transfert() on CallManager: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called transfert() on CallManager\n");
  }

}

void
dbus_accept (const call_t * c)
{
#if GTK_CHECK_VERSION(2,10,0)
  status_tray_icon_blink( FALSE );
#endif
  GError *error = NULL;
  org_sflphone_SFLphone_CallManager_accept ( callManagerProxy, c->callID, &error);
  if (error) 
  {
    g_printerr ("Failed to call accept(%s) on CallManager: %s\n", c->callID,
                (error->message == NULL ? g_quark_to_string(error->domain): error->message));
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called accept(%s) on CallManager\n", c->callID);
  }

}

void
dbus_refuse (const call_t * c)
{
#if GTK_CHECK_VERSION(2,10,0)
  status_tray_icon_blink( FALSE );
#endif
  GError *error = NULL;
  org_sflphone_SFLphone_CallManager_refuse ( callManagerProxy, c->callID, &error);
  if (error) 
  {
    g_printerr ("Failed to call refuse() on CallManager: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called refuse() on CallManager\n");
  
  }

}


void
dbus_place_call (const call_t * c)
{
  GError *error = NULL;
  org_sflphone_SFLphone_CallManager_place_call ( callManagerProxy, c->accountID, c->callID, c->to, &error);
  if (error) 
  {
    g_printerr ("Failed to call placeCall() on CallManager: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called placeCall() on CallManager\n");
  
  }

}

gchar ** 
dbus_account_list()
{
  g_print("Before");
  
  GError *error = NULL;
  char ** array;
  org_sflphone_SFLphone_ConfigurationManager_get_account_list (
    configurationManagerProxy, 
    &array, 
    &error);
    
  g_print("After");
  if (error) 
  {
  g_printerr ("Failed to call get_account_list() on ConfigurationManager: %s\n",
              error->message);
  g_error_free (error);
  } 
  else 
  {
  g_print ("DBus called get_account_list() on ConfigurationManager\n");

  }
  return array;
}

GHashTable * 
dbus_account_details(gchar * accountID)
{
  GError *error = NULL;
  GHashTable * details;
  org_sflphone_SFLphone_ConfigurationManager_get_account_details (
    configurationManagerProxy, 
    accountID, 
    &details, 
    &error);
  if (error) 
  {
    g_printerr ("Failed to call get_account_details() on ConfigurationManager: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called get_account_details() on ConfigurationManager\n");

  }
  return details;
}

void
dbus_send_register ( gchar* accountID , const guint expire)
{
  GError *error = NULL;
  org_sflphone_SFLphone_ConfigurationManager_send_register ( configurationManagerProxy, accountID, expire ,&error);
  if (error) 
  {
    g_printerr ("Failed to call send_register() on ConfigurationManager: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called send_register() on ConfigurationManager\n");
  }
}

void
dbus_remove_account(gchar * accountID)
{
  GError *error = NULL;
  org_sflphone_SFLphone_ConfigurationManager_remove_account (
    configurationManagerProxy, 
    accountID, 
    &error);
  if (error) 
  {
  g_printerr ("Failed to call remove_account() on ConfigurationManager: %s\n",
              error->message);
  g_error_free (error);
  } 
  else 
  {
  g_print ("DBus called remove_account() on ConfigurationManager\n");

  }
}

void
dbus_set_account_details(account_t *a)
{
  GError *error = NULL;
  org_sflphone_SFLphone_ConfigurationManager_set_account_details (
    configurationManagerProxy, 
    a->accountID, 
    a->properties, 
    &error);
  if (error) 
  {
    g_printerr ("Failed to call set_account_details() on ConfigurationManager: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called set_account_details() on ConfigurationManager\n");

  }
}

void
dbus_add_account(account_t *a)
{
  GError *error = NULL;
  org_sflphone_SFLphone_ConfigurationManager_add_account (
    configurationManagerProxy, 
    a->properties, 
    &error);
  if (error) 
  {
    g_printerr ("Failed to call add_account() on ConfigurationManager: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called add_account() on ConfigurationManager\n");

  }
}

void
dbus_set_volume(const gchar * device, gdouble value)
{
  GError *error = NULL;
  org_sflphone_SFLphone_CallManager_set_volume(
    callManagerProxy, 
    device, 
    value, 
    &error);

  if (error) 
  {
    g_printerr ("Failed to call set_volume() on callManagerProxy: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called set_volume() on callManagerProxy\n");

  }
}


gdouble
dbus_get_volume(const gchar * device)
{
  gdouble  value;
  GError *error = NULL;
  
  org_sflphone_SFLphone_CallManager_get_volume(
    callManagerProxy, 
    device, 
    &value, 
    &error);

  if (error) 
  {
    g_printerr ("Failed to call get_volume() on callManagerProxy: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called get_volume(%s) on callManagerProxy, got %f\n", device, value);

  }
  return value;
}


void
dbus_play_dtmf(const gchar * key)
{
  GError *error = NULL;
  
  org_sflphone_SFLphone_CallManager_play_dt_mf(
    callManagerProxy, 
    key, 
    &error);

  if (error) 
  {
    g_printerr ("Failed to call playDTMF() on callManagerProxy: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called playDTMF() on callManagerProxy\n");

  }
}

void
dbus_start_tone(const int start , const guint type )
{
  GError *error = NULL;
  
  org_sflphone_SFLphone_CallManager_start_tone(
    callManagerProxy, 
    start,
    type, 
    &error);

  if (error) 
  {
    g_printerr ("Failed to call startTone() on callManagerProxy: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called startTone() on callManagerProxy\n");

  }
}

void
dbus_register(int pid, gchar * name)
{
  GError *error = NULL;
  
  org_sflphone_SFLphone_Instance_register(
    instanceProxy, 
    pid, 
    name, 
    &error);

  if (error) 
  {
    g_printerr ("Failed to call register() on instanceProxy: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called register() on instanceProxy\n");

  }
}

void 
dbus_unregister(int pid)
{
  GError *error = NULL;
  
  org_sflphone_SFLphone_Instance_unregister(
    instanceProxy, 
    pid, 
    &error);

  if (error) 
  {
    g_printerr ("Failed to call unregister() on instanceProxy: %s\n",
                error->message);
    g_error_free (error);
  } 
  else 
  {
    g_print ("DBus called unregister() on instanceProxy\n");
  }
}

gchar**
dbus_codec_list()
{
  g_print("Before");

  GError *error = NULL;
  gchar** array;
  org_sflphone_SFLphone_ConfigurationManager_get_codec_list (
    configurationManagerProxy,
    &array,
    &error);

  g_print("After");
  if (error)
  {
  g_printerr ("Failed to call get_codec_list() on ConfigurationManager: %s\n",
              error->message);
  g_error_free (error);
  }
  else
  {
  g_print ("DBus called get_codec_list() on ConfigurationManager\n");

  }
  return array;
}

gchar**
dbus_codec_details( int payload )
{
  g_print("Before");

  GError *error = NULL;
  gchar ** array;
  org_sflphone_SFLphone_ConfigurationManager_get_codec_details (
    configurationManagerProxy,
    payload,
    &array,
    &error);

  g_print("After");
  if (error)
  {
  g_printerr ("Failed to call get_codec_details() on ConfigurationManager: %s\n",
              error->message);
  g_error_free (error);
  }
  else
  {
  g_print ("DBus called get_codec_details() on ConfigurationManager\n");

  }
  return array;
}



gchar**
dbus_get_active_codec_list()
{
  g_print("Before");

  gchar ** array;
  GError *error = NULL;
  org_sflphone_SFLphone_ConfigurationManager_get_active_codec_list (
    configurationManagerProxy,
    &array,
    &error);

  g_print("After");
  if (error)
  {
  g_printerr ("Failed to call get_active_codec_list() on ConfigurationManager: %s\n",
              error->message);
  g_error_free (error);
  }
  else
  {
  g_print ("DBus called get_active_codec_list() on ConfigurationManager\n");

  }
  return array;
}

void
dbus_set_active_codec_list(const gchar** list)
{
  g_print("Before");

  GError *error = NULL;
  org_sflphone_SFLphone_ConfigurationManager_set_active_codec_list (
    configurationManagerProxy,
    list,
    &error);

  g_print("After");
  if (error)
  {
  g_printerr ("Failed to call set_active_codec_list() on ConfigurationManager: %s\n",
              error->message);
  g_error_free (error);
  }
  else
  {
  g_print ("DBus called set_active_codec_list() on ConfigurationManager\n");

  }
}

/**
 * Get a list of input supported audio plugins
 */
gchar**
dbus_get_input_audio_plugin_list()
{
	g_print("Before get input audio plugin list");
	gchar** array;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_input_audio_plugin_list(
			configurationManagerProxy,
			&array,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call get_input_audio_plugin_list() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called get_input_audio_plugin_list() on ConfigurationManager\n");
	return array;
}

/**
 * Get a list of output supported audio plugins
 */
gchar**
dbus_get_output_audio_plugin_list()
{
	g_print("Before get output audio plugin list");
	gchar** array;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_output_audio_plugin_list(
			configurationManagerProxy,
			&array,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call get_output_audio_plugin_list() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called get_output_audio_plugin_list() on ConfigurationManager\n");
	return array;
}

void
dbus_set_input_audio_plugin(gchar* audioPlugin)
{
	g_print("Before set input audio plugin");
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_input_audio_plugin(
			configurationManagerProxy,
			audioPlugin,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call set_input_audio_plugin() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called set_input_audio_plugin() on ConfigurationManager\n");
}

void
dbus_set_output_audio_plugin(gchar* audioPlugin)
{
	g_print("Before set output audio plugin");
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_output_audio_plugin(
			configurationManagerProxy,
			audioPlugin,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call set_output_audio_plugin() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called set_output_audio_plugin() on ConfigurationManager\n");
}

/**
 * Get all output devices index supported by current audio manager
 */
gchar** dbus_get_audio_output_device_list()
{
	g_print("Before get audio output device list");
	gchar** array;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_audio_output_device_list(
			configurationManagerProxy,
			&array,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call get_audio_output_device_list() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called get_audio_output_device_list() on ConfigurationManager\n");
	return array;
}

/**
 * Set audio output device from its index
 */
void
dbus_set_audio_output_device(const int index)
{
	g_print("Before set audio output device");
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_audio_output_device(
			configurationManagerProxy,
			index,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call set_audio_output_device() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called set_audio_output_device() on ConfigurationManager\n");
}

/**
 * Get all input devices index supported by current audio manager
 */
gchar**
dbus_get_audio_input_device_list()
{
	g_print("Before get audio input device list");
	gchar** array;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_audio_input_device_list(
			configurationManagerProxy,
			&array,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call get_audio_input_device_list() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called get_audio_input_device_list() on ConfigurationManager\n");
	return array;
}

/**
 * Set audio input device from its index
 */
void
dbus_set_audio_input_device(const int index)
{
	g_print("Before set audio input device");
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_audio_input_device(
			configurationManagerProxy,
			index,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call set_audio_input_device() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called set_audio_input_device() on ConfigurationManager\n");
}

/**
 * Get output device index and input device index
 */
gchar**
dbus_get_current_audio_devices_index()
{
	g_print("Before get current audio devices index");
	gchar** array;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_current_audio_devices_index(
			configurationManagerProxy,
			&array,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call get_current_audio_devices_index() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called get_current_audio_devices_index() on ConfigurationManager\n");
	return array;
}

/**
 * Get index
 */
int
dbus_get_audio_device_index(const gchar *name)
{
	g_print("Before get audio device index");
	int index;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_audio_device_index(
			configurationManagerProxy,
			name,
			&index,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call get_audio_device_index() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called get_audio_device_index() on ConfigurationManager\n");
	return index;
}

/**
 * Get audio plugin 
 */
gchar*
dbus_get_current_audio_output_plugin()
{
	g_print("Before get audio plugin");
	gchar* plugin;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_current_audio_output_plugin(
			configurationManagerProxy,
			&plugin,
			&error);
	g_print("After");
	if(error)
	{
		g_printerr("Failed to call get_current_audio_output_plugin() on ConfigurationManager: %s\n", error->message);
		g_error_free(error);
	}
	else
		g_print("DBus called get_current_audio_output_plugin() on ConfigurationManager\n");
	return plugin;
}


gchar*
dbus_get_ringtone_choice()
{
	gchar* tone;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_ringtone_choice(
			configurationManagerProxy,
			&tone,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called get_ringtone_choice() on ConfigurationManager\n");
	return tone;
}

void
dbus_set_ringtone_choice( const gchar* tone )
{
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_ringtone_choice(
			configurationManagerProxy,
			tone,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called set_ringtone_choice() on ConfigurationManager\n");
}

int
dbus_is_ringtone_enabled()
{
	int res;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_is_ringtone_enabled(
			configurationManagerProxy,
			&res,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called is_ringtone_enabled() on ConfigurationManager\n");
	return res;
}

void
dbus_ringtone_enabled()
{
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_ringtone_enabled(
			configurationManagerProxy,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called ringtone_enabled() on ConfigurationManager\n");
}

int
dbus_is_iax2_enabled()
{
	int res;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_is_iax2_enabled(
			configurationManagerProxy,
			&res,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called is_iax2_enabled() on ConfigurationManager\n");
	return res;
}

int
dbus_get_dialpad()
{
	int state;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_dialpad(
			configurationManagerProxy,
			&state,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called get_dialpad on ConfigurationManager\n");
	return state;
}

void
dbus_set_dialpad(  )
{
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_dialpad(
			configurationManagerProxy,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called set_dialpad on ConfigurationManager\n");
}

int
dbus_get_searchbar()
{
	int state;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_searchbar(
			configurationManagerProxy,
			&state,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called get_searchbar on ConfigurationManager\n");
	return state;
}

void
dbus_set_searchbar(  )
{
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_searchbar(
			configurationManagerProxy,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called set_searchbar on ConfigurationManager\n");
}

int
dbus_get_volume_controls()
{
	int state;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_volume_controls(
			configurationManagerProxy,
			&state,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called get_volume_controls on ConfigurationManager\n");
	return state;
}

void
dbus_set_volume_controls(  )
{
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_volume_controls(
			configurationManagerProxy,
			&error);
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called set_volume_controls on ConfigurationManager\n");
}

void
dbus_set_max_calls( const guint calls  )
{
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_max_calls(
			configurationManagerProxy,
			calls,
			&error);
	if(error)
	{
		g_error_free(error);
	}
}

guint
dbus_get_max_calls( void )
{
	GError* error = NULL;
	guint calls;
	org_sflphone_SFLphone_ConfigurationManager_get_max_calls(
			configurationManagerProxy,
			&calls,
			&error);
	if(error)
	{
		g_error_free(error);
	}
	g_print("GET MAX CALLS = %i\n" , calls);
	return calls;
}

void
dbus_start_hidden( void )
{
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_start_hidden(
			configurationManagerProxy,
			&error);
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called start_hidden on ConfigurationManager\n");
}


int
dbus_is_start_hidden( void )
{
	GError* error = NULL;
	int state;
	org_sflphone_SFLphone_ConfigurationManager_is_start_hidden(
			configurationManagerProxy,
			&state,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called start_hidden on ConfigurationManager\n");
	return state;
}

int
dbus_popup_mode( void )
{
	GError* error = NULL;
	int state;
	org_sflphone_SFLphone_ConfigurationManager_popup_mode(
			configurationManagerProxy,
			&state,
			&error);
	g_print("After");
	if(error)
	{
		g_error_free(error);
	}
	else
		g_print("DBus called popup_mode on ConfigurationManager\n");
	return state;
}

void
dbus_switch_popup_mode( void )
{
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_switch_popup_mode(
			configurationManagerProxy,
			&error);
	if(error)
	{
		g_error_free(error);
	}
}

void
dbus_set_notify( void )
{
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_notify(
			configurationManagerProxy,
			&error);
	if(error)
	{
		g_error_free(error);
	}
	else
	  g_print("Called dbus_set_notif_level\n");
}

guint
dbus_get_notify( void )
{
  g_print("Before dbus_get_notif_level()\n");
	guint level;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_notify(
			configurationManagerProxy,
			&level,
			&error);
	if(error)
	{
	  g_print("Error calling dbus_get_notif_level\n");
		g_error_free(error);
	}
	else
	  g_print("Called dbus_get_notif_level\n");
	
	return level;
}

void
dbus_set_mail_notify( void )
{
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_set_mail_notify(
			configurationManagerProxy,
			&error);
	if(error)
	{
		g_error_free(error);
	}
	else
	  g_print("Called dbus_set_mail_notif_level\n");
}

guint
dbus_get_mail_notify( void )
{
  g_print("Before dbus_get_mail_notif_level()\n");
	guint level;
	GError* error = NULL;
	org_sflphone_SFLphone_ConfigurationManager_get_mail_notify(
			configurationManagerProxy,
			&level,
			&error);
	if(error)
	{
	  g_print("Error calling dbus_get_mail_notif_level\n");
		g_error_free(error);
	}
	else
	  g_print("Called dbus_get_mail_notif_level\n");
	
	return level;
}
