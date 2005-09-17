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

#ifndef SFLPHONEGUI_SESSIONIO_H
#define SFLPHONEGUI_SESSIONIO_H

#include <string>
#include <iostream>
#include <qthread.h>

#include "objectpool.h"

class SessionIO;

/**
 * This class is the thread that will read
 * from the SessionIO.
 */
class InputStreamer : public QThread
{
 public:
  InputStreamer(SessionIO *sessionIO);

  /**
   * This is the main processing function.
   */
  virtual void run();


 private:
  SessionIO *mSessionIO;
};



/**
 * This class is the thread that will write
 * to the SessionIO.
 */
class OutputStreamer : public QThread
{
 public:
  OutputStreamer(SessionIO *sessionIO);

  /**
   * This is the main processing function
   */
  virtual void run();

 private:
  SessionIO *mSessionIO;
};

/**
 * This is the main class that will handle 
 * the IO.
 */
class SessionIO
{
 public:
  friend class OutputStreamer;
  friend class InputStreamer;

  /**
   * Those streams will be the streams read or write to.
   */
  SessionIO(const std::string &id, 
	    std::istream *input, 
	    std::ostream *output);
  ~SessionIO();

  /**
   * This is the function that will start the threads
   * that will handle the streams.
   */
  void start();

  /**
   * This function will stop the streaming
   * processing. On return, the service 
   * might not be completly down. You need
   * to use waitStop if you want to be sure.
   */
  void stop();

  /**
   * This function will wait until the 
   * service is really down.
   */
  void waitStop();

  /**
   * You can use this function for sending request.
   * The sending is non-blocking. This function will
   * send the data as it is; it will NOT add an EOL.
   * the stream will be "sync"ed.
   */
  void send(const std::string &request);

  /**
   * You can use this function to receive answers.
   * This function will wait until there's an 
   * answer to be processed.
   */
  void receive(std::string &answer);

  bool isUp();

 private:
  /**
   * This function will send to the stream 
   * the given data. EOL will be added at the
   * end of the data.
   */
  void send();
  
  /**
   * This function will read a line of data from
   * the stream.
   */
  void receive();

 public:
  const std::string id;
  
 private:
  QMutex mMutex;
  bool mIsUp;

  ObjectPool< std::string > mInputPool;
  ObjectPool< std::string > mOutputPool;

  std::istream *mInput;
  std::ostream *mOutput;
  InputStreamer mInputStreamer;
  OutputStreamer mOutputStreamer;
};



#endif

