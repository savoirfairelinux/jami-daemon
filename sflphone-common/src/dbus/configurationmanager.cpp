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

#include <global.h>
#include <configurationmanager.h>
#include <sstream>
#include "../manager.h"

const char* ConfigurationManager::SERVER_PATH = "/org/sflphone/SFLphone/ConfigurationManager";



	ConfigurationManager::ConfigurationManager( DBus::Connection& connection )
: DBus::ObjectAdaptor(connection, SERVER_PATH)
{
}

      
	std::map< std::string, std::string >
ConfigurationManager::getAccountDetails( const std::string& accountID )
{
        _debug("ConfigurationManager::getAccountDetails\n");
	return Manager::instance().getAccountDetails(accountID);
}

	void
ConfigurationManager::setAccountDetails( const std::string& accountID,
		const std::map< std::string, std::string >& details )
{
	_debug("ConfigurationManager::setAccountDetails received\n");
	Manager::instance().setAccountDetails(accountID, details);
}

	void
ConfigurationManager::sendRegister( const std::string& accountID, const int32_t& expire )
{
  _debug("ConfigurationManager::sendRegister received\n");
	Manager::instance().sendRegister(accountID, expire);
}

	std::string
ConfigurationManager::addAccount( const std::map< std::string, std::string >& details )
{
	_debug("ConfigurationManager::addAccount received\n");
	return Manager::instance().addAccount(details);
}


	void
ConfigurationManager::removeAccount( const std::string& accoundID )
{
	_debug("ConfigurationManager::removeAccount received\n");
	return Manager::instance().removeAccount(accoundID);
}

std::vector< std::string >
ConfigurationManager::getAccountList(  )
{
	_debug("ConfigurationManager::getAccountList received\n");
	return Manager::instance().getAccountList();
}


std::vector< std::string >
ConfigurationManager::getToneLocaleList(  )
{
        std::vector< std::string > ret;
	_debug("ConfigurationManager::getToneLocaleList received\n");
        return ret;
}



	std::string
ConfigurationManager::getVersion(  )
{
        std::string ret("");
	_debug("ConfigurationManager::getVersion received\n");
        return ret;

}


	std::vector< std::string >
ConfigurationManager::getRingtoneList(  )
{
	std::vector< std::string >  ret;
	_debug("ConfigurationManager::getRingtoneList received\n");
        return ret;
}



	std::vector< std::string  >
ConfigurationManager::getCodecList(  )
{
        _debug("ConfigurationManager::getRingtoneList received\n");
	return Manager::instance().getCodecList();
}

	std::vector< std::string >
ConfigurationManager::getCodecDetails( const int32_t& payload )
{
        _debug("ConfigurationManager::getRingtoneList received\n");
	return Manager::instance().getCodecDetails( payload );
}

	std::vector< std::string >
ConfigurationManager::getActiveCodecList(  )
{
	_debug("ConfigurationManager::getActiveCodecList received\n");
	return Manager::instance().getActiveCodecList();
}

void
ConfigurationManager::setActiveCodecList( const std::vector< std::string >& list )
{
	_debug("ConfigurationManager::setActiveCodecList received\n");
	 Manager::instance().setActiveCodecList(list);
}

// Audio devices related methods
  std::vector< std::string >
ConfigurationManager::getInputAudioPluginList()
{
	_debug("ConfigurationManager::getInputAudioPluginList received\n");
	return Manager::instance().getInputAudioPluginList();
}

  std::vector< std::string >
ConfigurationManager::getOutputAudioPluginList()
{
	_debug("ConfigurationManager::getOutputAudioPluginList received\n");
	return Manager::instance().getOutputAudioPluginList();
}

  void
ConfigurationManager::setInputAudioPlugin(const std::string& audioPlugin)
{
	_debug("ConfigurationManager::setInputAudioPlugin received\n");
	return Manager::instance().setInputAudioPlugin(audioPlugin);
}

  void
