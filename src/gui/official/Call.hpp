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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef SFLPHONEGUI_CALL_H
#define SFLPHONEGUI_CALL_H

#include <string>

class Session;

class Call
{
 public:
  Call(const std::string &sessionId, 
       const std::string &callId);
  Call(const Session &session, 
       const std::string &callId);

  std::string call(const std::string &to);

  /**
   * This function will answer the call.
   */
  std::string answer();

  /**
   * This function will hangup on a call.
   */
  std::string hangup();

  /**
   * ///TODO need to clarify this function.
   */
  std::string cancel();
  
  /**
   * This function will put the call on hold.
   * This *should* stop temporarly the streaming.
   */
  std::string hold();

  /**
   * This function will unhold a holding call.
   * This *should* restart a stopped streaming.
   */
  std::string unhold();

  /**
   * This function refuse and incomming call.
   * It means that the phone is ringing but we
   * don't want to answer.
   */
  std::string refuse();


  /**
   * This function will send a tone to the line.
   * This is used if you make a choice when you
   * have a voice menu.
   */
  std::string sendDtmf(char c);

 private:
  
  /**
   * This is the session id that we belong to.
   */
  std::string mSessionId;

  /**
   * This is the unique identifier of the call.
   */
  std::string mId;
};

#endif
