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

#include <qstring.h>

class Account;
class Request;
class Session;

class Call
{
 public:
  /**
   * A call is automaticaly registered in
   * the CallManager. However, a call isn't
   * registered when you have a copy constructor.
   */
  Call(const QString &sessionId, 
       const QString &accountId,
       const QString &callId,
       const QString &destination,
       bool incomming = false);
  Call(const Session &session, 
       const Account &account,
       const QString &callId,
       const QString &destination,
       bool incomming = false);

  /**
   * This function returns true if the 
   * call is waiting to be picked up.
   */
  bool isIncomming();

  QString id() const
  {return mId;}

  QString peer() const
  {return mPeer;}

  /**
   * This function will answer the call.
   */
  Request *answer();

  /**
   * This function will try to transfer the
   * call to the peer.
   */
  Request *transfer(const QString &to);

  /**
   * This function will hangup on a call.
   */
  Request *hangup();

  /**
   * ///TODO need to clarify this function.
   */
  Request *cancel();
  
  /**
   * This function will put the call on hold.
   * This *should* stop temporarly the streaming.
   */
  Request *hold();

  /**
   * This function will unhold a holding call.
   * This *should* restart a stopped streaming.
   */
  Request *unhold();

  /**
   * This function refuse and incomming call.
   * It means that the phone is ringing but we
   * don't want to answer.
   */
  Request *refuse();

  /**
   * This function will set this client to be
   * not able to receive the call. It means that 
   * the phone can still ring. But if every client
   * sent notavailable, then it will be refused.
   */
  Request *notAvailable();


  /**
   * This function will send a tone to the line.
   * This is used if you make a choice when you
   * have a voice menu.
   */
  Request *sendDtmf(char c);

 private:
  
  /**
   * This is the session id that we belong to.
   */
  QString mSessionId;

  /**
   * This is the account id that we belong to.
   */
  QString mAccountId;

  /**
   * This is the unique identifier of the call.
   */
  QString mId;

  /**
   * This is the destination of the call.
   */
  QString mPeer;

  bool mIsIncomming;
};

#endif
