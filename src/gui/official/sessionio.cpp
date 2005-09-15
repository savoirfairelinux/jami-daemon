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

InputStreamer::InputStreamer(SessionIO *sessionIO)
  : mSessionIO(sessionIO)
{}

void
InputStreamer::run()
{
  while(mSessionIO->isUp()) {
    mSessionIO->receive();
  }
}

void 
OutputStreamer::OutputStreamer(SessionIO *sessionIO)
  : mSessionIO(sessionIO)
{}

void
OutputStreamer::run()
{
  while(mSessionIO->isUp()) {
    mSessionIO->send();
  }
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

void
SessionIO::run()
{
  stop();
  //just protecting the mutex
  {
    QMutexLock guard(&mMutex);
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
  (*mOutputStream) << mOutputPool.pop();
  mOutputPool->sync();
}

void
SessionIO::receive()
{
  std::string s;
  std::getline(*mInputStream, s);
  mInputPool.push(s);
}