ConfigurationManager::setOutputAudioPlugin(const std::string& audioPlugin)
{
	_debug("ConfigurationManager::setOutputAudioPlugin received\n");
	return Manager::instance().setOutputAudioPlugin(audioPlugin);
}

  std::vector< std::string >
ConfigurationManager::getAudioOutputDeviceList()
{
	_debug("ConfigurationManager::getAudioOutputDeviceList received\n");
	return Manager::instance().getAudioOutputDeviceList();
}
void
ConfigurationManager::setAudioOutputDevice(const int32_t& index)
{
	_debug("ConfigurationManager::setAudioOutputDevice received\n");
	return Manager::instance().setAudioOutputDevice(index);
}
std::vector< std::string >
ConfigurationManager::getAudioInputDeviceList()
{
	_debug("ConfigurationManager::getAudioInputDeviceList received\n");
	return Manager::instance().getAudioInputDeviceList();
}
void
ConfigurationManager::setAudioInputDevice(const int32_t& index)
{
	_debug("ConfigurationManager::setAudioInputDevice received\n");
	return Manager::instance().setAudioInputDevice(index);
}
std::vector< std::string >
ConfigurationManager::getCurrentAudioDevicesIndex()
{
	_debug("ConfigurationManager::getCurrentAudioDeviceIndex received\n");
	return Manager::instance().getCurrentAudioDevicesIndex();
}
 int32_t
ConfigurationManager::getAudioDeviceIndex(const std::string& name)
{
	_debug("ConfigurationManager::getAudioDeviceIndex received\n");
	return Manager::instance().getAudioDeviceIndex(name);
}

std::string
ConfigurationManager::getCurrentAudioOutputPlugin( void )
{
   _debug("ConfigurationManager::getCurrentAudioOutputPlugin received\n");
   return Manager::instance().getCurrentAudioOutputPlugin();
}


	std::vector< std::string >
ConfigurationManager::getPlaybackDeviceList(  )
{
	std::vector< std::string >  ret;
	_debug("ConfigurationManager::getPlaybackDeviceList received\n");
        return ret;
}

	std::vector< std::string >
ConfigurationManager::getRecordDeviceList(  )
{
	std::vector< std::string >  ret;
	_debug("ConfigurationManager::getRecordDeviceList received\n");
        return ret;

}

int32_t
ConfigurationManager::isIax2Enabled( void )
{
  return Manager::instance().isIax2Enabled(  );
}

void
ConfigurationManager::ringtoneEnabled( void )
{
  Manager::instance().ringtoneEnabled(  );
}

int32_t
ConfigurationManager::isRingtoneEnabled( void )
{
  return Manager::instance().isRingtoneEnabled(  );
}

std::string
ConfigurationManager::getRingtoneChoice( void )
{
  return Manager::instance().getRingtoneChoice(  );
}

void
ConfigurationManager::setRingtoneChoice( const std::string& tone )
{
  Manager::instance().setRingtoneChoice( tone );
}

std::string
ConfigurationManager::getRecordPath( void )
{
  return Manager::instance().getRecordPath( );
}

void
ConfigurationManager::setRecordPath( const std::string& recPath)
{
  Manager::instance().setRecordPath( recPath );
}

int32_t
ConfigurationManager::getDialpad( void )
{
  return Manager::instance().getDialpad(  );
}

void
ConfigurationManager::setDialpad( void )
{
  Manager::instance().setDialpad( );
}

int32_t
ConfigurationManager::getSearchbar( void )
{
  return Manager::instance().getSearchbar(  );
}

void
ConfigurationManager::setSearchbar( void )
{
  Manager::instance().setSearchbar( );
}

int32_t
ConfigurationManager::getVolumeControls( void )
{
  return Manager::instance().getVolumeControls(  );
}

void
ConfigurationManager::setVolumeControls( void )
{
  Manager::instance().setVolumeControls( );
}

int32_t
ConfigurationManager::getHistoryLimit( void )
{
  return Manager::instance().getHistoryLimit();
}

void
ConfigurationManager::setHistoryLimit (const int32_t& days)
{
  Manager::instance().setHistoryLimit (days);
}


