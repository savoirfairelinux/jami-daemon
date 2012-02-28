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

#include "yamlparser.h"

#include "../global.h"
#include "config.h"
#include "yamlnode.h"
#include <cstdio>

namespace Conf {

YamlParser::YamlParser(const char *file) : filename_(file)
    , fd_(fopen(filename_.c_str(), "rb"))
    , parser_()
    , events_()
    , eventNumber_(0)
    , doc_(NULL)
    , eventIndex_(0)
    , accountSequence_(NULL)
    , preferenceNode_(NULL)
    , addressbookNode_(NULL)
    , audioNode_(NULL)
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

YamlParser::~YamlParser()
{
    if (fd_) {
        fclose(fd_);
        yaml_parser_delete(&parser_);
    }

    for (int i = 0; i < eventNumber_; ++i)
        yaml_event_delete(&events_[i]);

    if (doc_) {
        doc_->deleteChildNodes();
        delete doc_;
    }
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


YamlDocument *YamlParser::composeEvents()
{
    if (eventNumber_ == 0)
        throw YamlParserException("No event available");

    if (events_[0].type != YAML_STREAM_START_EVENT)
        throw YamlParserException("Parsing does not start with stream start");

    eventIndex_ = 0;

    processStream();

    return doc_;
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
    doc_ = new YamlDocument();

    if (!doc_)
        throw YamlParserException("Not able to create new document");

    for (; (eventIndex_ < eventNumber_) and (events_[eventIndex_].type != YAML_DOCUMENT_END_EVENT); ++eventIndex_) {
        switch (events_[eventIndex_].type) {
            case YAML_SCALAR_EVENT:
                processScalar(doc_);
                break;
            case YAML_SEQUENCE_START_EVENT:
                processSequence(doc_);
                break;
            case YAML_MAPPING_START_EVENT:
                processMapping(doc_);
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

    switch (topNode->getType()) {
        case DOCUMENT:
            ((YamlDocument *)(topNode))->addNode(sclr);
            break;
        case SEQUENCE:
            ((SequenceNode *)(topNode))->addNode(sclr);
            break;
        case MAPPING:
            ((MappingNode *)(topNode))->addNode(sclr);
        case SCALAR:
        default:
            break;
    }
}


void YamlParser::processSequence(YamlNode *topNode)
{
    if (!topNode)
        throw YamlParserException("No container for sequence");

    SequenceNode *seq = new SequenceNode(topNode);

    switch (topNode->getType()) {
        case DOCUMENT:
            ((YamlDocument *)(topNode))->addNode(seq);
            break;
        case SEQUENCE:
            ((SequenceNode *)(topNode))->addNode(seq);
            break;
        case MAPPING:
            ((MappingNode *)(topNode))->addNode(seq);
        case SCALAR:
        default:
            break;
    }

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

    switch (topNode->getType()) {
        case DOCUMENT:
            ((YamlDocument *)(topNode))->addNode(map);
            break;
        case SEQUENCE:
            ((SequenceNode *)(topNode))->addNode(map);
            break;
        case MAPPING:
            ((MappingNode *)(topNode))->addNode(map);
        case SCALAR:
        default:
            break;
    }

    ++eventIndex_;

    while ((eventIndex_ < eventNumber_) && (events_[eventIndex_].type != YAML_MAPPING_END_EVENT)) {

        if (events_[eventIndex_].type != YAML_SCALAR_EVENT)
            throw YamlParserException("Mapping not followed by a key");

        map->setTmpKey(std::string((const char *)events_[eventIndex_].data.scalar.value));
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
    Sequence *seq = doc_->getSequence();

    for (Sequence::iterator iter = seq->begin(); iter != seq->end(); ++iter) {
        switch ((*iter)->getType()) {
            case SCALAR:
                throw YamlParserException("No scalar allowed at document level, expect a mapping");
                break;
            case SEQUENCE:
                throw YamlParserException("No sequence allowed at document level, expect a mapping");
                break;
            case MAPPING: {
                MappingNode *map = (MappingNode *)(*iter);
                mainNativeDataMapping(map);
                break;
            }
            default:
                throw YamlParserException("Unknown type in configuration file, expect a mapping");
                break;
        }
    }
}

void YamlParser::mainNativeDataMapping(MappingNode *map)
{
    Mapping *mapping = map->getMapping();

    accountSequence_    = (SequenceNode*)(*mapping)["accounts"];
    addressbookNode_    = (MappingNode*)(*mapping)["addressbook"];
    audioNode_          = (MappingNode*)(*mapping)["audio"];
    hooksNode_          = (MappingNode*)(*mapping)["hooks"];
    preferenceNode_     = (MappingNode*)(*mapping)["preferences"];
    voiplinkNode_       = (MappingNode*)(*mapping)["voipPreferences"];
    shortcutNode_       = (MappingNode*)(*mapping)["shortcuts"];
}
}

