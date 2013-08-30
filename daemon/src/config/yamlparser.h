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

#ifndef __YAMLPARSER_H__
#define __YAMLPARSER_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "yamlnode.h"
#include <yaml.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include "noncopyable.h"

namespace Conf {

#define PARSER_BUFFERSIZE 65536

typedef std::vector<yaml_event_t> YamlEventVector;

class YamlParserException : public std::runtime_error {
    public:
        YamlParserException(const char *err) : std::runtime_error(err) {}
};


class YamlParser {

    public:

        YamlParser(FILE *fd);

        ~YamlParser();

        void serializeEvents();

        void composeEvents();

        void constructNativeData();

        SequenceNode *getAccountSequence();

        MappingNode *getPreferenceNode();

        MappingNode *getAudioNode();

#ifdef SFL_VIDEO
        MappingNode *getVideoNode();
#endif
        MappingNode *getHookNode();

        MappingNode *getVoipPreferenceNode();

        MappingNode *getShortcutNode();

    private:
        NON_COPYABLE(YamlParser);

        /**
         * Copy yaml parser event in event_to according to their type.
         */
        void copyEvent(yaml_event_t *event_to, yaml_event_t *event_from);

        void processStream();

        void processDocument();

        void processScalar(YamlNode *topNode);

        void processSequence(YamlNode *topNode);

        void processMapping(YamlNode *topNode);

        void mainNativeDataMapping(MappingNode *map);

        /**
         * Configuration file descriptor
         */
        FILE *fd_;

        /**
         * The parser structure.
         */
        yaml_parser_t parser_;

        /**
         * The event structure array.
         */
        YamlEventVector events_;

        /**
         * Number of event actually parsed
         */
        int eventNumber_;

        YamlDocument doc_;

        int eventIndex_;

        SequenceNode *accountSequence_;
        MappingNode *preferenceNode_;
        MappingNode *audioNode_;
#ifdef SFL_VIDEO
        MappingNode *videoNode_;
#endif
        MappingNode *hooksNode_;
        MappingNode *voiplinkNode_;
        MappingNode *shortcutNode_;
};
}

#endif
