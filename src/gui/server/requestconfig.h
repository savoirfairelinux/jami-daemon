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

#ifndef __REQUESTCONFIG_H__
#define __REQUESTCONFIG_H__

#include "request.h"


class RequestZeroconf : public RequestGlobal {
public:
  RequestZeroconf(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};


class RequestZeroconfEvent : public RequestGlobal {
public:
  RequestZeroconfEvent(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};


class RequestCallStatus : public RequestGlobal {
public:
  RequestCallStatus(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};


class RequestConfigGetAll : public RequestGlobal {
public:
  RequestConfigGetAll(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};


class RequestConfigGet : public RequestGlobal {
public:
  RequestConfigGet(const std::string &sequenceId, const TokenList& argList);
  ResponseMessage execute();
private:
  std::string _section;
  std::string _name;
};


class RequestConfigSet : public RequestGlobal {
public:
  RequestConfigSet(const std::string &sequenceId, const TokenList& argList);
  ResponseMessage execute();
private:
  std::string _section;
  std::string _name;
  std::string _value;
};


class RequestConfigSave : public RequestGlobal {
public:
  RequestConfigSave(const std::string &sequenceId, const TokenList& argList) : RequestGlobal(sequenceId,argList) {}
  ResponseMessage execute();
};

class RequestList : public RequestGlobal {
public:
  RequestList(const std::string &sequenceId, const TokenList& argList);
  ResponseMessage execute();
private:
  std::string _name;
};

#endif // __REQUESTCONFIG_H__
