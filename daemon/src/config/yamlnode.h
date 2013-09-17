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

#ifndef __YAMLNODE_H__
#define __YAMLNODE_H__

#include <string>
#include <list>
#include <map>
#include <stdexcept>

#include "noncopyable.h"
#include "global.h"

namespace Conf {

class YamlNode;

typedef std::list<YamlNode *> Sequence;
typedef std::map<std::string, YamlNode*> YamlNodeMap;

enum NodeType { DOCUMENT, SCALAR, MAPPING, SEQUENCE };

class YamlNode {
    public:
        YamlNode(NodeType t, YamlNode *top = NULL) : type_(t), topNode_(top) {}

        virtual ~YamlNode() {}

        NodeType getType() const { return type_; }

        YamlNode *getTopNode() { return topNode_; }

        virtual void deleteChildNodes() = 0;

        virtual void addNode(YamlNode *node) = 0;

        virtual YamlNode *getValue(const std::string &key) const = 0;
        virtual void getValue(const std::string &key UNUSED, bool *b) const = 0;
        virtual void getValue(const std::string &key UNUSED, int *i) const = 0;
        virtual void getValue(const std::string &key UNUSED, double *d) const = 0;
        virtual void getValue(const std::string &key UNUSED, std::string *s) const = 0;

    private:
        NON_COPYABLE(YamlNode);
        NodeType type_;
        YamlNode *topNode_;
};


class YamlDocument : public YamlNode {
    public:
        YamlDocument(YamlNode* top = NULL) : YamlNode(DOCUMENT, top), doc_() {}

        virtual void addNode(YamlNode *node);

        YamlNode *popNode();

        Sequence *getSequence() { return &doc_; }

        virtual void deleteChildNodes();

        virtual YamlNode *getValue(const std::string &key UNUSED) const { return NULL; }
        virtual void getValue(const std::string &key UNUSED, bool *b) const { *b = false; }
        virtual void getValue(const std::string &key UNUSED, int *i) const { *i = 0; }
        virtual void getValue(const std::string &key UNUSED, double *d) const { *d = 0.0; }
        virtual void getValue(const std::string &key UNUSED, std::string *s) const { *s = ""; }

    private:
        Sequence doc_;
};

class SequenceNode : public YamlNode {
    public:
        SequenceNode(YamlNode *top) : YamlNode(SEQUENCE, top), seq_() {}

        Sequence *getSequence() {
            return &seq_;
        }

        virtual void addNode(YamlNode *node);

        virtual void deleteChildNodes();

        virtual YamlNode *getValue(const std::string &key UNUSED) const { return NULL; }
        virtual void getValue(const std::string &key UNUSED, bool *b) const { *b = false; }
        virtual void getValue(const std::string &key UNUSED, int *i) const { *i = 0; }
        virtual void getValue(const std::string &key UNUSED, double *d) const { *d = 0.0; }
        virtual void getValue(const std::string &key UNUSED, std::string *s) const { *s = ""; }


    private:
        Sequence seq_;
};


class MappingNode : public YamlNode {
    public:
        MappingNode(YamlNode *top) :
            YamlNode(MAPPING, top), map_(), tmpKey_() {}

        YamlNodeMap &getMapping() { return map_; }

        virtual void addNode(YamlNode *node);

        void setTmpKey(std::string key) { tmpKey_ = key; }

        void setKeyValue(const std::string &key, YamlNode *value);

        void removeKeyValue(const std::string &key);

        YamlNode *getValue(const std::string &key) const;
        void getValue(const std::string &key, bool *b) const;
        void getValue(const std::string &key, int *i) const;
        void getValue(const std::string &key, double *d) const;
        void getValue(const std::string &key, std::string *s) const;

        virtual void deleteChildNodes();

    private:
        YamlNodeMap map_;
        std::string tmpKey_;
};

class ScalarNode : public YamlNode {
    public:
        ScalarNode(const std::string &s, YamlNode *top = NULL) : YamlNode(SCALAR, top), str_(s) {}
        /* This avoids const char * arguments calling the constructor, see #20793 */
        ScalarNode(const char *s, YamlNode *top = NULL) : YamlNode(SCALAR, top), str_(s) {}
        ScalarNode(bool b, YamlNode *top = NULL) : YamlNode(SCALAR, top), str_(b ? "true" : "false") {}

        virtual void addNode(YamlNode *node UNUSED) {}

        const std::string &getValue() const { return str_; }
        void setValue(const std::string &s) { str_ = s; }

        virtual YamlNode *getValue(const std::string &key UNUSED) const { return NULL; }
        virtual void getValue(const std::string &key UNUSED, bool *b) const { *b = false; }
        virtual void getValue(const std::string &key UNUSED, int *i) const { *i = 0; }
        virtual void getValue(const std::string &key UNUSED, double *d) const { *d = 0.0; }
        virtual void getValue(const std::string &key UNUSED, std::string *s) const { *s = ""; }

        virtual void deleteChildNodes() {}

    private:
        std::string str_;
};

}

#endif

