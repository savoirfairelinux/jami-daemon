/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
 */

#include "observer.h"
#include <algorithm>
namespace Pattern {

void 
Subject::attach(Observer& observer) 
{
  if (std::find(_observers.begin(), _observers.end(), &observer) == _observers.end()) {
    _observers.push_back(&observer);
  } 
}

void 
Subject::detach(Observer& observer) 
{
  std::list<Observer*>::iterator iter = std::find(_observers.begin(), _observers.end(), &observer);
  if ( iter != _observers.end()) {
    _observers.erase(iter);
  } 
}

void 
Subject::notify()
{
  std::list<Observer*>::iterator iter = _observers.begin();
  while( iter != _observers.end()) {
    (*iter)->update();
    iter++;
  }
}

} // end of namespace
