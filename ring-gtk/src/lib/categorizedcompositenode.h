/****************************************************************************
 *   Copyright (C) 2012-2014 by Savoir-Faire Linux                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#ifndef CATEGORIZEDCOMPOSITENODE_H
#define CATEGORIZEDCOMPOSITENODE_H

#include "typedefs.h"
#include <QtCore/QModelIndex>
class QObject;

class LIB_EXPORT CategorizedCompositeNode {
public:
    enum class Type {
        CALL     = 0,
        NUMBER   = 1,
        TOP_LEVEL= 2,
        BOOKMARK = 3,
        CONTACT  = 4,
    };
    explicit CategorizedCompositeNode(CategorizedCompositeNode::Type _type);
    virtual ~CategorizedCompositeNode();
    CategorizedCompositeNode::Type type() const;
    virtual QObject* getSelf() const = 0;
    char dropState();
    void setDropState(const char state);
    int  hoverState();
    void setHoverState(const int state);
    CategorizedCompositeNode* parentNode() const;
    void setParentNode(CategorizedCompositeNode* node);
private:
    CategorizedCompositeNode::Type m_type;
    char m_DropState;
    int  m_HoverState;
    CategorizedCompositeNode* m_pParent;
};

#endif
