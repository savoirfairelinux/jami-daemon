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

#include "yamlnode.h"
#include "src/global.h"

namespace Conf
{


void YamlDocument::addNode (YamlNode *node)
{
    Sequence::iterator it = doc.end();
    doc.insert (it, node);
}

YamlNode *YamlDocument::popNode()
{
    YamlNode *node = doc.front();

    //removed element's destructor is called
    doc.pop_front();

    return node;
}

void YamlDocument::deleteChildNodes()
{
    Sequence::iterator it = doc.begin();

    while (it != doc.end()) {
        YamlNode *yamlNode = static_cast<YamlNode *> (*it);

        switch (yamlNode->getType()) {
            case DOCUMENT:
                break;
            case SEQUENCE: {
                SequenceNode *sequence = static_cast<SequenceNode *> (yamlNode);
                sequence->deleteChildNodes();
                delete sequence;
                sequence = NULL;
            }
            break;
            case MAPPING: {
                MappingNode *mapping = static_cast<MappingNode *> (yamlNode);
                mapping->deleteChildNodes();
                delete mapping;
                mapping = NULL;
            }
            break;
            case SCALAR: {
                ScalarNode *scalar = static_cast<ScalarNode *> (yamlNode);
                delete scalar;
                scalar = NULL;
            }
            break;
            default:
                break;
        }

        it++;
    }
}

void MappingNode::addNode (YamlNode *node)
{
    Mapping::iterator it = map.end();
    map.insert (it, std::pair<std::string, YamlNode *> (tmpKey, node));
}

void MappingNode::setKeyValue (const std::string &key, YamlNode *value)
{
    Mapping::iterator it = map.end();
    map.insert (it, std::pair<std::string, YamlNode *> (key, value));
}

void MappingNode::removeKeyValue (const std::string &key)
{

    Mapping::iterator it = map.find (key);
    map.erase (it);
}


YamlNode *MappingNode::getValue (const std::string &key)
{

    Mapping::iterator it = map.find (key);

    if (it != map.end()) {
        return it->second;
    } else {
        _debug ("MappingNode: Could not find %s", key.c_str());
        return NULL;
    }
}

void MappingNode::getValue (const std::string &key, bool *b)
{
	ScalarNode *node = (ScalarNode*)getValue(key);
	if (!node)
		return;

	const std::string &v = node->getValue();
	*b = v == "true";
}

void MappingNode::getValue (const std::string &key, int *i)
{
	ScalarNode *node = (ScalarNode*)getValue(key);
	if (!node)
		return;

	const std::string &v = node->getValue();
	*i = atoi(v.c_str());
}

void MappingNode::getValue (const std::string &key, std::string *v)
{
	ScalarNode *node = (ScalarNode*)getValue(key);
	if (!node)
		return;

	*v = node->getValue();
}


void MappingNode::deleteChildNodes()
{
    Mapping::iterator it;

    for (it = map.begin(); it != map.end(); ++it) {
        YamlNode *yamlNode = static_cast<YamlNode *> (it->second);
        if (!yamlNode)
            continue;

        switch (yamlNode->getType()) {
            case DOCUMENT:
                break;
            case SEQUENCE: {
                SequenceNode *sequence = static_cast<SequenceNode *> (yamlNode);
                sequence->deleteChildNodes();
                delete sequence;
                sequence = NULL;
            }
            break;
            case MAPPING: {
                MappingNode *mapping = static_cast<MappingNode *> (yamlNode);
                mapping->deleteChildNodes();
                delete mapping;
                mapping = NULL;
            }
            break;
            case SCALAR: {
                ScalarNode *scalar = static_cast<ScalarNode *> (yamlNode);
                delete scalar;
                scalar = NULL;
            }
            break;
            default:
                break;
        }
    }
}

void SequenceNode::addNode (YamlNode *node)
{
    Sequence::iterator it = seq.end();
    seq.insert (it, node);
}


void SequenceNode::deleteChildNodes()
{
    Sequence::iterator it;
    for (it = seq.begin(); it != seq.end(); ++it) {
        YamlNode *yamlNode = static_cast<YamlNode *> (*it);

        switch (yamlNode->getType()) {
            case DOCUMENT:
                break;
            case SEQUENCE: {
                SequenceNode *sequence = static_cast<SequenceNode *> (yamlNode);
                sequence->deleteChildNodes();
                delete sequence;
            }
            break;
            case MAPPING: {
                MappingNode *mapping = static_cast<MappingNode *> (yamlNode);
                mapping->deleteChildNodes();
                delete mapping;
            }
            break;
            case SCALAR: {
                ScalarNode *scalar = static_cast<ScalarNode *> (yamlNode);
                delete scalar;
            }
            break;
            default:
                break;
        }
    }
}

}
