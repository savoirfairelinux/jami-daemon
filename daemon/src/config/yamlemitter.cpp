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

#include "yamlemitter.h"
#include "yamlnode.h"
#include <cstdio>
#include "logger.h"

namespace Conf {

YamlEmitter::YamlEmitter(const char *file) : filename_(file), fd_(0),
    emitter_(), events_(), buffer_(), document_(), topLevelMapping_(0),
    isFirstAccount_(true), accountSequence_(0)
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

    // Refererence:
    // http://www.parashift.com/c++-faq-lite/exceptions.html#faq-17.9
    if (!fd_) {
        ERROR("File descriptor not valid");
        return;
    }

    if (fclose(fd_))
        ERROR("Error closing file descriptor");
}

void YamlEmitter::serializeData()
{
    // Document object is destroyed once its content is emitted
    if (yaml_emitter_dump(&emitter_, &document_) == 0)
        throw YamlEmitterException("Error while emitting configuration yaml document");
}

void YamlEmitter::serializeAccount(MappingNode *map)
{
    if (map->getType() != MAPPING)
        throw YamlEmitterException("Node type is not a mapping while writing account");

    if (isFirstAccount_) {
        int accountid;

        // accountSequence_ need to be static outside this scope since reused each time an account is written
        if ((accountid = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) "accounts", -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
            throw YamlEmitterException("Could not add preference scalar to document");

        if ((accountSequence_ = yaml_document_add_sequence(&document_, NULL, YAML_BLOCK_SEQUENCE_STYLE)) == 0)
            throw YamlEmitterException("Could not add sequence to document");

        if (yaml_document_append_mapping_pair(&document_, topLevelMapping_, accountid, accountSequence_) == 0)
            throw YamlEmitterException("Could not add mapping pair to top level mapping");

        isFirstAccount_ = false;
    }

    int accountMapping;
    if ((accountMapping = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not add account mapping to document");

    if (yaml_document_append_sequence_item(&document_, accountSequence_, accountMapping) == 0)
        throw YamlEmitterException("Could not append account mapping to sequence");

    addMappingItems(accountMapping, map->getMapping());
}

void YamlEmitter::serializePreference(MappingNode *map, const char *preference_str)
{
    if (map->getType() != MAPPING)
        throw YamlEmitterException("Node type is not a mapping while writing preferences");

    int preferenceid;
    if ((preferenceid = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) preference_str, -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
        throw YamlEmitterException("Could not add scalar to document");

    int preferenceMapping;
    if ((preferenceMapping = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
        throw YamlEmitterException("Could not add mapping to document");

    if (yaml_document_append_mapping_pair(&document_, topLevelMapping_, preferenceid, preferenceMapping) == 0)
        throw YamlEmitterException("Could not add mapping pair to top leve mapping");

    addMappingItems(preferenceMapping, map->getMapping());
}

void YamlEmitter::addMappingItems(int mappingID, YamlNodeMap &iMap)
{
    for (const auto &i : iMap)
        addMappingItem(mappingID, i.first, i.second);
}

void YamlEmitter::addMappingItem(int mappingid, const std::string &key, YamlNode *node)
{
    if (node->getType() == SCALAR) {
        ScalarNode *sclr = static_cast<ScalarNode *>(node);

        int temp1;
        if ((temp1 = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) key.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
            throw YamlEmitterException("Could not add scalar to document");

        int temp2;
        if ((temp2 = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) sclr->getValue().c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
            throw YamlEmitterException("Could not add scalar to document");

        if (yaml_document_append_mapping_pair(&document_, mappingid, temp1, temp2) == 0)
            throw YamlEmitterException("Could not append mapping pair to mapping");

    } else if (node->getType() == MAPPING) {
        MappingNode *map = static_cast<MappingNode *>(node);

        int temp1;
        if ((temp1 = yaml_document_add_scalar(&document_, NULL, (yaml_char_t *) key.c_str(), -1, YAML_PLAIN_SCALAR_STYLE)) == 0)
            throw YamlEmitterException("Could not add scalar to document");

        int temp2;
        if ((temp2 = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
            throw YamlEmitterException("Could not add scalar to document");

        if (yaml_document_append_mapping_pair(&document_, mappingid, temp1, temp2) == 0)
            throw YamlEmitterException("Could not add mapping pair to mapping");

        addMappingItems(temp2, map->getMapping());

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
        for (const auto &it : *seq) {
            YamlNode *yamlNode = it;
            int id;
            if ((id = yaml_document_add_mapping(&document_, NULL, YAML_BLOCK_MAPPING_STYLE)) == 0)
                throw YamlEmitterException("Could not add account mapping to document");

            if (yaml_document_append_sequence_item(&document_, temp2, id) == 0)
                throw YamlEmitterException("Could not append account mapping to sequence");

            MappingNode *mapnode = static_cast<MappingNode*>(yamlNode);
            addMappingItems(id, mapnode->getMapping());
        }
    } else
        throw YamlEmitterException("Unknown node type while adding mapping node");
}
}
