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

#ifndef __YAMLNODE_H__
#define __YAMLNODE_H__

#include <string>
#include <list>
#include <map>
#include <exception>

namespace Conf {


class YamlNode;

typedef std::string Key;
typedef std::string Value;
typedef std::list<YamlNode *> Sequence;
typedef std::map<Key, YamlNode *> Mapping;

class YamlNodeException : public std::exception
{

 public:
  YamlNodeException(const std::string& str="") throw() : errstr(str) {}

  virtual ~YamlNodeException() throw() {}

  virtual const char *what() const throw() {
    std::string expt("YamlNodeException occured: ");
    expt.append(errstr);

    return expt.c_str();
  }
 private:
  std::string errstr;

};

enum NodeType { DOCUMENT, SCALAR, MAPPING, SEQUENCE };

class YamlNode {
  
 public:

 YamlNode(NodeType t, YamlNode *top=NULL) : type(t), topNode(top) {}

  ~YamlNode() {}

  NodeType getType() { return type; }

  YamlNode *getTopNode() { return topNode; }

 private:

  NodeType type;

  YamlNode *topNode;

};


class YamlDocument : YamlNode {

 public:

 YamlDocument(YamlNode* top=NULL) : YamlNode(DOCUMENT, top) {}

  ~YamlDocument() {}

  void addNode(YamlNode *node);

  YamlNode *popNode(void);

  Sequence *getSequence(void) { return &doc; }

 private:

  Sequence doc;

 };

class SequenceNode : public YamlNode {

 public:

 SequenceNode(YamlNode *top) : YamlNode(SEQUENCE, top) {}

  ~SequenceNode() {}

  Sequence *getSequence() { return &seq; }

  void addNode(YamlNode *node);

 private:

  Sequence seq;

};


class MappingNode : public YamlNode {

 public:

 MappingNode(YamlNode *top) : YamlNode(MAPPING, top) {}

  ~MappingNode() {}

  Mapping *getMapping() { return &map; }

  void addNode(YamlNode *node);

  void setTmpKey(Key key) { tmpKey = key; }

  void  setKeyValue(Key key, YamlNode *value);

  void removeKeyValue(Key key);

  YamlNode *getValue(Key key);

 private:

  Mapping map;

  Key tmpKey;

};


class ScalarNode : public YamlNode {

 public:

  ScalarNode(Value v="", YamlNode *top=NULL) : YamlNode(SCALAR, top), val(v) {}

  ~ScalarNode() {}

  Value getValue() { return val; }

  void setValue(Value v) { val = v; }

 private:

  Value val;

};


}



#endif
