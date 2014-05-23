/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
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

#ifndef __SFL_DBUSVIDEOMANAGER_H__
#define __SFL_DBUSVIDEOMANAGER_H__

#include <vector>
#include <map>
#include <string>

#include "dbus_cpp.h"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "dbusvideomanager.adaptor.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

#include <stdexcept>

class DBusVideoManager :
    public org::sflphone::SFLphone::VideoManager_adaptor,
    public DBus::IntrospectableAdaptor,
    public DBus::ObjectAdaptor
{
    public:
        DBusVideoManager(DBus::Connection& connection);

        // Methods
        std::vector<std::map<std::string, std::string>> getCodecs(const std::string& accountID);
        void setCodecs(const std::string& accountID, const std::vector<std::map<std::string, std::string> > &details);
        std::vector<std::string> getDeviceList();
        std::map<std::string, std::map<std::string, std::vector<std::string>>> getCapabilities(const std::string& name);
        std::map<std::string, std::string> getSettings(const std::string& name);
        void applySettings(const std::string& name, const std::map<std::string, std::string>& settings);
        void setDefaultDevice(const std::string &dev);
        std::string getDefaultDevice();
        std::string getCurrentCodecName(const std::string &callID);
        void startCamera();
        void stopCamera();
        bool switchInput(const std::string& resource);
        bool hasCameraStarted();
};

#endif // __SFL_DBUSVIDEOMANAGER_H__
