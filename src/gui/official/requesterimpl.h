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

#ifndef SFLPHONEGUI_REQUESTERIMPL_H
#define SFLPHONEGUI_REQUESTERIMPL_H

#include "request.h"
#include "objectfactory.h"

class Call;
class SessionIO;

class RequesterImpl
{
 public:
  RequesterImpl();

  /**
   * Will send the command to the sflphone's server.
   * This command is non-blocking. The command linked
   * to this command will be executed.
   */
  std::string send(const std::string &sessionId,
		   const std::string &command,
		   const std::list< std::string > &args);

  void receiveAnswer(const std::string &answer);
  void receiveAnswer(const std::string &code, 
		     const std::string &sequence, 
		     const std::string &message);


  static int getCodeCategory(const std::string &code);

  /**
   * Generate a unique call ID. 
   */
  std::string generateCallId();

  /**
   * Generate a unique session ID.
   */
  std::string generateSessionId();

  /**
   * Generate a unique sequence ID.
   */
  std::string generateSequenceId();

  /**
   * Register the string to return a Actual type.
   */
  template< typename Actual >
    void registerObject(const std::string &name);

  std::string getSessionIdFromSequenceId(const std::string &sequence)
    {return mSequenceToSession[sequence];}

  /**
   * Register the session.
   */
  void registerSession(const std::string &id, SessionIO *io);

 private:

  /**
   * Return the SessionIO instance related to
   * the session ID.
   */
  SessionIO *getSessionIO(const std::string &sessionId);

  /**
   * Register the string to return a Actual type.
   */
  void registerRequest(const std::string &sessionId,
		       const std::string &sequenceId,
		       Request *request);


 private:
  ObjectFactory< Request > mRequestFactory;
  std::map< std::string, SessionIO * > mSessions;
  std::map< std::string, Request * > mRequests;
  std::map< std::string, std::string > mSequenceToSession;
  
  
  /**
   * This is the integer used to generate the call IDs.
   */
  unsigned long mCallIdCount;
  unsigned long mSessionIdCount;
  unsigned long mSequenceIdCount;
};

#include "requesterimpl.inl"

#endif
