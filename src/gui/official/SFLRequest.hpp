


#ifndef SFLPHONEGUI_SFLREQUEST_HPP
#define SFLPHONEGUI_SFLREQUEST_HPP

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

#endif
