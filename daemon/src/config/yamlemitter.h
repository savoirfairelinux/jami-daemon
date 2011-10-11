/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#ifndef __YAMLEMITTER_H__
#define __YAMLEMITTER_H__

#include <yaml.h>
#include <stdexcept>
#include <string>
#include "yamlnode.h"

namespace Conf {

#define EMITTER_BUFFERSIZE 65536
#define EMITTER_MAXEVENT 1024

class YamlEmitterException : public std::runtime_error {
    public:
        YamlEmitterException(const std::string& str="") :
            std::runtime_error("YamlEmitterException occured: " + str) {}
};

class YamlEmitter {

    public:

        YamlEmitter(const char *file);

        ~YamlEmitter();

        void open() throw(YamlEmitterException);

        void close() throw(YamlEmitterException);

        void serializeAccount(MappingNode *map) throw(YamlEmitterException);

        void serializePreference(MappingNode *map) throw(YamlEmitterException);

        void serializeVoipPreference(MappingNode *map) throw(YamlEmitterException);

        void serializeAddressbookPreference(MappingNode *map) throw(YamlEmitterException);

        void serializeHooksPreference(MappingNode *map) throw(YamlEmitterException);

        void serializeAudioPreference(MappingNode *map) throw(YamlEmitterException);

        void serializeShortcutPreference(MappingNode *map) throw(YamlEmitterException);

        void writeAudio();

        void writeHooks();

        void writeVoiplink();

        void serializeData() throw(YamlEmitterException);

    private:

        void addMappingItem(int mappingid, std::string key, YamlNode *node);

        std::string filename;

        FILE *fd;

        /**
         * The parser structure.
         */
        yaml_emitter_t emitter;

        /**
         * The event structure array.
         */
        yaml_event_t events[EMITTER_MAXEVENT];

        /**
         *
         */
        unsigned char buffer[EMITTER_BUFFERSIZE];


        /**
         * Main document for this serialization
         */
        yaml_document_t document;

        /**
         * Reference id to the top levell mapping when creating
         */
        int topLevelMapping;

        /**
         * We need to add the account sequence if this is the first account to be
         */
        bool isFirstAccount;

        /**
         * Reference to the account sequence
         */
        int accountSequence;

        friend class ConfigurationTest;

};
}

#endif
