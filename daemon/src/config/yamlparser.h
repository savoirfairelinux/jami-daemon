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

#ifndef __YAMLPARSER_H__
#define __YAMLPARSER_H__

#include "yamlnode.h"
#include <yaml.h>
#include <stdio.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace Conf
{

#define PARSER_BUFFERSIZE 65536

typedef std::vector<yaml_event_t> YamlEventVector;

class YamlParserException : public std::runtime_error
{
    public:
        YamlParserException (const std::string& str="") :
            std::runtime_error("YamlParserException occured: " + str) {}
};


class YamlParser
{

    public:

        YamlParser (const char *file);

        ~YamlParser();

        void serializeEvents() throw(YamlParserException);

        YamlDocument *composeEvents() throw(YamlParserException);

        void constructNativeData() throw(YamlParserException);

        SequenceNode *getAccountSequence (void) {
            return accountSequence;
        };

        MappingNode *getPreferenceNode (void) {
            return preferenceNode;
        }

        MappingNode *getAddressbookNode (void) {
            return addressbookNode;
        }

        MappingNode *getAudioNode (void) {
            return audioNode;
        }

        MappingNode *getHookNode (void) {
            return hooksNode;
        }

        MappingNode *getVoipPreferenceNode (void) {
            return voiplinkNode;
        }

        MappingNode *getShortcutNode (void) {
            return shortcutNode;
        }

    private:

        /**
         * Copy yaml parser event in event_to according to their type.
         */
        void copyEvent (yaml_event_t *event_to, yaml_event_t *event_from) throw(YamlParserException);

        void processStream (void) throw(YamlParserException);

        void processDocument (void) throw(YamlParserException);

        void processScalar (YamlNode *topNode) throw(YamlParserException);

        void processSequence (YamlNode *topNode) throw(YamlParserException);

        void processMapping (YamlNode *topNode) throw(YamlParserException);

        void mainNativeDataMapping (MappingNode *map);

        /**
         * Configuration file name
         */
        std::string filename;

        /**
         * Configuration file descriptor
         */
        FILE *fd;

        /**
         * The parser structure.
         */
        yaml_parser_t parser;

        /**
         * The event structure array.
         */
        YamlEventVector events;

        /**
         * Number of event actually parsed
         */
        int eventNumber;

        YamlDocument *doc;

        int eventIndex;

        SequenceNode *accountSequence;

        MappingNode *preferenceNode;

        MappingNode *addressbookNode;

        MappingNode *audioNode;

        MappingNode *hooksNode;

        MappingNode *voiplinkNode;

        MappingNode *shortcutNode;
};

}

#endif
