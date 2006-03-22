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

#include <list>

#include "Request.hpp"
#include "ObjectFactory.hpp"

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
  Request *send(const QString &sessionId,
		const QString &command,
		const std::list< QString > &args);
  
  void receiveAnswer(const QString &answer);
  void receiveAnswer(const QString &code, 
		     const QString &sequence, 
		     const QString &message);


  static int getCodeCategory(const QString &code);

  /**
   * Generate a unique call ID. 
   */
  QString generateCallId();

  /**
   * Generate a unique session ID.
   */
  QString generateSessionId();

  /**
   * Generate a unique sequence ID.
   */
  QString generateSequenceId();

  /**
   * Register the string to return a Actual type.
   */
  template< typename Actual >
    void registerObject(const QString &name);

  /**
   * Register the default request to be created if
   * the command isn't registered.
   */
  template< typename Actual >
    void registerDefaultObject();


  QString getSessionIdFromSequenceId(const QString &sequence)
  {return mSequenceToSession[sequence];}

  /**
   * Register the session.
   */
  void registerSession(const QString &id, SessionIO *io);

  /**
   * Will ask the session IO with id to connect.
   */
  void connect(const QString &id);

  /**
   * This function is used to notify that the SessionIO
   * input of a session is down. It means that we no longer
   * can receive answers. 
   *
   * Note: Only SessionIO related classes should call this function.
   */
  void inputIsDown(const QString &sessionId);

  /**
   * This function is used to notify that the SessionIO
   * output of a session is down. It means that we no longer
   * can send requests.
   *
   * Note: Only SessionIO related classes should call this function.
   */
  void outputIsDown(const QString &sessionId);

 private:

  /**
   * Return the SessionIO instance related to
   * the session ID.
   */
  SessionIO *getSessionIO(const QString &sessionId);

  /**
   * Register the string to return a Actual type.
   */
  void registerRequest(const QString &sessionId,
		       const QString &sequenceId,
		       Request *request);

  Request *getRequest(const QString &sessionId);


 private:
  ObjectFactory< Request > mRequestFactory;
  std::map< QString, SessionIO * > mSessions;
  std::map< QString, Request * > mRequests;
  std::map< QString, QString > mSequenceToSession;
  
  
  /**
   * This is the integer used to generate the call IDs.
   */
  unsigned long mCallIdCount;
  unsigned long mSessionIdCount;
  unsigned long mSequenceIdCount;
};

#include "RequesterImpl.inl"

#endif
