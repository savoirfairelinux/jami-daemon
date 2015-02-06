/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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

#include "dbusvideomanager.h"
#include "managerimpl.h"
#include "manager.h"
#include "client/videomanager.h"

static ring::VideoManager* getVideoManager()
{
    return ring::Manager::instance().getVideoManager();
}

DBusVideoManager::DBusVideoManager(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, "/cx/ring/Ring/VideoManager")
{
}

std::vector<std::map<std::string, std::string>> DBusVideoManager::getCodecs(const std::string& accountID)
{
    return getVideoManager()->getCodecs(accountID);
}

void DBusVideoManager::setCodecs(const std::string& accountID, const std::vector<std::map<std::string, std::string> > &details)
{
    getVideoManager()->setCodecs(accountID, details);
}

std::vector<std::string> DBusVideoManager::getDeviceList()
{
    return getVideoManager()->getDeviceList();
}

std::map<std::string, std::map<std::string, std::vector<std::string>>> DBusVideoManager::getCapabilities(const std::string& name)
{
    return getVideoManager()->getCapabilities(name);
}

std::map<std::string, std::string> DBusVideoManager::getSettings(const std::string& name)
{
    return getVideoManager()->getSettings(name);
}

void DBusVideoManager::applySettings(const std::string& name, const std::map<std::string, std::string>& settings)
{
    getVideoManager()->applySettings(name, settings);
}

void DBusVideoManager::setDefaultDevice(const std::string &dev)
{
    getVideoManager()->setDefaultDevice(dev);
}

std::string DBusVideoManager::getDefaultDevice()
{
    return getVideoManager()->getDefaultDevice();
}

std::string DBusVideoManager::getCurrentCodecName(const std::string &callID)
{
    return getVideoManager()->getCurrentCodecName(callID);
}

void DBusVideoManager::startCamera()
{
    getVideoManager()->startCamera();
}

void DBusVideoManager::stopCamera()
{
    getVideoManager()->stopCamera();
}

bool DBusVideoManager::switchInput(const std::string& resource)
{
    return getVideoManager()->switchInput(resource);
}

bool DBusVideoManager::hasCameraStarted()
{
    return getVideoManager()->hasCameraStarted();
}
