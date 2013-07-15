/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

%header %{
#include "configurationmanager.h"

typedef struct configurationmanager_callback
{
    void (*on_accounts_changed)(void);
    void (*on_account_state_changed)(const std::string& accoundID, const int32_t& state);
    void (*on_account_state_changed_with_code)(const std::string& accoundID, const std::string& state, const int32_t& code);
} configurationmanager_callback_t;


class ConfigurationCallback {
public:
    virtual ~ConfigurationCallback() {}
    virtual void on_accounts_changed(void) {}
    virtual void on_account_state_changed(const std::string& accoundID, const int32_t& state) {}
    virtual void on_account_state_changed_with_code(const std::string& accoundID, const std::string& state, const int32_t& code) {}
};

static ConfigurationCallback *registeredConfigurationCallbackObject = NULL;

void on_accounts_changed_wrapper (void) {
    registeredConfigurationCallbackObject->on_accounts_changed();
}

void on_account_state_changed_wrapper (const std::string& accoundID, const int32_t& state) {
    registeredConfigurationCallbackObject->on_account_state_changed(accoundID, state);
}

void on_account_state_changed_with_code_wrapper (const std::string& accoundID, const std::string& state, const int32_t& code) {
    registeredConfigurationCallbackObject->on_account_state_changed_with_code(accoundID, state, code);
}

static struct configurationmanager_callback wrapper_configurationcallback_struct = {
    &on_accounts_changed_wrapper,
    &on_account_state_changed_wrapper,
    &on_account_state_changed_with_code_wrapper
};

void setConfigurationCallbackObject(ConfigurationCallback *callback) {
    registeredConfigurationCallbackObject = callback;
}

%}

%feature("director") ConfigurationCallback;

class ConfigurationManager {
public:
    std::map<std::string, std::string> getIp2IpDetails();
    std::map<std::string, std::string> getAccountDetails(const std::string& accountID);
    std::map<std::string, std::string> getTlsSettingsDefault();
    std::map<std::string, std::string> getTlsSettings();
    void setTlsSettings(const std::map<std::string, std::string>& details);
    void setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details);
    void sendRegister(const std::string& accountID, const bool& enable);
    void registerAllAccounts();
    std::map<std::string, std::string> getAccountTemplate();
    std::string addAccount(const std::map<std::string, std::string>& details);
    void removeAccount(const std::string& accoundID);
    std::vector<std::string> getAccountList();
    std::vector<std::string> getSupportedTlsMethod();
    std::vector<std::string> getAudioCodecDetails(const int32_t& payload);
    std::vector<int32_t> getActiveAudioCodecList(const std::string& accountID);
    void setActiveAudioCodecList(const std::vector<std::string>& list, const std::string& accountID);
    std::vector<std::string> getAudioPluginList();
    void setAudioPlugin(const std::string& audioPlugin);
    std::vector<std::string> getAudioOutputDeviceList();
    std::vector<std::string> getAudioInputDeviceList();
    void setAudioOutputDevice(const int32_t& index);
    void setAudioInputDevice(const int32_t& index);
    void setAudioRingtoneDevice(const int32_t& index);
    std::vector<std::string> getCurrentAudioDevicesIndex();
    int32_t getAudioDeviceIndex(const std::string& name);
    std::string getCurrentAudioOutputPlugin();
    std::string getNoiseSuppressState();
    void setNoiseSuppressState(const std::string& state);
    std::string getEchoCancelState();
    std::map<std::string, std::string> getRingtoneList();
    void setEchoCancelState(const std::string& state);
    int32_t isIax2Enabled();

    int32_t getHistoryLimit();
    void clearHistory();
    void setHistoryLimit(const int32_t& days);

    void setAudioManager(const std::string& api);
    std::string getAudioManager();
    std::map<std::string, std::string> getHookSettings();
    void setHookSettings(const std::map<std::string, std::string>& settings);
    void setAccountsOrder(const std::string& order);
    std::vector<std::map<std::string, std::string> > getHistory();
    std::string getAddrFromInterfaceName(const std::string& interface);
    std::vector<std::string> getAllIpInterface();
    std::vector<std::string> getAllIpInterfaceByName();
    std::map<std::string, std::string> getShortcuts();
    void setShortcuts(const std::map<std::string, std::string>& shortcutsMap);
    std::vector<std::map<std::string, std::string> > getCredentials(const std::string& accountID);
    void setCredentials(const std::string& accountID, const std::vector<std::map<std::string, std::string> >& details);
};

class ConfigurationCallback {
public:
    virtual ~ConfigurationCallback();
    virtual void on_accounts_changed(void);
    virtual void on_account_state_changed(const std::string& accoundID, const int32_t& state);
    virtual void on_account_state_changed_with_code(const std::string& accoundID, const std::string& state, const int32_t& code);
};

static ConfigurationCallback *registeredConfigurationCallbackObject = NULL;

void setConfigurationCallbackObject(ConfigurationCallback *callback) {
    registeredConfigurationCallbackObject = callback;
}
