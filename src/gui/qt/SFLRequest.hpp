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




#ifndef SFLPHONEGUI_SFLREQUEST_HPP
#define SFLPHONEGUI_SFLREQUEST_HPP

#include <list>
#include "Request.hpp"

class EventRequest : public Request
{
public:
  EventRequest(const QString &sequenceId,
	       const QString &command,
	       const std::list< QString > &args);


  virtual ~EventRequest(){}

  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended. When we have an error on an EventRequest, we should
   * quit the program.
   */
  virtual void onError(const QString &code, const QString &message);

  /**
   * This function will be called when the request 
   * receive an answer, but there's other answers to come.
   * This will be dispatched to the valid event.
   */
  virtual void onEntry(const QString &code, const QString &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended. The event handling is gone, so we should 
   * quit.
   */
  virtual void onSuccess(const QString &code, const QString &message);

};

class CallStatusRequest : public Request
{
public:
  CallStatusRequest(const QString &sequenceId,
		    const QString &command,
		    const std::list< QString > &args);


  virtual ~CallStatusRequest(){}

  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended. When we have an error on an EventRequest, we should
   * quit the program.
   */
  virtual void onError(const QString &code, const QString &message);

  /**
   * This function will be called when the request 
   * receive an answer, but there's other answers to come.
   * This will be dispatched to the valid event.
   */
  virtual void onEntry(const QString &code, const QString &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended. The event handling is gone, so we should 
   * quit.
   */
  virtual void onSuccess(const QString &code, const QString &message);

};


class CallRequest : public AccountRequest
{
 public:
  CallRequest(const QString &sequenceId,
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
  QString mCallId;
};


class PermanentRequest : public CallRelatedRequest
{
 public:
  PermanentRequest(const QString &sequenceId,
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
};

class TemporaryRequest : public CallRelatedRequest
{
 public:
  TemporaryRequest(const QString &sequenceId,
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
};

class ConfigGetAllRequest : public Request
{
public:
  ConfigGetAllRequest(const QString &sequenceId,
		   const QString &command,
		   const std::list< QString > &args);


  virtual ~ConfigGetAllRequest(){}


  virtual void onError(const QString &code, const QString &message);
  virtual void onEntry(const QString &code, const QString &message);
  virtual void onSuccess(const QString &code, const QString &message);
};

class ConfigSaveRequest : public Request
{
public:
  ConfigSaveRequest(const QString &sequenceId,
		    const QString &command,
		    const std::list< QString > &args);


  virtual ~ConfigSaveRequest(){}


  virtual void onError(const QString &code, const QString &message);
  virtual void onSuccess(const QString &code, const QString &message);
};

class StopRequest : public Request
{
public:
  StopRequest(const QString &sequenceId,
		    const QString &command,
		    const std::list< QString > &args);


  virtual ~StopRequest(){}
  virtual void onError(const QString &code, const QString &message);
  virtual void onSuccess(const QString &code, const QString &message);
};


class SignalizedRequest : public Request
{
  Q_OBJECT

public:
  SignalizedRequest(const QString &sequenceId,
		    const QString &command,
		    const std::list< QString > &args);

  virtual void onError(const QString &code, const QString &message);
  virtual void onEntry(const QString &code, const QString &message);
  virtual void onSuccess(const QString &code, const QString &message);

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
};

#endif
