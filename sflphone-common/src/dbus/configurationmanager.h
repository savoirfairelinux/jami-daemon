/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
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

#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include "configurationmanager-glue.h"
#include <dbus-c++/dbus.h>


class ConfigurationManager
: public org::sflphone::SFLphone::ConfigurationManager_adaptor,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
public:

    ConfigurationManager(DBus::Connection& connection);
    static const char* SERVER_PATH;

private:
    std::vector<std::string> shortcutsKeys;

public:

    std::map< std::string, std::string > getAccountDetails( const std::string& accountID );
    void setAccountDetails( const std::string& accountID, const std::map< std::string, std::string >& details );
    std::string addAccount( const std::map< std::string, std::string >& details );
    void removeAccount( const std::string& accoundID );
    void deleteAllCredential (const std::string& accountID);
    std::vector< std::string > getAccountList(  );
    void sendRegister(  const std::string& accoundID , const int32_t& expire );

    std::map< std::string, std::string > getTlsSettingsDefault (void);
    void setIp2IpDetails(const std::map< std::string, std::string >& details);
    std::map< std::string, std::string > getIp2IpDetails(void);
    std::map< std::string, std::string > getCredential (const std::string& accountID, const int32_t& index);
    int32_t getNumberOfCredential (const std::string& accountID);
    void setCredential (const std::string& accountID, const int32_t& index, const std::map< std::string, std::string >& details);
    void setNumberOfCredential (const std::string& accountID, const int32_t& number);

    std::vector< std::string > getCodecList(void);
    std::vector< std::string > getSupportedTlsMethod(void);
    std::vector< std::string > getCodecDetails( const int32_t& payload );
    std::vector< std::string > getActiveCodecList (const std::string& accountID);
    void setActiveCodecList (const std::vector< std::string >& list, const std::string& accountID);

    std::vector< std::string > getAudioPluginList();
    void setInputAudioPlugin(const std::string& audioPlugin);
    void setOutputAudioPlugin(const std::string& audioPlugin);
    std::vector< std::string > getAudioOutputDeviceList();
    void setAudioOutputDevice(const int32_t& index);
    void setAudioInputDevice(const int32_t& index);
    void setAudioRingtoneDevice(const int32_t& index);
    std::vector< std::string > getAudioInputDeviceList();
    std::vector< std::string > getCurrentAudioDevicesIndex();
    int32_t getAudioDeviceIndex(const std::string& name);
    std::string getCurrentAudioOutputPlugin( void );
    std::string getEchoCancelState(void);
    void setEchoCancelState(const std::string& state);
    std::string getNoiseSuppressState(void);
    void setNoiseSuppressState(const std::string& state);


    std::vector< std::string > getToneLocaleList(  );
    std::vector< std::string > getPlaybackDeviceList(  );
    std::vector< std::string > getRecordDeviceList(  );
    std::string getVersion(  );
    std::vector< std::string > getRingtoneList(  );
    int32_t getAudioManager( void );
    void setAudioManager( const int32_t& api );

    bool isMd5CredentialHashing (void);
    void setMd5CredentialHashing (const bool& enabled);
    int32_t isIax2Enabled( void );
    int32_t isRingtoneEnabled( void );
    void ringtoneEnabled( void );
    std::string getRingtoneChoice( void );
    void setRingtoneChoice( const std::string& tone );
    std::string getRecordPath( void );
    void setRecordPath(const std::string& recPath );
    int32_t getDialpad( void );
    void setDialpad (const bool& display);
    int32_t getSearchbar( void );
    
    void setSearchbar( void );
    
    void setHistoryLimit( const int32_t& days);
    int32_t getHistoryLimit (void);
    
    void setHistoryEnabled (void);
	std::string getHistoryEnabled (void);

    int32_t getVolumeControls( void );
    void setVolumeControls (const bool& display);
    int32_t isStartHidden( void );
    void startHidden( void );
    int32_t popupMode( void );
    void switchPopupMode( void );
    int32_t getNotify( void );
    void setNotify( void );
    int32_t getMailNotify( void );
    void setMailNotify( void );

	int32_t getWindowWidth (void);
	int32_t getWindowHeight (void);
	void setWindowWidth (const int32_t& width);
	void setWindowHeight (const int32_t& height);
	int32_t getWindowPositionX (void);
	int32_t getWindowPositionY (void);
	void setWindowPositionX (const int32_t& posX);
	void setWindowPositionY (const int32_t& posY);

	void enableStatusIcon (const std::string&);
	std::string isStatusIconEnabled (void);

    std::map<std::string, int32_t> getAddressbookSettings (void);
    void setAddressbookSettings (const std::map<std::string, int32_t>& settings);
    std::vector< std::string > getAddressbookList ( void );
    void setAddressbookList( const std::vector< std::string >& list );

    void setAccountsOrder (const std::string& order);

    std::map<std::string, std::string> getHookSettings (void);
    void setHookSettings (const std::map<std::string, std::string>& settings);
    
    std::map <std::string, std::string> getHistory (void);
    void setHistory (const std::map <std::string, std::string>& entries);

    std::map<std::string, std::string> getTlsSettings(const std::string& accountID);
    void setTlsSettings(const std::string& accountID, const std::map< std::string, std::string >& details);

    std::string getAddrFromInterfaceName(const std::string& interface);
    
    std::vector<std::string> getAllIpInterface(void);
    std::vector<std::string> getAllIpInterfaceByName(void);

    std::map< std::string, int32_t > getShortcuts ();
    void setShortcuts (const std::map< std::string, int32_t >& shortcutsMap);
};


#endif//CONFIGURATIONMANAGER_H
