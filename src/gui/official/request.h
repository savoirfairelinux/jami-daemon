/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef SFLPHONEGUI_REQUEST_H
#define SFLPHONEGUI_REQUEST_H

#include <list>
#include <string>

#include "call.h"

class Request
{
 public:
  Request(const std::string &sequenceId,
	  const std::string &command,
	  const std::list< std::string > &args);

  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended.
   */
  virtual void onError(const std::string &code, const std::string &message);

  /**
   * This function will be called when the request 
   * receive an answer, but there's other answers to come.
   */
  virtual void onEntry(const std::string &code, const std::string &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended.
   */
  virtual void onSuccess(const std::string &code, const std::string &message);

  /**
   * This function will translate the function into a string.
   * This is used for sending the request through a text channel.
   */
  std::string toString();

  /**
   * Return the sequence ID.
   */
  std::string getSequenceId()
    {return mSequenceId;}

  /**
   * Return the command.
   */
  std::string getCommand()
    {return mCommand;}

  /**
   * Return the args.
   */
  std::list< std::string > getArgs()
    {return mArgs;}


 private:
  const std::string mSequenceId;
  const std::string mSessionId;
  const std::string mCommand;
  const std::list< std::string > mArgs;
};

class CallRequest : public Request
{
 public:
  CallRequest(const std::string &sequenceId,
	      const std::string &command,
	      const std::list< std::string > &args);


  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended. 
   */
  virtual void onError(Call call, 
		       const std::string &code, 
		       const std::string &message);

  /**
   * This function will be called when the request 
   * receive an answer, but there's other answers to come.
   */
  virtual void onEntry(Call call,
		       const std::string &code, 
		       const std::string &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended.
   */
  virtual void onSuccess(Call call, 
			 const std::string &code,
			 const std::string &message);

 private:
  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended. This function will call the onError, but with
   * the call linked to this request.
   */
  virtual void onError(const std::string &code, 
		       const std::string &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended. This function will call the onSuccess function, 
   * but with the call linked to this request.
   */
  virtual void onSuccess(const std::string &code, 
			 const std::string &message);


 private:
  const std::string mCallId;
};

#endif
