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

#ifndef __PATTERN_OBSERVER_H_
#define __PATTERN_OBSERVER_H_

#include <list>

/**
 * Observer design pattern interface
 */
namespace Pattern {

class Observer {
public:
  virtual ~Observer() {};
  virtual void update() = 0;
};

class Subject {
public:
  virtual ~Subject() {};
  void attach(Observer& observer);
  void detach(Observer& observer);
  void notify();

private:
  std::list<Observer*> _observers;
};

} // end namespace

#endif // __PATTERN_OBSERVER_H_
