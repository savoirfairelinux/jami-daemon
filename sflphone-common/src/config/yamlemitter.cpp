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

#include "yamlemitter.h"
#include <stdio.h>
#include "../global.h"

namespace Conf {

YamlEmitter::YamlEmitter(const char *file) : filename(file), isFirstAccount(true)
{
  open();
}

YamlEmitter::~YamlEmitter() 
{
  close();
}

void YamlEmitter::open() 
{
  fd = fopen(filename.c_str(), "wb");

  if(!fd)
    throw YamlEmitterException("Could not open file descriptor");

  if(!yaml_emitter_initialize(&emitter))
    throw YamlEmitterException("Could not initialize emitter");

  // Use unicode format
  yaml_emitter_set_unicode(&emitter, 1);

  yaml_emitter_set_output_file(&emitter, fd);

  yaml_document_initialize(&document, NULL, NULL, NULL, 0, 0);

  // Init the main configuration mapping
  if((topLevelMapping = yaml_document_add_mapping (&document, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
    throw YamlEmitterException("Could not create top level mapping");
}

void YamlEmitter::close() 
{
  // yaml_emitter_delete(&emitter);

  if(!fd)
    throw YamlEmitterException("File descriptor not valid");
 
  fclose(fd);
  /*
  if(!fclose(fd))
    throw YamlEmitterException("Error closing file descriptor");
  */

  yaml_document_delete(&document);
}

void YamlEmitter::read() {}

void YamlEmitter::write() 
{

}

void YamlEmitter::serializeData()
{
  yaml_emitter_dump(&emitter, &document);
}


void YamlEmitter::serializeAccount(MappingNode *map)
{

  std::string accountstr("accounts");
 
  int accountid, accountmapping;

  _debug("YamlEmitter: Serialize account");

  if(map->getType() != MAPPING)
    throw YamlEmitterException("Node type is not a mapping while writing account");

  if(isFirstAccount) {
    // accountSequence need to be static outside this scope since reused each time an account is written
    if((accountid = yaml_document_add_scalar(&document, NULL, (yaml_char_t *)accountstr.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
      throw YamlEmitterException("Could not add preference scalar to document");

    if((accountSequence = yaml_document_add_sequence (&document, NULL, YAML_BLOCK_SEQUENCE_STYLE)) == 0)
      throw YamlEmitterException("Could not add sequence to document");

    if(!yaml_document_append_mapping_pair (&document, topLevelMapping, accountid, accountSequence))
      throw YamlEmitterException("Could not add mapping pair to top level mapping");
       
    isFirstAccount = false;
  }

  if((accountmapping = yaml_document_add_mapping (&document, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
    throw YamlEmitterException("Could not add account mapping to document");

  if(!yaml_document_append_sequence_item (&document, accountSequence, accountmapping))
     throw YamlEmitterException("Could not append account mapping to sequence");

  Mapping *internalmap = map->getMapping();
  Mapping::iterator iter = internalmap->begin();

  while(iter != internalmap->end()) {
    addMappingItem(accountmapping, iter->first, iter->second);
    iter++;
  }

}

void YamlEmitter::serializePreference(MappingNode *map)
{
  std::string preferencestr("preferences");

  int preferenceid, preferencemapping;

  _debug("YamlEmitter: Serialize preference");

  if(map->getType() != MAPPING)
    throw YamlEmitterException("Node type is not a mapping while writing preferences");

  if((preferenceid = yaml_document_add_scalar(&document, NULL, (yaml_char_t *)preferencestr.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
    throw YamlEmitterException("Could not add scalar to document");

  if((preferencemapping = yaml_document_add_mapping (&document, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
    throw YamlEmitterException("Could not add mapping to document");

  if(!yaml_document_append_mapping_pair (&document, topLevelMapping, preferenceid, preferencemapping))
    throw YamlEmitterException("Could not add mapping pair to top leve mapping");

  Mapping *internalmap = map->getMapping();
  Mapping::iterator iter = internalmap->begin();

  while(iter != internalmap->end()) {
    addMappingItem(preferencemapping, iter->first, iter->second);
    iter++;
  }

}

void YamlEmitter::serializeVoipPreference(MappingNode *map)
{
  std::string preferencestr("voipPreferences");

  int preferenceid, preferencemapping;

  _debug("YamlEmitter: Serialize voip preference");

  if(map->getType() != MAPPING)
    throw YamlEmitterException("Node type is not a mapping while writing preferences");

  if((preferenceid = yaml_document_add_scalar(&document, NULL, (yaml_char_t *)preferencestr.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
    throw YamlEmitterException("Could not add scalar to document");

  if((preferencemapping = yaml_document_add_mapping (&document, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
    throw YamlEmitterException("Could not add mapping to document");

  if(!yaml_document_append_mapping_pair (&document, topLevelMapping, preferenceid, preferencemapping))
    throw YamlEmitterException("Could not add mapping pair to top leve mapping");

  Mapping *internalmap = map->getMapping();
  Mapping::iterator iter = internalmap->begin();

  while(iter != internalmap->end()) {
    addMappingItem(preferencemapping, iter->first, iter->second);
    iter++;
  }

}

void YamlEmitter::serializeAddressbookPreference(MappingNode *map)
{
  std::string preferencestr("addressbook");

  int preferenceid, preferencemapping;

  _debug("YamlEmitter: Serialize addressbook preferences");

  if(map->getType() != MAPPING)
    throw YamlEmitterException("Node type is not a mapping while writing preferences");

  if((preferenceid = yaml_document_add_scalar(&document, NULL, (yaml_char_t *)preferencestr.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
    throw YamlEmitterException("Could not add scalar to document");

  if((preferencemapping = yaml_document_add_mapping (&document, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
    throw YamlEmitterException("Could not add mapping to document");

  if(!yaml_document_append_mapping_pair (&document, topLevelMapping, preferenceid, preferencemapping))
    throw YamlEmitterException("Could not add mapping pair to top leve mapping");

  Mapping *internalmap = map->getMapping();
  Mapping::iterator iter = internalmap->begin();

  while(iter != internalmap->end()) {
    addMappingItem(preferencemapping, iter->first, iter->second);
    iter++;
  }

}

void YamlEmitter::serializeHooksPreference(MappingNode *map)
{
  std::string preferencestr("hooks");

  int preferenceid, preferencemapping;

  _debug("YamlEmitter: Serialize hooks preferences");

  if(map->getType() != MAPPING)
    throw YamlEmitterException("Node type is not a mapping while writing preferences");

  if((preferenceid = yaml_document_add_scalar(&document, NULL, (yaml_char_t *)preferencestr.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
    throw YamlEmitterException("Could not add scalar to document");

  if((preferencemapping = yaml_document_add_mapping (&document, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
    throw YamlEmitterException("Could not add mapping to document");

  if(!yaml_document_append_mapping_pair (&document, topLevelMapping, preferenceid, preferencemapping))
    throw YamlEmitterException("Could not add mapping pair to top leve mapping");

  Mapping *internalmap = map->getMapping();
  Mapping::iterator iter = internalmap->begin();

  while(iter != internalmap->end()) {
    addMappingItem(preferencemapping, iter->first, iter->second);
    iter++;
  }

}


void YamlEmitter::serializeAudioPreference(MappingNode *map)
{
  std::string preferencestr("audio");

  int preferenceid, preferencemapping;

  _debug("YamlEmitter: Serialize hooks preferences");

  if(map->getType() != MAPPING)
    throw YamlEmitterException("Node type is not a mapping while writing preferences");

  if((preferenceid = yaml_document_add_scalar(&document, NULL, (yaml_char_t *)preferencestr.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
    throw YamlEmitterException("Could not add scalar to document");

  if((preferencemapping = yaml_document_add_mapping (&document, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
    throw YamlEmitterException("Could not add mapping to document");

  if(!yaml_document_append_mapping_pair (&document, topLevelMapping, preferenceid, preferencemapping))
    throw YamlEmitterException("Could not add mapping pair to top leve mapping");

  Mapping *internalmap = map->getMapping();
  Mapping::iterator iter = internalmap->begin();

  while(iter != internalmap->end()) {
    addMappingItem(preferencemapping, iter->first, iter->second);
    iter++;
  }

}


void YamlEmitter::addMappingItem(int mappingid, Key key, YamlNode *node) 
{

  if(node->getType() == SCALAR) {

    int temp1, temp2;

    ScalarNode *sclr = (ScalarNode *)node;

    if((temp1 = yaml_document_add_scalar(&document, NULL, (yaml_char_t *)key.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
      throw YamlEmitterException("Could not add scalar to document");

    if((temp2 = yaml_document_add_scalar(&document, NULL, (yaml_char_t *)sclr->getValue().c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
      throw YamlEmitterException("Could not add scalar to document");

    if(!yaml_document_append_mapping_pair (&document, mappingid, temp1, temp2))
      throw YamlEmitterException("Could not append mapping pair to mapping");

  }
  else if(node->getType() == MAPPING){

    int temp1, temp2;

    if((temp1 = yaml_document_add_scalar(&document, NULL, (yaml_char_t *)key.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
      throw YamlEmitterException("Could not add scalar to document");

    if((temp2 = yaml_document_add_mapping (&document, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
      throw YamlEmitterException("Could not add scalar to document");

    if(!yaml_document_append_mapping_pair (&document, mappingid, temp1, temp2))
      throw YamlEmitterException("Could not add mapping pair to mapping");

    MappingNode *map = (MappingNode *)node;
    Mapping *internalmap = map->getMapping();
    Mapping::iterator iter = internalmap->begin();
    
    while(iter != internalmap->end()) {
      addMappingItem(temp2, iter->first, iter->second);
      iter++;
    }
  }
  else
    throw YamlEmitterException("Unknown node type while adding mapping node");
}


}
