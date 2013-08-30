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

#include <cstdio>
#include <assert.h>

#include "yamlparser.h"

#include "../global.h"
#include "sfl_config.h"
#include "yamlnode.h"
#include "logger.h"

namespace Conf {

YamlParser::YamlParser(FILE *fd) : fd_(fd)
    , parser_()
    , events_()
    , eventNumber_(0)
    , doc_(NULL)
    , eventIndex_(0)
    , accountSequence_(NULL)
    , preferenceNode_(NULL)
    , audioNode_(NULL)
#ifdef SFL_VIDEO
    , videoNode_(NULL)
#endif
    , hooksNode_(NULL)
    , voiplinkNode_(NULL)
    , shortcutNode_(NULL)
{
    if (!fd_)
        throw YamlParserException("Could not open file descriptor");

    if (!yaml_parser_initialize(&parser_))
        throw YamlParserException("Could not initialize");

    yaml_parser_set_input_file(&parser_, fd_);
}

#define CHECK_AND_RETURN(x) \
    if (!x) { \
        ERROR("%s", __PRETTY_FUNCTION__); \
        throw YamlParserException("Invalid node"); \
    } \
    return x

SequenceNode *
YamlParser::getAccountSequence()
{
    CHECK_AND_RETURN(accountSequence_);
}

MappingNode *
YamlParser::getPreferenceNode()
{
    CHECK_AND_RETURN(preferenceNode_);
}

MappingNode *
YamlParser::getAudioNode()
{
    CHECK_AND_RETURN(audioNode_);
}

#ifdef SFL_VIDEO
MappingNode *
YamlParser::getVideoNode()
{
    CHECK_AND_RETURN(videoNode_);
}
#endif

MappingNode *
YamlParser::getHookNode()
{
    CHECK_AND_RETURN(hooksNode_);
}

MappingNode *
YamlParser::getVoipPreferenceNode()
{
    CHECK_AND_RETURN(voiplinkNode_);
}

MappingNode *
YamlParser::getShortcutNode()
{
    CHECK_AND_RETURN(shortcutNode_);
}

#undef CHECK_AND_RETURN

YamlParser::~YamlParser()
{
    if (fd_)
        yaml_parser_delete(&parser_);

    for (int i = 0; i < eventNumber_; ++i)
        yaml_event_delete(&events_[i]);

    doc_.deleteChildNodes();
}

void YamlParser::serializeEvents()
{
    bool done = false;
    yaml_event_t event, copiedEvent;

    while (not done) {
        if (!yaml_parser_parse(&parser_, &event))
            throw YamlParserException("Error while parsing");

        done = (event.type == YAML_STREAM_END_EVENT);

        copyEvent(&copiedEvent, &event);

        events_.push_back(copiedEvent);

        ++eventNumber_;

        yaml_event_delete(&event);
    }
}


void YamlParser::copyEvent(yaml_event_t *event_to, yaml_event_t *event_from)
{
    switch (event_from->type) {
        case YAML_STREAM_START_EVENT: {
            if (yaml_stream_start_event_initialize(event_to,
                                                   event_from->data.stream_start.encoding) == 0)
                throw YamlParserException("Error stream start event");

            break;
        }

        case YAML_STREAM_END_EVENT: {
            if (yaml_stream_end_event_initialize(event_to) == 0)
                throw YamlParserException("Error stream end event");

            break;
        }

        case YAML_DOCUMENT_START_EVENT: {
            if (yaml_document_start_event_initialize(event_to,
                    event_from->data.document_start.version_directive,
                    event_from->data.document_start.tag_directives.start,
                    event_from->data.document_start.tag_directives.end,
                    event_from->data.document_start.implicit) == 0)
                throw YamlParserException("Error document start event");

            break;
        }

        case YAML_DOCUMENT_END_EVENT: {
            if (yaml_document_end_event_initialize(event_to,
                                                   event_from->data.document_end.implicit) == 0)
                throw YamlParserException("Error document end event");

            break;
        }
        case YAML_ALIAS_EVENT: {
            if (yaml_alias_event_initialize(event_to,
                                            event_from->data.alias.anchor) == 0)
                throw YamlParserException("Error alias event initialize");

            break;
        }
        case YAML_SCALAR_EVENT: {
            if (yaml_scalar_event_initialize(event_to,
                                             event_from->data.scalar.anchor,
                                             event_from->data.scalar.tag,
                                             event_from->data.scalar.value,
                                             event_from->data.scalar.length,
                                             event_from->data.scalar.plain_implicit,
                                             event_from->data.scalar.quoted_implicit,
                                             event_from->data.scalar.style) == 0)
                throw YamlParserException("Error scalar event initialize");

            break;
        }
        case YAML_SEQUENCE_START_EVENT: {
            if (yaml_sequence_start_event_initialize(event_to,
                    event_from->data.sequence_start.anchor,
                    event_from->data.sequence_start.tag,
                    event_from->data.sequence_start.implicit,
                    event_from->data.sequence_start.style) == 0)
                throw YamlParserException("Error sequence start event");

            break;
        }
        case YAML_SEQUENCE_END_EVENT: {
            if (yaml_sequence_end_event_initialize(event_to) == 0)
                throw YamlParserException("Error sequence end event");

            break;
        }
        case YAML_MAPPING_START_EVENT: {
            if (yaml_mapping_start_event_initialize(event_to,
                                                    event_from->data.mapping_start.anchor,
                                                    event_from->data.mapping_start.tag,
                                                    event_from->data.mapping_start.implicit,
                                                    event_from->data.mapping_start.style) == 0)
                throw YamlParserException("Error mapping start event");
            break;
        }
        case YAML_MAPPING_END_EVENT: {
            if (yaml_mapping_end_event_initialize(event_to) == 0)
                throw YamlParserException("Error mapping end event");

            break;
        }
        default:
            break;
    }
}


void YamlParser::composeEvents()
{
    if (eventNumber_ == 0)
        throw YamlParserException("No event available");

    if (events_[0].type != YAML_STREAM_START_EVENT)
        throw YamlParserException("Parsing does not start with stream start");

    eventIndex_ = 0;

    processStream();
}

void YamlParser::processStream()
{
    for (; (eventIndex_ < eventNumber_) and (events_[eventIndex_].type != YAML_STREAM_END_EVENT); ++eventIndex_)
        if (events_[eventIndex_].type == YAML_DOCUMENT_START_EVENT)
            processDocument();

    if (events_[eventIndex_].type != YAML_STREAM_END_EVENT)
        throw YamlParserException("Did not found end of stream");
}

void YamlParser::processDocument()
{
    assert(eventNumber_ > 0);

    for (; (eventIndex_ < eventNumber_) and (events_[eventIndex_].type != YAML_DOCUMENT_END_EVENT); ++eventIndex_) {
        switch (events_[eventIndex_].type) {
            case YAML_SCALAR_EVENT:
                processScalar(&doc_);
                break;
            case YAML_SEQUENCE_START_EVENT:
                processSequence(&doc_);
                break;
            case YAML_MAPPING_START_EVENT:
                processMapping(&doc_);
                break;
            default:
                break;
        }
    }

    if (events_[eventIndex_].type != YAML_DOCUMENT_END_EVENT)
        throw YamlParserException("Did not found end of document");
}


void YamlParser::processScalar(YamlNode *topNode)
{
    if (!topNode)
        throw YamlParserException("No container for scalar");

    ScalarNode *sclr = new ScalarNode(std::string((const char*) events_[eventIndex_].data.scalar.value), topNode);

    topNode->addNode(sclr);
}


void YamlParser::processSequence(YamlNode *topNode)
{
    if (!topNode)
        throw YamlParserException("No container for sequence");

    SequenceNode *seq = new SequenceNode(topNode);

    topNode->addNode(seq);

    ++eventIndex_;

    for (; (eventIndex_ < eventNumber_) and (events_[eventIndex_].type != YAML_SEQUENCE_END_EVENT); ++eventIndex_) {
        switch (events_[eventIndex_].type) {
            case YAML_SCALAR_EVENT:
                processScalar(seq);
                break;
            case YAML_SEQUENCE_START_EVENT:
                processSequence(seq);
                break;
            case YAML_MAPPING_START_EVENT:
                processMapping(seq);
                break;
            default:
                break;
        }
    }

    if (events_[eventIndex_].type != YAML_SEQUENCE_END_EVENT)
        throw YamlParserException("Did not found end of sequence");

}

void YamlParser::processMapping(YamlNode *topNode)
{
    if (!topNode)
        throw YamlParserException("No container for mapping");

    MappingNode *map = new MappingNode(topNode);

    topNode->addNode(map);

    ++eventIndex_;

    while ((eventIndex_ < eventNumber_) && (events_[eventIndex_].type != YAML_MAPPING_END_EVENT)) {

        if (events_[eventIndex_].type != YAML_SCALAR_EVENT)
            throw YamlParserException("Mapping not followed by a key");

        map->setTmpKey(std::string((const char *)events_[eventIndex_].data.scalar.value));
        std::string tmpstring((const char *)events_[eventIndex_].data.scalar.value);
        ++eventIndex_;

        switch (events_[eventIndex_].type) {
            case YAML_SCALAR_EVENT:
                processScalar(map);
                break;
            case YAML_SEQUENCE_START_EVENT:
                processSequence(map);
                break;
            case YAML_MAPPING_START_EVENT:
                processMapping(map);
                break;
            default:
                break;
        }

        ++eventIndex_;
    }

    if (events_[eventIndex_].type != YAML_MAPPING_END_EVENT)
        throw YamlParserException("Did not found end of mapping");
}

void YamlParser::constructNativeData()
{
    Sequence *seq = doc_.getSequence();

    for (const auto &item : *seq) {
        YamlNode *yamlNode = static_cast<YamlNode *>(item);
        if (yamlNode == NULL) {
            ERROR("Could not retrieve yaml node from document sequence");
            continue;
        }

        NodeType nodeType = yamlNode->getType();
        switch (nodeType) {
            case MAPPING: {
                MappingNode *map = static_cast<MappingNode *>(yamlNode);
                mainNativeDataMapping(map);
                break;
            }
            case SCALAR:
            case SEQUENCE:
            default:
                throw YamlParserException("Unknown type in configuration file, expect a mapping");
                break;
        }
    }
}

void YamlParser::mainNativeDataMapping(MappingNode *map)
{
    std::map<std::string, YamlNode*> &mapping = map->getMapping();

    accountSequence_    = static_cast<SequenceNode*>(mapping["accounts"]);
    audioNode_          = static_cast<MappingNode *>(mapping["audio"]);
#ifdef SFL_VIDEO
    videoNode_          = static_cast<MappingNode *>(mapping["video"]);
#endif
    hooksNode_          = static_cast<MappingNode *>(mapping["hooks"]);
    preferenceNode_     = static_cast<MappingNode *>(mapping["preferences"]);
    voiplinkNode_       = static_cast<MappingNode *>(mapping["voipPreferences"]);
    shortcutNode_       = static_cast<MappingNode *>(mapping["shortcuts"]);
}
}

