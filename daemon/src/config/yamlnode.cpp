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

#include "yamlnode.h"
#include <cstdlib>
#include "logger.h"

namespace Conf {

void YamlDocument::addNode(YamlNode *node)
{
    Sequence::iterator it = doc_.end();
    doc_.insert(it, node);
}

YamlNode *YamlDocument::popNode()
{
    YamlNode *node = doc_.front();

    //removed element's destructor is called
    doc_.pop_front();

    return node;
}

void YamlDocument::deleteChildNodes()
{
    for (auto &it : doc_) {
        YamlNode *yamlNode = static_cast<YamlNode *>(it);

        yamlNode->deleteChildNodes();
        delete yamlNode;
        yamlNode = NULL;
    }
}

void MappingNode::addNode(YamlNode *node)
{
    setKeyValue(tmpKey_, node);
}


void MappingNode::setKeyValue(const std::string &key, YamlNode *value)
{
    map_[key] = value;
}

void MappingNode::removeKeyValue(const std::string &key)
{
    YamlNodeMap::iterator it = map_.find(key);
    map_.erase(it);
}

YamlNode *MappingNode::getValue(const std::string &key) const
{
    YamlNodeMap::const_iterator it = map_.find(key);

    if (it != map_.end())
        return it->second;
    else
        return NULL;
}

void MappingNode::getValue(const std::string &key, bool *b) const
{
    ScalarNode *node = static_cast<ScalarNode*>(getValue(key));
    if (!node)
        return;

    const std::string &v = node->getValue();
    *b = v == "true";
}

void MappingNode::getValue(const std::string &key, int *i) const
{
    ScalarNode *node = static_cast<ScalarNode*>(getValue(key));
    if (!node) {
        ERROR("node %s not found", key.c_str());
        return;
    }

    *i = std::atoi(node->getValue().c_str());
}

void MappingNode::getValue(const std::string &key, double *d) const
{
    ScalarNode *node = static_cast<ScalarNode*>(getValue(key));
    if (!node) {
        ERROR("node %s not found", key.c_str());
        return;
    }

    *d = std::atof(node->getValue().c_str());
}

void MappingNode::getValue(const std::string &key, std::string *v) const
{
    ScalarNode *node = static_cast<ScalarNode*>(getValue(key));

    if (!node) {
        ERROR("node %s not found", key.c_str());
        return;
    }

    *v = node->getValue();
}


void MappingNode::deleteChildNodes()
{
    for (auto &it : map_) {
        YamlNode *yamlNode = static_cast<YamlNode *>(it.second);

        if (!yamlNode)
            continue;

        yamlNode->deleteChildNodes();
        delete yamlNode;
        yamlNode = NULL;
    }
}

void SequenceNode::addNode(YamlNode *node)
{
    Sequence::iterator it = seq_.end();
    seq_.insert(it, node);
}

void SequenceNode::deleteChildNodes()
{
    for (auto &it : seq_) {
        YamlNode *yamlNode = static_cast<YamlNode *>(it);

        yamlNode->deleteChildNodes();
        delete yamlNode;
        yamlNode = NULL;
    }
}

}
