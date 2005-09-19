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
#ifndef REQUESTFACTORY_H
#define REQUESTFACTORY_H

#include <string>
#include <map>

#include "request.h"

class Request;
/**
*/
class RequestCreatorBase
{
public:
  virtual Request *create(const std::string &sequenceId, const std::string &arg) = 0;
  virtual RequestCreatorBase *clone() = 0;
};

template< typename T >
class RequestCreator : public RequestCreatorBase
{
public:
  virtual Request *create(const std::string &sequenceId, const std::string &arg)
  {
    return new T(sequenceId, arg);
  }

  virtual RequestCreatorBase *clone()
  {
    return new RequestCreator< T >();
  }
};


class RequestFactory
{
public:
  Request *create(const std::string &requestLine);
  Request *create(
    const std::string &requestname, 
    const std::string &sequenceId, 
    const std::string &arg);

  template< typename T >
  void registerRequest(const std::string &requestname);
  void registerAll();
private:
  std::map< std::string, RequestCreatorBase * > mRequests;
};


#endif
