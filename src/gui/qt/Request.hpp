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
#include <qobject.h>
#include <qstring.h>


#include "Account.hpp"
#include "Call.hpp"

class Request : public QObject
{
  Q_OBJECT

signals:
  /**
   * Be aware that the first string is the message,
   * and the second is the code. This is done that
   * way because usually the message is more important
   * than the code.
   */
  void error(QString, QString);
  void success(QString, QString);
  void entry(QString, QString);
  void parsedEntry(QString, QString, QString, QString, QString);

public:
  Request(const QString &sequenceId,
	  const QString &command,
	  const std::list< QString > &args);

  virtual ~Request(){}

  /**
   * This function will parse the message and will cut the message
   * in many arguments.
   */
  static std::list< QString > parseArgs(const QString &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended.
   */
  virtual void onError(const QString &code, const QString &message);

  /**
   * This function will be called when the request 
   * receive an answer, but there's other answers to come.
   */
  virtual void onEntry(const QString &code, const QString &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended.
   */
  virtual void onSuccess(const QString &code, const QString &message);

  /**
   * This function will translate the function into a string.
   * This is used for sending the request through a text channel.
   */
  QString toString();

  /**
   * Return the sequence ID.
   */
  QString getSequenceId()
    {return mSequenceId;}

  /**
   * Return the command.
   */
  QString getCommand()
    {return mCommand;}

  /**
   * Return the args.
   */
  std::list< QString > getArgs()
    {return mArgs;}


 private:
  const QString mSequenceId;
  const QString mCommand;
  const std::list< QString > mArgs;
};

class CallRelatedRequest : public Request
{
 public:
  CallRelatedRequest(const QString &sequenceId,
		     const QString &command,
		     const std::list< QString > &args);


  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended. 
   */
  virtual void onError(Call call, 
		       const QString &code, 
		       const QString &message);

  /**
   * This function will be called when the request 
   * receive an answer, but there's other answers to come.
   */
  virtual void onEntry(Call call,
		       const QString &code, 
		       const QString &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended.
   */
  virtual void onSuccess(Call call, 
			 const QString &code,
			 const QString &message);

 private:
  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended. This function will call the onError, but with
   * the call linked to this request.
   */
  virtual void onError(const QString &code, 
		       const QString &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the there's other answer to 
   * come. This function will call the onEntry, but with
   * the call linked to this request.
   */
  virtual void onEntry(const QString &code, 
		       const QString &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended. This function will call the onSuccess function, 
   * but with the call linked to this request.
   */
  virtual void onSuccess(const QString &code, 
			 const QString &message);


 private:
  QString mCallId;
};

class AccountRequest : public Request
{
 public:
  AccountRequest(const QString &sequenceId,
		 const QString &command,
		 const std::list< QString > &args);

  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended. 
   */
  virtual void onError(Account account, 
		       const QString &code, 
		       const QString &message);

  /**
   * This function will be called when the request 
   * receive an answer, but there's other answers to come.
   */
  virtual void onEntry(Account account,
		       const QString &code, 
		       const QString &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended.
   */
  virtual void onSuccess(Account account, 
			 const QString &code,
			 const QString &message);

 private:
  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended. This function will call the onError, but with
   * the account linked to this request.
   */
  virtual void onError(const QString &code, 
		       const QString &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the there's other answer to 
   * come. This function will call the onEntry, but with
   * the account linked to this request.
   */
  virtual void onEntry(const QString &code, 
		       const QString &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended. This function will call the onSuccess function, 
   * but with the account linked to this request.
   */
  virtual void onSuccess(const QString &code, 
			 const QString &message);


 private:
  const QString mAccountId;
};

#endif