void ConfigurationManager::setHistoryEnabled (void)
{
    Manager::instance ().setHistoryEnabled ();
}
    
int32_t ConfigurationManager::getHistoryEnabled (void)
{
    return Manager::instance ().getHistoryEnabled ();
}

    void
ConfigurationManager::startHidden( void )
{
  Manager::instance().startHidden(  );
}

int32_t
ConfigurationManager::isStartHidden( void )
{
  return Manager::instance().isStartHidden(  );
}

void
ConfigurationManager::switchPopupMode( void )
{
  Manager::instance().switchPopupMode(  );
}

int32_t
ConfigurationManager::popupMode( void )
{
  return Manager::instance().popupMode(  );
}

void
ConfigurationManager::setNotify( void )
{
  _debug("Manager received setNotify\n");
  Manager::instance().setNotify( );
}

int32_t
ConfigurationManager::getNotify( void )
{
  _debug("Manager received getNotify\n");
  return Manager::instance().getNotify(  );
}

void
ConfigurationManager::setAudioManager( const int32_t& api )
{
  _debug("Manager received setAudioManager\n");
  Manager::instance().setAudioManager( api );
}

int32_t
ConfigurationManager::getAudioManager( void )
{
  _debug("Manager received getAudioManager\n");
  return Manager::instance().getAudioManager(  );
}

void
ConfigurationManager::setMailNotify( void )
{
  _debug("Manager received setMailNotify\n");
  Manager::instance().setMailNotify( );
}

int32_t
ConfigurationManager::getMailNotify( void )
{
  _debug("Manager received getMailNotify\n");
  return Manager::instance().getMailNotify(  );
}

int32_t
ConfigurationManager::getPulseAppVolumeControl( void )
{
  return Manager::instance().getPulseAppVolumeControl();
}

void
ConfigurationManager::setPulseAppVolumeControl( void )
{
  Manager::instance().setPulseAppVolumeControl();
}

int32_t
ConfigurationManager::getSipPort( void )
{
  return Manager::instance().getSipPort();
}

void
ConfigurationManager::setSipPort( const int32_t& portNum )
{
  _debug("Manager received setSipPort: %d\n", portNum);
  Manager::instance().setSipPort(portNum);
}

std::string ConfigurationManager::getStunServer( void )
{
    return Manager::instance().getStunServer();
}

void ConfigurationManager::setStunServer( const std::string& server )
{
    Manager::instance().setStunServer( server );
}

void ConfigurationManager::enableStun (void)
{
    Manager::instance().enableStun();
}

int32_t ConfigurationManager::isStunEnabled (void)
{
    return Manager::instance().isStunEnabled();
}

std::map<std::string, int32_t> ConfigurationManager::getAddressbookSettings (void) {
    return Manager::instance().getAddressbookSettings ();
}

void ConfigurationManager::setAddressbookSettings (const std::map<std::string, int32_t>& settings) {
    Manager::instance().setAddressbookSettings (settings);
}

std::vector< std::string > ConfigurationManager::getAddressbookList ( void ) {
    return Manager::instance().getAddressbookList();
}

void ConfigurationManager::setAddressbookList( const std::vector< std::string >& list ) {
  _debug("Manager received setAddressbookList") ;
    Manager::instance().setAddressbookList(list);
}

std::map<std::string,std::string> ConfigurationManager::getHookSettings (void) {
    return Manager::instance().getHookSettings ();
}

void ConfigurationManager::setHookSettings (const std::map<std::string, std::string>& settings) {
    Manager::instance().setHookSettings (settings);
}

void  ConfigurationManager::setAccountsOrder (const std::string& order) {
    Manager::instance().setAccountsOrder (order);
}

std::map <std::string, std::string> ConfigurationManager::getHistory (void)
{
    return Manager::instance().send_history_to_client ();
}

void ConfigurationManager::setHistory (const std::map <std::string, std::string>& entries)
{
    Manager::instance().receive_history_from_client (entries);
}
