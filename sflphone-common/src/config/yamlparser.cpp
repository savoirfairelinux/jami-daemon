/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

namespace Conf {

YamlParser::YamlParser(const char *file) : filename(file)
{
  memset(buffer, 0, PARSER_BUFFERSIZE);

  open();
}

YamlParser::~YamlParser() 
{
  close();
}

void YamlParser::open() 
{

  fd = fopen(filename.c_str(), "rb");

  if(!fd)
    throw YamlParserException("Could not open file descriptor");

  if(!yaml_parser_initialize(&parser))
    throw YamlParserException("Could not open file descriptor");

  yaml_parser_set_input_file(&parser, fd);
}

void YamlParser::close() 
{

  yaml_parser_delete(&parser);

  if(!fd)
    throw YamlParserException("File descriptor not valid");

  if(!fclose(fd))
    throw YamlParserException("Error closing file descriptor");

}

void YamlParser::serializeEvents() 
{
  bool done = false;
  yaml_event_t event;

  while(!done) {

    if(!yaml_parser_parse(&parser, &event))
      throw YamlParserException("Error while parsing");

    done = (event.type == YAML_STREAM_END_EVENT);
    
    if(eventNumber > PARSER_MAXEVENT)
      throw YamlParserException("Reached maximum of event");

    if(!copyEvent(&(events[eventNumber++]), &event))
      throw YamlParserException("Error copying event");

  }
}


int YamlParser::copyEvent(yaml_event_t *event_to, yaml_event_t *event_from) 
{

  switch (event_from->type) {
  case YAML_STREAM_START_EVENT: {
    // _debug("YAML_STREAM_START_EVENT");
    return yaml_stream_start_event_initialize(event_to,
					      event_from->data.stream_start.encoding);
  }

  case YAML_STREAM_END_EVENT: {
    // _debug("YAML_STREAM_END_EVENT");
    return yaml_stream_end_event_initialize(event_to);
  }

  case YAML_DOCUMENT_START_EVENT: {
    // _debug("YAML_DOCUMENT_START_EVENT");
    return yaml_document_start_event_initialize(event_to,
						event_from->data.document_start.version_directive,
						event_from->data.document_start.tag_directives.start,
						event_from->data.document_start.tag_directives.end,
						event_from->data.document_start.implicit);
  }

  case YAML_DOCUMENT_END_EVENT: {
    // _debug("YAML_DOCUMENT_END_EVENT");
    return yaml_document_end_event_initialize(event_to,
					      event_from->data.document_end.implicit);
  }
  case YAML_ALIAS_EVENT:{
    // _debug("YAML_ALIAS_EVENT");
    return yaml_alias_event_initialize(event_to,
				       event_from->data.alias.anchor);
  }
  case YAML_SCALAR_EVENT: {
    // _debug("YAML_SCALAR_EVENT");
    return yaml_scalar_event_initialize(event_to,
					event_from->data.scalar.anchor,
					event_from->data.scalar.tag,
					event_from->data.scalar.value,
					event_from->data.scalar.length,
					event_from->data.scalar.plain_implicit,
					event_from->data.scalar.quoted_implicit,
					event_from->data.scalar.style);
  }
  case YAML_SEQUENCE_START_EVENT: {
    // _debug("YAML_SEQUENCE_START_EVENT");
    return yaml_sequence_start_event_initialize(event_to,
						event_from->data.sequence_start.anchor,
						event_from->data.sequence_start.tag,
						event_from->data.sequence_start.implicit,
						event_from->data.sequence_start.style);
  }
  case YAML_SEQUENCE_END_EVENT: {
    // _debug("YAML_SEQUENCE_END_EVENT");
    return yaml_sequence_end_event_initialize(event_to);
  }
  case YAML_MAPPING_START_EVENT: {
    // _debug("YAML_MAPPING_START_EVENT");
    return yaml_mapping_start_event_initialize(event_to,
					       event_from->data.mapping_start.anchor,
					       event_from->data.mapping_start.tag,
					       event_from->data.mapping_start.implicit,
					       event_from->data.mapping_start.style);
  }
  case YAML_MAPPING_END_EVENT: {
    // _debug("YAML_MAPPING_END_EVENT");
    return yaml_mapping_end_event_initialize(event_to);

  }
  default:
    assert(1);

  }

  return 0;
}


YamlDocument *YamlParser::composeEvents() {

  if(eventNumber == 0)
    throw YamlParserException("No event available");

  doc = NULL;
  topNode = NULL;

  for (int i = 0; i < eventNumber;) {

    switch(events[i].type) {
    case YAML_STREAM_START_EVENT:
      _debug("YAML_STREAM_START_EVENT");
      break;
    case YAML_STREAM_END_EVENT:
      _debug("YAML_STREAM_END_EVENT");
      break;
    case YAML_DOCUMENT_START_EVENT: {
      _debug("YAML_DOCUMENT_START_EVENT");
      // startDocument();
      break;
    }
    case YAML_DOCUMENT_END_EVENT: {
      _debug("YAML_DOCUMENT_END_EVENT");
      // topNode = NULL;
      break;
    }
    case YAML_ALIAS_EVENT:
      _debug("YAML_ALIAS_EVENT");
      break;
    case YAML_SCALAR_EVENT: {      
      _debug("YAML_SCALAR_EVENT: anchor %s, tag %s, value %s", events[i].data.scalar.anchor, events[i].data.scalar.tag, events[i].data.scalar.value);
      char buffer[1000];
      snprintf(buffer, 1000, "%s", events[i].data.scalar.value);
      composeScalarEvent(topNode, buffer);
      break;
    }
    case YAML_SEQUENCE_START_EVENT: {
      _debug("YAML_SEQUENCE_START_EVENT: anchor %s, tag %s", events[i].data.sequence_start.anchor, events[i].data.sequence_start.tag);
      // startSequence();
      break;
    }
    case YAML_SEQUENCE_END_EVENT: {
      _debug("YAML_SEQUENCE_END_EVENT");
      // endSequence();
      break;
    }
    case YAML_MAPPING_START_EVENT: {
      _debug("YAML_MAPPING_START_EVENT: anchor %s, tag %s", events[i].data.mapping_start.anchor, events[i].data.sequence_start.tag);

      Key mapkey;
      YamlNode *mapvalue;

      MappingNode *map = new MappingNode(topNode);

      if(events[i+1].type == YAML_SCALAR_EVENT) {
	char buffer[1000];
	snprintf(buffer, 1000, "%s", events[i+1].data.scalar.value);
	mapkey = Key(buffer);
	i++;
      }
      else
	throw YamlParserException("Mapping event not followed by scalar");

      if(events[i+1].type == YAML_SCALAR_EVENT) {
	char buffer[1000];
	snprintf(buffer, 1000, "%s", events[i+1].data.scalar.value);
	ScalarNode *sclr = new ScalarNode(buffer, topNode);
	mapvalue = (YamlNode *)sclr;
	composeMappingEvent(map, mapkey, mapvalue);
      }
      else if(events[i+1].type == YAML_SEQUENCE_START_EVENT) {
	SequenceNode *seq = new SequenceNode(topNode);
	mapvalue = (YamlNode *)seq;
	composeMappingEvent(map, mapkey, mapvalue);
	topNode = (YamlNode *)seq;
      }
      else
	throw YamlParserException("Not acceptable value for mapping");

      break;
    }
    case YAML_MAPPING_END_EVENT: {
      _debug("YAML_MAPPING_END_EVENT");
      // endMapping();
      break;
    }
    default:
      throw YamlParserException("Unknown Event");
    }

    i++;
  }

  return doc;
}


void YamlParser::startDocument()
{
  doc = new YamlDocument();

  if(!doc)
    throw YamlParserException("Not able to create new document");

  topNode = (YamlNode *)(doc);
}

void YamlParser::endDocument()
{
  if(!doc || !topNode)
    throw YamlParserException("No document to end");

  if(topNode->getType() != DOCUMENT)
    throw YamlParserException("Top node is not a document");

  topNode = NULL;
}

void YamlParser::startSequence()
{
  if(!topNode)
    throw YamlParserException("No container for sequence");

  SequenceNode *seq = new SequenceNode(topNode);

  composeSequence();
  topNode = (YamlNode *)seq;
}

void YamlParser::endSequence()
{
  if(!topNode)
    throw YamlParserException("No sequence to stop");

  if(topNode->getType() != SEQUENCE)
    throw YamlParserException("Top node is not a sequence");


  topNode = topNode->getTopNode();
}

void YamlParser::endMapping() {

  if(!topNode)
    throw YamlParserException("No mapping to stop");

  if(topNode->getType() != MAPPING)
    throw YamlParserException("Top node is not a mapping");

  topNode = topNode->getTopNode();

}

void YamlParser::composeScalarEvent(YamlNode *topNode, char *data) 
{
  if(!topNode)
    throw YamlParserException("No container for scalar");

  ScalarNode *sclr = new ScalarNode(data, topNode);

  switch(topNode->getType()) {
  case DOCUMENT:
    ((YamlDocument *)(topNode))->addNode(sclr);
    break;
  case SEQUENCE:
    ((SequenceNode *)(topNode))->addNode(sclr);
    break;
  case SCALAR:
  case MAPPING:
  default:
    break;
  }
  
}


void YamlParser::composeMappingEvent(MappingNode *map, Key key, YamlNode *val)
{
  YamlNode *topNode = map->getTopNode();

  if(!topNode)
    throw YamlParserException("No container for mapping");

  map->setKeyValue(key, val);

  switch(topNode->getType()) {
  case DOCUMENT:
    ((YamlDocument *)(topNode))->addNode(map);
    break;
  case SEQUENCE:
    ((SequenceNode *)(topNode))->addNode(map);
    break;
  case SCALAR:
  case MAPPING:
  default:
    break;
  }

}

void YamlParser::composeSequenceEvent(YamlNode *topNode, YamlNode *node) 
{

  if(!topNode)
    throw YamlParserException("No container for sequence");

  switch(topNode->getType()) {
  case DOCUMENT:
    ((YamlDocument *)(topNode))->addNode(node);
    break;
  case SEQUENCE:
    ((SequenceNode *)(topNode))->addNode(node);
    break;
  case SCALAR:
  case MAPPING:
  default:
    break;
  }

}

}
