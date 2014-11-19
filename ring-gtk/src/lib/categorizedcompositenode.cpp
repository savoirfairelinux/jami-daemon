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
#include "categorizedcompositenode.h"


CategorizedCompositeNode::CategorizedCompositeNode(CategorizedCompositeNode::Type _type) : m_type(_type)
   ,m_DropState(0),m_pParent(nullptr),m_HoverState(0)
{
}

CategorizedCompositeNode::~CategorizedCompositeNode()
{
}

char CategorizedCompositeNode::dropState()
{
   return m_DropState;
}

void CategorizedCompositeNode::setDropState(const char state)
{
   m_DropState = state;
}

CategorizedCompositeNode::Type CategorizedCompositeNode::type() const
{
   return m_type;
}

int CategorizedCompositeNode::hoverState()
{
   return m_HoverState;
}

void CategorizedCompositeNode::setHoverState(const int state)
{
   m_HoverState = state;
}

CategorizedCompositeNode* CategorizedCompositeNode::parentNode() const
{
   return m_pParent;
}

void CategorizedCompositeNode::setParentNode(CategorizedCompositeNode* node)
{
   m_pParent = node;
}
