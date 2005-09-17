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

#include "global.h"
#include "sessionio.h"

InputStreamer::InputStreamer(SessionIO *sessionIO)
  : mSessionIO(sessionIO)
{}

void
InputStreamer::run()
{
  while(mSessionIO->isUp()) {
    mSessionIO->receive();
  }

  _debug("The Session input is down.\n");
}

OutputStreamer::OutputStreamer(SessionIO *sessionIO)
  : mSessionIO(sessionIO)
{}

void
OutputStreamer::run()
{
  while(mSessionIO->isUp()) {
    mSessionIO->send();
  }

  _debug("The Session output is down.\n");
}

SessionIO::SessionIO(std::istream *input, std::ostream *output)
  : mIsUp(false)
  , mInput(input)
  , mOutput(output)
  , mInputStreamer(this)
  , mOutputStreamer(this)
{}

SessionIO::~SessionIO()
{
  stop();
}

bool
SessionIO::isUp() 
{
  {
    QMutexLocker guard(&mMutex);
    return mIsUp;
  }
}

void
SessionIO::start()
{
  stop();
  //just protecting the mutex
  {
    QMutexLocker guard(&mMutex);
    mInputStreamer.start();
    mOutputStreamer.start();
    mIsUp = true;
  }
}

void
SessionIO::stop()
{
  mMutex.lock();
  mIsUp = false;
  mMutex.unlock();

  mInputStreamer.wait();
  mOutputStreamer.wait();
}

void 
SessionIO::send(const std::string &request)
{
  mOutputPool.push(request);
}

void 
SessionIO::receive(std::string &answer)
{
  answer = mInputPool.pop();
}

void
SessionIO::send()
{
  if(!mOutput->good()) {
    mMutex.lock();
    mIsUp = false;
    mMutex.unlock();
  }
  else {
    (*mOutput) << mOutputPool.pop();
    mOutput->flush();
  }
}

void
SessionIO::receive()
{
  if(!mInput->good()) {
    mMutex.lock();
    mIsUp = false;
    mMutex.unlock();
  }
  else {
    std::string s;
    std::getline(*mInput, s);
    if(s.size() > 0) {
      mInputPool.push(s);
    }
  }
}




