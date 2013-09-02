/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef YAMLEMITTER_H_
#define YAMLEMITTER_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <yaml.h>
#include <stdexcept>
#include <string>
#include <map>

#include "yamlnode.h"
#include "noncopyable.h"

namespace Conf {

#define EMITTER_BUFFERSIZE 65536
#define EMITTER_MAXEVENT 1024


class YamlEmitterException : public std::runtime_error {
    public:
        YamlEmitterException(const char *err) : std::runtime_error(err) {}
};

class YamlEmitter {

    public:

        YamlEmitter(const char *file);
        ~YamlEmitter();

        void open();

        void close();

        void serializeAccount(MappingNode *map);

        void serializePreference(MappingNode *map, const char *preference_str);

        void writeAudio();

        void writeHooks();

        void writeVoiplink();

        void serializeData();

    private:

        NON_COPYABLE(YamlEmitter);
        void addMappingItems(int mappingid, YamlNodeMap &iMap);
        void addMappingItem(int mappingid, const std::string &key, YamlNode *node);

        std::string filename_;

        FILE *fd_;

        /**
         * The parser structure.
         */
        yaml_emitter_t emitter_;

        /**
         * The event structure array.
         */
        yaml_event_t events_[EMITTER_MAXEVENT];

        unsigned char buffer_[EMITTER_BUFFERSIZE];

        /**
         * Main document for this serialization
         */
        yaml_document_t document_;

        /**
         * Reference id to the top levell mapping when creating
         */
        int topLevelMapping_;

        /**
         * We need to add the account sequence if this is the first account to be
         */
        bool isFirstAccount_;

        /**
         * Reference to the account sequence
         */
        int accountSequence_;

        friend class ConfigurationTest;
};
}

#endif  // YAMLEMITTER_H__
