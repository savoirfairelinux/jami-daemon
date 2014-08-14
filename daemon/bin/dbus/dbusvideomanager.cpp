/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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
#include "sflphone.h"

#include "dbusvideomanager.h"

DBusVideoManager::DBusVideoManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/org/sflphone/SFLphone/VideoManager")
{
}

std::vector<std::map<std::string, std::string>> DBusVideoManager::getCodecs(const std::string& accountID)
{
    return sflph_video_get_codecs(accountID);
}

void DBusVideoManager::setCodecs(const std::string& accountID, const std::vector<std::map<std::string, std::string> > &details)
{
    sflph_video_set_codecs(accountID, details);
}

std::vector<std::string> DBusVideoManager::getDeviceList()
{
    return sflph_video_get_device_list();
}

std::map<std::string, std::map<std::string, std::vector<std::string>>> DBusVideoManager::getCapabilities(const std::string& name)
{
    return sflph_video_get_capabilities(name);
}

std::map<std::string, std::string> DBusVideoManager::getSettings(const std::string& name)
{
    return sflph_video_get_settings(name);
}

void DBusVideoManager::applySettings(const std::string& name, const std::map<std::string, std::string>& settings)
{
    sflph_video_apply_settings(name, settings);
}

void DBusVideoManager::setDefaultDevice(const std::string &dev)
{
    sflph_video_set_default_device(dev);
}

std::string DBusVideoManager::getDefaultDevice()
{
    return sflph_video_get_default_device();
}

std::string DBusVideoManager::getCurrentCodecName(const std::string &callID)
{
    return sflph_video_get_current_codec_name(callID);
}

void DBusVideoManager::startCamera()
{
    sflph_video_start_camera();
}

void DBusVideoManager::stopCamera()
{
    sflph_video_stop_camera();
}

bool DBusVideoManager::switchInput(const std::string& resource)
{
    return sflph_video_switch_input(resource);
}

bool DBusVideoManager::hasCameraStarted()
{
    return sflph_video_is_camera_started();
}
