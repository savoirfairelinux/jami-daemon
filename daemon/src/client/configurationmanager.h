/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_DBUS

#include "dbus/dbus_cpp.h"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Weffc++"
#include "dbus/configurationmanager-glue.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"
#pragma GCC diagnostic warning "-Weffc++"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

#else
// these includes normally come with DBus C++
#include <vector>
#include <map>
#include <string>
#endif // HAVE_DBUS

class ConfigurationManager
#if HAVE_DBUS
    : public org::sflphone::SFLphone::ConfigurationManager_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor
#endif
{
    public:
#if HAVE_DBUS
        ConfigurationManager(DBus::Connection& connection);
#else
        ConfigurationManager();
#endif
        std::map< std::string, std::string > getAccountDetails(const std::string& accountID);
        void setAccountDetails(const std::string& accountID, const std::map< std::string, std::string >& details);
        std::map<std::string, std::string> getAccountTemplate();
        std::string addAccount(const std::map< std::string, std::string >& details);
        void removeAccount(const std::string& accoundID);
        std::vector< std::string > getAccountList();
        void sendRegister(const std::string& accoundID, const bool& enable);
        void registerAllAccounts(void);

        std::map< std::string, std::string > getTlsSettingsDefault();

        std::vector< int32_t > getAudioCodecList();
        std::vector< std::string > getSupportedTlsMethod();
        std::vector< std::string > getAudioCodecDetails(const int32_t& payload);
        std::vector< int32_t > getActiveAudioCodecList(const std::string& accountID);

        void setActiveAudioCodecList(const std::vector< std::string >& list, const std::string& accountID);

        std::vector< std::string > getAudioPluginList();
        void setAudioPlugin(const std::string& audioPlugin);
        std::vector< std::string > getAudioOutputDeviceList();
        void setAudioOutputDevice(const int32_t& index);
        void setAudioInputDevice(const int32_t& index);
        void setAudioRingtoneDevice(const int32_t& index);
        std::vector< std::string > getAudioInputDeviceList();
        std::vector< std::string > getCurrentAudioDevicesIndex();
        int32_t getAudioInputDeviceIndex(const std::string& name);
        int32_t getAudioOutputDeviceIndex(const std::string& name);
        std::string getCurrentAudioOutputPlugin();
        bool getNoiseSuppressState();
        void setNoiseSuppressState(const bool& state);

        bool isAgcEnabled();
        void setAgcState(const bool& enabled);

        void muteDtmf(const bool& mute);
        bool isDtmfMuted();

        bool isCaptureMuted();
        void muteCapture(const bool& mute);
        bool isPlaybackMuted();
        void mutePlayback(const bool& mute);

        std::map<std::string, std::string> getRingtoneList();

        std::string getAudioManager();
        bool setAudioManager(const std::string& api);
        std::vector<std::string> getSupportedAudioManagers();

        int32_t isIax2Enabled();
        std::string getRecordPath();
        void setRecordPath(const std::string& recPath);
        bool getIsAlwaysRecording();
        void setIsAlwaysRecording(const bool& rec);

        void setHistoryLimit(const int32_t& days);
        int32_t getHistoryLimit();
        void clearHistory();

        void setAccountsOrder(const std::string& order);

        std::map<std::string, std::string> getHookSettings();
        void setHookSettings(const std::map<std::string, std::string>& settings);

        std::vector<std::map<std::string, std::string> > getHistory();

        std::map<std::string, std::string> getTlsSettings();
        void setTlsSettings(const std::map< std::string, std::string >& details);
        std::map< std::string, std::string > getIp2IpDetails();

        std::vector< std::map< std::string, std::string > > getCredentials(const std::string& accountID);
        void setCredentials(const std::string& accountID, const std::vector< std::map< std::string, std::string > >& details);

        std::string getAddrFromInterfaceName(const std::string& interface);

        std::vector<std::string> getAllIpInterface();
        std::vector<std::string> getAllIpInterfaceByName();

        std::map<std::string, std::string> getShortcuts();
        void setShortcuts(const std::map<std::string, std::string> &shortcutsMap);

        void setVolume(const std::string& device, const double& value);
        double getVolume(const std::string& device);

        /*
         * Security
         */
        bool checkForPrivateKey(const std::string& pemPath);
        bool checkCertificateValidity(const std::string& caPath,
                                      const std::string& pemPath);
        bool checkHostnameCertificate(const std::string& host,
                                      const std::string& port);

        /* the following signals must be implemented manually for any
         * platform or configuration that does not supply dbus */
#if !HAVE_DBUS
        void volumeChanged(const std::string& device, const int& value);

        void accountsChanged();

        void historyChanged();

        void stunStatusFailure(const std::string& accoundID);

        void registrationStateChanged(const std::string& accoundID, int const& state);
        void sipRegistrationStateChanged(const std::string&, const std::string&, const int32_t&);
        void errorAlert(const int& alert);

	std::vector< int32_t > getHardwareAudioFormat();
#endif  // !HAVE_DBUS
};

#endif //CONFIGURATIONMANAGER_H
