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

#include "yamlemitter.h"
#include <stdio.h>
#include "../global.h"

namespace Conf {

YamlEmitter::YamlEmitter(const char *file) : filename_(file), fd_(0),
    emitter_(), document_(), topLevelMapping_(0), isFirstAccount_(true),
    accountSequence_(0)
{
    open();
}

YamlEmitter::~YamlEmitter()
{
    close();
}

void YamlEmitter::open()
{
    fd_ = fopen(filename_.c_str(), "w");

    if (!fd_)
        throw YamlEmitterException("Could not open file descriptor");

    if (!yaml_emitter_initialize(&emitter_))
        throw YamlEmitterException("Could not initialize emitter");

    // Allows unescaped unicode characters
    yaml_emitter_set_unicode(&emitter_, 1);

    yaml_emitter_set_output_file(&emitter_, fd_);

    if (yaml_document_initialize(&document_, NULL, NULL, NULL, 0, 0) == 0)
        throw YamlEmitterException("Could not initialize yaml document while saving configuration");

    // Init the main configuration mapping
    if ((topLevelMapping_ = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not create top level mapping");
}

void YamlEmitter::close()
{
    yaml_emitter_delete(&emitter_);

    if (!fd_)
        throw YamlEmitterException("File descriptor not valid");

    if (fclose(fd_))
        throw YamlEmitterException("Error closing file descriptor");
}

void YamlEmitter::serializeData()
{
    // Document object is destroyed once its content is emitted
    if (yaml_emitter_dump(&emitter_, &document_) == 0)
        throw YamlEmitterException("Error while emitting configuration yaml document");
}

void YamlEmitter::serializeAccount(MappingNode *map)
{
    int accountmapping;

    if (map->getType() != MAPPING)
        throw YamlEmitterException("Node type is not a mapping while writing account");

    if (isFirstAccount_) {
        int accountid;
        DEBUG("YamlEmitter: Create account sequence");

        // accountSequence_ need to be static outside this scope since reused each time an account is written
        if ((accountid = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) "accounts", -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
            throw YamlEmitterException("Could not add preference scalar to document");

        if ((accountSequence_ = yaml_document_add_sequence(&document_, NULL, YAML_BLOCK_SEQUENCE_STYLE)) == 0)
            throw YamlEmitterException("Could not add sequence to document");

        if (yaml_document_append_mapping_pair(&document_, topLevelMapping_, accountid, accountSequence_) == 0)
            throw YamlEmitterException("Could not add mapping pair to top level mapping");

        isFirstAccount_ = false;
    }

    if ((accountmapping = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not add account mapping to document");

    if (yaml_document_append_sequence_item(&document_, accountSequence_, accountmapping) == 0)
        throw YamlEmitterException("Could not append account mapping to sequence");

    Mapping *internalmap = map->getMapping();
    for (Mapping::iterator iter = internalmap->begin(); iter != internalmap->end(); ++iter)
        addMappingItem(accountmapping, iter->first, iter->second);
}

void YamlEmitter::serializePreference(MappingNode *map)
{
    if (map->getType() != MAPPING)
        throw YamlEmitterException("Node type is not a mapping while writing preferences");

    static const char * const PREFERENCE_STR = "preferences";
    int preferenceid;

    if ((preferenceid = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) PREFERENCE_STR, -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
        throw YamlEmitterException("Could not add scalar to document");

    int preferencemapping;
    if ((preferencemapping = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not add mapping to document");

    if (yaml_document_append_mapping_pair(&document_, topLevelMapping_, preferenceid, preferencemapping) == 0)
        throw YamlEmitterException("Could not add mapping pair to top leve mapping");

    Mapping *internalmap = map->getMapping();
    for (Mapping::iterator iter = internalmap->begin(); iter != internalmap->end(); ++iter)
        addMappingItem(preferencemapping, iter->first, iter->second);
}

void YamlEmitter::serializeVoipPreference(MappingNode *map)
{
    if (map->getType() != MAPPING)
        throw YamlEmitterException("Node type is not a mapping while writing preferences");


    static const char *const PREFERENCE_STR = "voipPreferences";
    int preferenceid;
    if ((preferenceid = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) PREFERENCE_STR, -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
        throw YamlEmitterException("Could not add scalar to document");

    int preferencemapping;
    if ((preferencemapping = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not add mapping to document");

    if (yaml_document_append_mapping_pair(&document_, topLevelMapping_, preferenceid, preferencemapping) == 0)
        throw YamlEmitterException("Could not add mapping pair to top leve mapping");

    Mapping *internalmap = map->getMapping();
    Mapping::iterator iter = internalmap->begin();

    while (iter != internalmap->end()) {
        addMappingItem(preferencemapping, iter->first, iter->second);
        iter++;
    }
}

void YamlEmitter::serializeAddressbookPreference(MappingNode *map)
{
    if (map->getType() != MAPPING)
        throw YamlEmitterException("Node type is not a mapping while writing preferences");

    static const char * const PREFERENCE_STR = "addressbook";
    int preferenceid;
    if ((preferenceid = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) PREFERENCE_STR, -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
        throw YamlEmitterException("Could not add scalar to document");
    int preferencemapping;
    if ((preferencemapping = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not add mapping to document");

    if (yaml_document_append_mapping_pair(&document_, topLevelMapping_, preferenceid, preferencemapping) == 0)
        throw YamlEmitterException("Could not add mapping pair to top leve mapping");

    Mapping *internalmap = map->getMapping();
    for (Mapping::iterator iter = internalmap->begin(); iter != internalmap->end(); ++iter)
        addMappingItem(preferencemapping, iter->first, iter->second);
}

void YamlEmitter::serializeHooksPreference(MappingNode *map)
{
    if (map->getType() != MAPPING)
        throw YamlEmitterException("Node type is not a mapping while writing preferences");

    static const char * const PREFERENCE_STR = "hooks";
    int preferenceid;
    if ((preferenceid = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) PREFERENCE_STR, -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
        throw YamlEmitterException("Could not add scalar to document");

    int preferencemapping;
    if ((preferencemapping = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not add mapping to document");

    if (yaml_document_append_mapping_pair(&document_, topLevelMapping_, preferenceid, preferencemapping) == 0)
        throw YamlEmitterException("Could not add mapping pair to top leve mapping");

    Mapping *internalmap = map->getMapping();
    for (Mapping::iterator iter = internalmap->begin(); iter != internalmap->end(); ++iter)
        addMappingItem(preferencemapping, iter->first, iter->second);
}


void YamlEmitter::serializeAudioPreference(MappingNode *map)
{
    static const char *const PREFERENCE_STR = "audio";

    int preferenceid, preferencemapping;

    if (map->getType() != MAPPING)
        throw YamlEmitterException("Node type is not a mapping while writing preferences");

    if ((preferenceid = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) PREFERENCE_STR, -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
        throw YamlEmitterException("Could not add scalar to document");

    if ((preferencemapping = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not add mapping to document");

    if (yaml_document_append_mapping_pair(&document_, topLevelMapping_, preferenceid, preferencemapping) == 0)
        throw YamlEmitterException("Could not add mapping pair to top leve mapping");

    Mapping *internalmap = map->getMapping();
    for (Mapping::iterator iter = internalmap->begin(); iter != internalmap->end(); ++iter)
        addMappingItem(preferencemapping, iter->first, iter->second);
}


void YamlEmitter::serializeVideoPreference(MappingNode *map)
{
    if (map->getType() != MAPPING)
        throw YamlEmitterException("Node type is not a mapping while writing preferences");


    static const char * const PREFERENCE_STR = "video";
    int preferenceid;
    if ((preferenceid = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) PREFERENCE_STR, -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
        throw YamlEmitterException("Could not add scalar to document");

    int preferencemapping;
    if ((preferencemapping = yaml_document_add_mapping (&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not add mapping to document");

    if (yaml_document_append_mapping_pair (&document_, topLevelMapping_, preferenceid, preferencemapping) == 0)
        throw YamlEmitterException("Could not add mapping pair to top leve mapping");

    Mapping *internalmap = map->getMapping();
    for (Mapping::iterator iter = internalmap->begin(); iter != internalmap->end(); ++iter)
        addMappingItem(preferencemapping, iter->first, iter->second);
}


void YamlEmitter::serializeShortcutPreference(MappingNode *map)
{
    if (map->getType() != MAPPING)
        throw YamlEmitterException("Node type is not a mapping while writing preferences");

    static const char *const PREFERENCE_STR = "shortcuts";
    int preferenceid;
    if ((preferenceid = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) PREFERENCE_STR, -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
        throw YamlEmitterException("Could not add scalar to document");

    int preferencemapping;
    if ((preferencemapping = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not add mapping to document");

    if (yaml_document_append_mapping_pair(&document_, topLevelMapping_, preferenceid, preferencemapping) == 0)
        throw YamlEmitterException("Could not add mapping pair to top leve mapping");

    Mapping *internalmap = map->getMapping();
    for (Mapping::iterator iter = internalmap->begin(); iter != internalmap->end(); ++iter)
        addMappingItem(preferencemapping, iter->first, iter->second);
}


void YamlEmitter::addMappingItem(int mappingid, std::string key, YamlNode *node)
{
    if (node->getType() == SCALAR) {
        ScalarNode *sclr = (ScalarNode *) node;

        int temp1;
        if ((temp1 = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) key.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
            throw YamlEmitterException("Could not add scalar to document");

        int temp2;
        if ((temp2 = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) sclr->getValue().c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
            throw YamlEmitterException("Could not add scalar to document");

        if (yaml_document_append_mapping_pair(&document_, mappingid, temp1, temp2) == 0)
            throw YamlEmitterException("Could not append mapping pair to mapping");

    } else if (node->getType() == MAPPING) {
        MappingNode *map = (MappingNode *) node;

        int temp1;
        if ((temp1 = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) key.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
            throw YamlEmitterException("Could not add scalar to document");

        int temp2;
        if ((temp2 = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
            throw YamlEmitterException("Could not add scalar to document");

        if (yaml_document_append_mapping_pair(&document_, mappingid, temp1, temp2) == 0)
            throw YamlEmitterException("Could not add mapping pair to mapping");

        Mapping *internalmap = map->getMapping();
        for (Mapping::iterator iter = internalmap->begin(); iter != internalmap->end(); ++iter)
            addMappingItem(temp2, iter->first, iter->second);

    } else if (node->getType() == SEQUENCE) {
        SequenceNode *seqnode = static_cast<SequenceNode *>(node);

        int temp1;
        if ((temp1 = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) key.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
            throw YamlEmitterException("Could not add scalar to document");

        int temp2;
        if ((temp2 = yaml_document_add_sequence(&document_, NULL, YAML_BLOCK_SEQUENCE_STYLE)) == 0)
            throw YamlEmitterException("Could not add scalar to document");

        if (yaml_document_append_mapping_pair(&document_, mappingid, temp1, temp2) == 0)
            throw YamlEmitterException("Could not append mapping pair to mapping");

        Sequence *seq = seqnode->getSequence();
        for (Sequence::const_iterator it = seq->begin(); it != seq->end(); ++it) {
            YamlNode *yamlNode = *it;
            int id;
            if ((id = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
                throw YamlEmitterException("Could not add account mapping to document");

            if (yaml_document_append_sequence_item(&document_, temp2, id) == 0)
                throw YamlEmitterException("Could not append account mapping to sequence");

            MappingNode *mapnode = static_cast<MappingNode*>(yamlNode);
            Mapping *map = mapnode->getMapping();
            Mapping::iterator mapit;

            for (mapit = map->begin(); mapit != map->end() ; ++mapit)
                addMappingItem(id, mapit->first, mapit->second);
        }
    } else
        throw YamlEmitterException("Unknown node type while adding mapping node");
}
}
