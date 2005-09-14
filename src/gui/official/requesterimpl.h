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

class Call;

class RequesterImpl
{
 public:
  RequesterImpl();

  /**
   * Will send the command to the sflphone's server.
   * This command is non-blocking. The command linked
   * to this command will be executed.
   */
  void sendCallCommand(const std::string &sessionId,
		       const std::string &callId, 
		       const std::string &command);
  void sendCallCommand(const std::string &sessionId,
		       const std::string &callId, 
		       char *command);

  

 private:
  /**
   * Generate a unique call ID. 
   */
  std::string generateCallId();

  /**
   * Generate a unique account ID.
   */
  std::string generateAccountId();
  
  /**
   * Generate a unique session ID.
   */
  std::string generateSessionId();

  /**
   * Generate a unique sequence ID.
   */
  std::string generateSequenceId();


 private:
  std::map< std::string, SessionImpl * > mSessions;


  /**
   * This map is used to map accounts ids to session ids. 
   */
  std::map< std::string, std::string > mAccountToSessionMap;

  /**
   * This map is used to map call ids to session ids.
   */
  std::map< std::string, std::string > mCallToSessionMap;

  /**
   * This is the list of all accounts in each session.
   */
  std::map< std::string, std::list< std::string > > mSessionAccounts;

  /**
   * This is the list of all calls ids in each accounts.
   */
  std::map< std::string, std::list< std::string > > mAccountCalls;

  /**
   * Those maps are used to create a request from a request
   * string.
   */
  std::map< std::string, * CallRequestCreator > mCallCommandCreators;
  std::map< std::string, * AccountRequestCreator > mAccountRequestCreators;
  std::map< std::string, * SessionRequestCreator > mSessionRequestCreators;
  std::map< std::string, * SessionListRequestCreator > mSessionListRequestCreators;

  /**
   * This is the integer used to generate the call IDs.
   */
  unsigned long mCallIdCount;
}

#endif
