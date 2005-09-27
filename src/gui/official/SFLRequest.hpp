


#ifndef SFLPHONEGUI_SFLREQUEST_HPP
#define SFLPHONEGUI_SFLREQUEST_HPP

#include "Request.hpp"

class EventRequest : public Request
{
  virtual ~EventRequest(){}

  /**
   * This function will be called when the request 
   * receive its answer, if the request didn't successfully
   * ended. When we have an error on an EventRequest, we should
   * quit the program.
   */
  virtual void onError(const std::string &code, const std::string &message);

  /**
   * This function will be called when the request 
   * receive an answer, but there's other answers to come.
   * This will be dispatched to the valid event.
   */
  virtual void onEntry(const std::string &code, const std::string &message);

  /**
   * This function will be called when the request 
   * receive its answer, if the request successfully
   * ended. The event handling is gone, so we should 
   * quit.
   */
  virtual void onSuccess(const std::string &code, const std::string &message);

};

#endif
