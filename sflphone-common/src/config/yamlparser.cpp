/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include <stdio.h>

namespace Conf
{

YamlParser::YamlParser (const char *file) : filename (file)
	, fd(NULL)
	, events()
    , eventNumber (0)
    , doc (NULL)
    , eventIndex (0)
    , accountSequence (NULL)
    , preferenceNode (NULL)
    , addressbookNode (NULL)
    , audioNode (NULL)
    , hooksNode (NULL)
    , voiplinkNode (NULL)
    , shortcutNode (NULL)
{
    fd = fopen (filename.c_str(), "rb");

    if (!fd)
        throw YamlParserException ("Could not open file descriptor");

    if (!yaml_parser_initialize (&parser))
        throw YamlParserException ("Could not initialize");

    yaml_parser_set_input_file (&parser, fd);
}

YamlParser::~YamlParser()
{
    if (!fd)
        throw YamlParserException ("File descriptor not valid");

    if (fclose (fd))
        throw YamlParserException ("Error closing file descriptor");

    yaml_parser_delete (&parser);

    for (int i = 0; i < eventNumber; i++)
        yaml_event_delete (&events[i]);

    if (doc) {
        doc->deleteChildNodes();
        delete doc;
        doc = NULL;
    }
}

void YamlParser::serializeEvents() throw(YamlParserException)
{
    bool done = false;
    yaml_event_t event, copiedEvent;

    try {

    	while (!done) {

    		if (!yaml_parser_parse (&parser, &event))
    			throw YamlParserException ("Error while parsing");

    		done = (event.type == YAML_STREAM_END_EVENT);

    		copyEvent (&copiedEvent, &event);

    		events.push_back(copiedEvent);

    		eventNumber++;

    		yaml_event_delete (&event);
    	}
    }
    catch(YamlParserException &e) {
    	throw;
    }

}


void YamlParser::copyEvent (yaml_event_t *event_to, yaml_event_t *event_from) throw(YamlParserException)
{

    switch (event_from->type) {
        case YAML_STREAM_START_EVENT: {
            if(yaml_stream_start_event_initialize (event_to,
                    event_from->data.stream_start.encoding) == 0) {
            	throw YamlParserException("Error stream start event");
            }
            break;
        }

        case YAML_STREAM_END_EVENT: {
            if(yaml_stream_end_event_initialize (event_to) == 0) {
            	throw YamlParserException("Error stream end event");
            }
            break;
        }

        case YAML_DOCUMENT_START_EVENT: {
            if(yaml_document_start_event_initialize (event_to,
                    event_from->data.document_start.version_directive,
                    event_from->data.document_start.tag_directives.start,
                    event_from->data.document_start.tag_directives.end,
                    event_from->data.document_start.implicit) == 0) {
            	throw YamlParserException("Error document start event");
            }
            break;
        }

        case YAML_DOCUMENT_END_EVENT: {
            if(yaml_document_end_event_initialize (event_to,
                    event_from->data.document_end.implicit) == 0) {
            	throw YamlParserException("Error document end event");
            }
            break;
        }
        case YAML_ALIAS_EVENT: {
            if (yaml_alias_event_initialize (event_to,
                     event_from->data.alias.anchor) == 0) {
            	throw YamlParserException("Error alias event initialize");
            }
            break;
        }
        case YAML_SCALAR_EVENT: {
            if(yaml_scalar_event_initialize (event_to,
                    event_from->data.scalar.anchor,
                    event_from->data.scalar.tag,
                    event_from->data.scalar.value,
                    event_from->data.scalar.length,
                    event_from->data.scalar.plain_implicit,
                    event_from->data.scalar.quoted_implicit,
                    event_from->data.scalar.style) == 0) {
            	throw YamlParserException("Error scalar event initialize");
            }
            break;
        }
        case YAML_SEQUENCE_START_EVENT: {
            if(yaml_sequence_start_event_initialize (event_to,
                    event_from->data.sequence_start.anchor,
                    event_from->data.sequence_start.tag,
                    event_from->data.sequence_start.implicit,
                    event_from->data.sequence_start.style) == 0) {
            	throw YamlParserException("Error sequence start event");
            }
            break;
        }
        case YAML_SEQUENCE_END_EVENT: {
            if(yaml_sequence_end_event_initialize (event_to) == 0) {
            	throw YamlParserException("Error sequence end event");
            }
            break;
        }
        case YAML_MAPPING_START_EVENT: {
            if(yaml_mapping_start_event_initialize (event_to,
                    event_from->data.mapping_start.anchor,
                    event_from->data.mapping_start.tag,
                    event_from->data.mapping_start.implicit,
                    event_from->data.mapping_start.style) == 0) {
            	throw YamlParserException("Error mapping start event");
            }
            break;
        }
        case YAML_MAPPING_END_EVENT: {
            if(yaml_mapping_end_event_initialize (event_to) == 0) {
            	throw YamlParserException("Error mapping end event");
            }
            break;
        }
        default:
        	break;
    }
}


YamlDocument *YamlParser::composeEvents() throw(YamlParserException)
{
	try {
		if (eventNumber == 0)
			throw YamlParserException ("No event available");

		if (events[0].type != YAML_STREAM_START_EVENT)
			throw YamlParserException ("Parsing does not start with stream start");

		eventIndex = 0;

		processStream();
	}
	catch(YamlParserException &e) {
		throw;
	}


    return doc;
}

void YamlParser::processStream () throw(YamlParserException)
{
	try {
		while ( (eventIndex < eventNumber) && (events[eventIndex].type != YAML_STREAM_END_EVENT)) {

			if (events[eventIndex].type == YAML_DOCUMENT_START_EVENT)
				processDocument();

			eventIndex++;
		}

		if (events[eventIndex].type != YAML_STREAM_END_EVENT)
			throw YamlParserException ("Did not found end of stream");
	}
	catch(YamlParserException &e) {
		throw;
	}
}

void YamlParser::processDocument() throw(YamlParserException)
{
	try {

		doc = new YamlDocument();

		if (!doc)
			throw YamlParserException ("Not able to create new document");

		while ( (eventIndex < eventNumber) && (events[eventIndex].type != YAML_DOCUMENT_END_EVENT)) {

			switch (events[eventIndex].type) {
            case YAML_SCALAR_EVENT:
            	processScalar ( (YamlNode *) doc);
            	break;
            case YAML_SEQUENCE_START_EVENT:
            	processSequence ( (YamlNode *) doc);
            	break;
            case YAML_MAPPING_START_EVENT:
                processMapping ( (YamlNode *) doc);
                break;
            default:
                break;
			}

			eventIndex++;
		}

		if (events[eventIndex].type != YAML_DOCUMENT_END_EVENT)
			throw YamlParserException ("Did not found end of document");

	}
	catch(YamlParserException &e) {
		throw;
	}
}


void YamlParser::processScalar (YamlNode *topNode) throw(YamlParserException)
{
	try {

		if (!topNode)
			throw YamlParserException ("No container for scalar");

		ScalarNode *sclr = new ScalarNode (std::string((const char*)events[eventIndex].data.scalar.value), topNode);

		switch (topNode->getType()) {
        case DOCUMENT:
            ( (YamlDocument *) (topNode))->addNode (sclr);
            break;
        case SEQUENCE:
            ( (SequenceNode *) (topNode))->addNode (sclr);
            break;
        case MAPPING:
            ( (MappingNode *) (topNode))->addNode (sclr);
        case SCALAR:
        default:
            break;
		}
	}
	catch(YamlParserException &e) {
		throw;
	}
}


void YamlParser::processSequence (YamlNode *topNode) throw(YamlParserException)
{

	try {

		if (!topNode)
			throw YamlParserException ("No container for sequence");

		SequenceNode *seq = new SequenceNode (topNode);

		switch (topNode->getType()) {
        case DOCUMENT:
            ( (YamlDocument *) (topNode))->addNode (seq);
            break;
        case SEQUENCE:
            ( (SequenceNode *) (topNode))->addNode (seq);
            break;
        case MAPPING:
            ( (MappingNode *) (topNode))->addNode (seq);
        case SCALAR:
        default:
            break;
		}

		eventIndex++;

		while ( (eventIndex < eventNumber) && (events[eventIndex].type != YAML_SEQUENCE_END_EVENT)) {

			switch (events[eventIndex].type) {
            case YAML_SCALAR_EVENT:
                processScalar (seq);
                break;
            case YAML_SEQUENCE_START_EVENT:
                processSequence (seq);
                break;
            case YAML_MAPPING_START_EVENT:
                processMapping (seq);
                break;
            default:
                break;
			}

			eventIndex++;
		}

		if (events[eventIndex].type != YAML_SEQUENCE_END_EVENT)
			throw YamlParserException ("Did not found end of sequence");

	}
	catch(YamlParserException &e) {
		throw;
	}

}

void YamlParser::processMapping (YamlNode *topNode) throw(YamlParserException)
{
	try {

		if (!topNode)
			throw YamlParserException ("No container for mapping");

		MappingNode *map = new MappingNode (topNode);

		switch (topNode->getType()) {
        case DOCUMENT:
            ( (YamlDocument *) (topNode))->addNode (map);
            break;
        case SEQUENCE:
            ( (SequenceNode *) (topNode))->addNode (map);
            break;
        case MAPPING:
            ( (MappingNode *) (topNode))->addNode (map);
        case SCALAR:
        default:
            break;
		}

		eventIndex++;

		while ( (eventIndex < eventNumber) && (events[eventIndex].type != YAML_MAPPING_END_EVENT)) {

			if (events[eventIndex].type != YAML_SCALAR_EVENT)
				throw YamlParserException ("Mapping not followed by a key");

			map->setTmpKey (std::string ((const char *)events[eventIndex].data.scalar.value));

			eventIndex++;

			switch (events[eventIndex].type) {
            case YAML_SCALAR_EVENT:
                processScalar (map);
                break;
            case YAML_SEQUENCE_START_EVENT:
                processSequence (map);
                break;
            case YAML_MAPPING_START_EVENT:
                processMapping (map);
                break;
            default:
                break;
			}

			eventIndex++;
		}

		if (events[eventIndex].type != YAML_MAPPING_END_EVENT)
			throw YamlParserException ("Did not found end of mapping");

	}
	catch(YamlParserException &e) {
		throw;
	}
}

void YamlParser::constructNativeData() throw(YamlParserException)
{

	try {
		Sequence *seq;

		seq = doc->getSequence();

		Sequence::iterator iter = seq->begin();

		while (iter != seq->end()) {

			switch ( (*iter)->getType()) {
            case SCALAR:
                // _debug("construct scalar");
                throw YamlParserException ("No scalar allowed at document level, expect a mapping");
                break;
            case SEQUENCE:
                // _debug("construct sequence");
                throw YamlParserException ("No sequence allowed at document level, expect a mapping");
                break;
            case MAPPING: {
                // _debug("construct mapping");
                MappingNode *map = (MappingNode *) (*iter);
                mainNativeDataMapping (map);
                break;
            }
            default:
                throw YamlParserException ("Unknown type in configuration file, expect a mapping");
                break;
			}

			iter++;

		}
	}
	catch(YamlParserException &e) {
		throw;
	}

}


void YamlParser::mainNativeDataMapping (MappingNode *map)
{
	Mapping *mapping = map->getMapping();

	accountSequence	= (SequenceNode*)(*mapping)["accounts"];
	addressbookNode = (MappingNode*)(*mapping)["addressbook"];
	audioNode       = (MappingNode*)(*mapping)["audio"];
	hooksNode       = (MappingNode*)(*mapping)["hooks"];
	preferenceNode  = (MappingNode*)(*mapping)["preferences"];
	voiplinkNode    = (MappingNode*)(*mapping)["voipPreferences"];
	shortcutNode    = (MappingNode*)(*mapping)["shortcuts"];
}

}
