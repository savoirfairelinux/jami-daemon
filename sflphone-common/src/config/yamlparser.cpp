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

  fclose(fd);
  // if(!fclose(fd))
    // throw YamlParserException("Error closing file descriptor");
 

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
    //_debug("YAML_STREAM_END_EVENT");
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

  // _debug("YamlParser: Compose Events");

  if(eventNumber == 0)
    throw YamlParserException("No event available");

  if(events[0].type != YAML_STREAM_START_EVENT)
    throw YamlParserException("Parsing does not start with stream start");

  eventIndex = 0;

  processStream();

  return doc;
}

void YamlParser::processStream () {

  // _debug("YamlParser: process stream");

  while((eventIndex < eventNumber) && (events[eventIndex].type != YAML_STREAM_END_EVENT)) {

    if(events[eventIndex].type == YAML_DOCUMENT_START_EVENT)
      processDocument();

    eventIndex++;
  }

  if(events[eventIndex].type != YAML_STREAM_END_EVENT)
    throw YamlParserException("Did not found end of stream");
}


void YamlParser::processDocument()
{
  // _debug("YamlParser: process document");

  doc = new YamlDocument();

  if(!doc)
    throw YamlParserException("Not able to create new document");

  while((eventIndex < eventNumber) && (events[eventIndex].type != YAML_DOCUMENT_END_EVENT)) {

    switch(events[eventIndex].type){
    case YAML_SCALAR_EVENT:
      processScalar((YamlNode *)doc);
      break;
    case YAML_SEQUENCE_START_EVENT:
      processSequence((YamlNode *)doc);
      break;
    case YAML_MAPPING_START_EVENT:
      processMapping((YamlNode *)doc);
      break;
    default:
      break;
    }

    eventIndex++;
  }

  if(events[eventIndex].type != YAML_DOCUMENT_END_EVENT)
    throw YamlParserException("Did not found end of document");
  
}


void YamlParser::processScalar(YamlNode *topNode)
{

  // _debug("YamlParser: process scalar");

  if(!topNode)
    throw YamlParserException("No container for scalar");

  char buffer[1000];
  snprintf(buffer, 1000, "%s", events[eventIndex].data.scalar.value);
  // _debug("and the scalar is: %s", buffer);

  ScalarNode *sclr = new ScalarNode(buffer, topNode);

  switch(topNode->getType()) {
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
  _debug("YamlParser: process sequence");

  if(!topNode)
    throw YamlParserException("No container for sequence");

  SequenceNode *seq = new SequenceNode(topNode);

  switch(topNode->getType()) {
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

  eventIndex++;

  while((eventIndex < eventNumber) && (events[eventIndex].type != YAML_SEQUENCE_END_EVENT)) {

    switch(events[eventIndex].type){
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

    eventIndex++;
  }

  if(events[eventIndex].type != YAML_SEQUENCE_END_EVENT)
    throw YamlParserException("Did not found end of sequence");
}


void YamlParser::processMapping(YamlNode *topNode)
{
  // _debug("YamlParser: process mapping");

  if(!topNode)
    throw YamlParserException("No container for mapping");

  MappingNode *map = new MappingNode(topNode);

  switch(topNode->getType()) {
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

  eventIndex++;

  while((eventIndex < eventNumber) && (events[eventIndex].type != YAML_MAPPING_END_EVENT)) {

    if(events[eventIndex].type != YAML_SCALAR_EVENT)
      throw YamlParserException("Mapping not followed by a key");
  
    char buffer[1000];
    snprintf(buffer, 1000, "%s", events[eventIndex].data.scalar.value);
    map->setTmpKey(Key(buffer));
    // _debug("KEY %s", buffer);
    
    eventIndex++;

    switch(events[eventIndex].type){
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

    eventIndex++;
  }

  if(events[eventIndex].type != YAML_MAPPING_END_EVENT)
    throw YamlParserException("Did not found end of mapping");
}

void YamlParser::constructNativeData() {
  
  Sequence *seq;

  seq = doc->getSequence();

  Sequence::iterator iter = seq->begin();

  while(iter != seq->end()) {

    switch((*iter)->getType()){
    case SCALAR:
      // _debug("construct scalar");
      throw YamlParserException("No scalar allowed at document level, expect a mapping");
      break;
    case SEQUENCE:
      // _debug("construct sequence");
      throw YamlParserException("No sequence allowed at document level, expect a mapping");
      break;
    case MAPPING: {
      // _debug("construct mapping");
      MappingNode *map = (MappingNode *)(*iter);
      mainNativeDataMapping(map);
      break;
    }
    default:
      throw YamlParserException("Unknown type in configuration file, expect a mapping");
      break;
    }
    iter++;

  }
  
}


void YamlParser::mainNativeDataMapping(MappingNode *map) {

  
  Mapping::iterator iter = map->getMapping()->begin();

  Key accounts("accounts");
  Key addressbook("addressbook");
  Key audio("audio");
  Key hooks("hooks");
  Key preferences("preferences");
  Key voiplink("voipPreferences");

  while(iter != map->getMapping()->end()) {

    _debug("Iterating: %s", iter->first.c_str());
    if(accounts.compare(iter->first) == 0) {
      _debug("YamlParser: Adding voip account preferences");
      accountSequence = (SequenceNode *)(iter->second);
    }
    else if(addressbook.compare(iter->first) == 0) {
      _debug("YamlParser: Adding voip addressbook preference");
      addressbookSequence = (SequenceNode *)(iter->second);
    }
    else if(audio.compare(iter->first) == 0) {
      _debug("YamlParser: Adding voip audio preference");
      audioSequence = (SequenceNode *)(iter->second);
    }
    else if(hooks.compare(iter->first) == 0) {
      _debug("YamlParser: Adding voip hooks preference");
      hooksSequence = (SequenceNode *)(iter->second);
    }
    else if(preferences.compare(iter->first) == 0) {
      _debug("YamlParser: Adding voip preference preference");
      preferenceSequence = (SequenceNode *)(iter->second);
    }
    else if(voiplink.compare(iter->first) == 0) {
      _debug("YamlParser: Adding voip voip preference");
      voiplinkSequence = (SequenceNode *)(iter->second);
    }
    else
      throw YamlParserException("Unknow map key in configuration");

    iter++;
  }
  // _debug("Done");
}

}
