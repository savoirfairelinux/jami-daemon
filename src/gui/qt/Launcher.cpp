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

#include "DebugOutput.hpp"
#include "Launcher.hpp"

Launcher::Launcher()
  : mProc(NULL)
{}

Launcher::~Launcher()
{
  delete mProc;
}

void 
Launcher::start()
{
  if(!mProc) {
    DebugOutput::instance() << QObject::tr("Launcher::start()\n");
    mProc = new QProcess(this);
    mProc->addArgument("sflphoned");

    connect(mProc, SIGNAL(processExited()),
	    this, SLOT(stop()));
    connect(mProc, SIGNAL(readyReadStdout()),
	    this, SLOT(readOutput()));
    connect(mProc, SIGNAL(readyReadStderr()),
	    this, SLOT(readError()));

    if(!mProc->start()) {
      DebugOutput::instance() << tr("Launcher: Couldn't launch sflphoned.\n");
      emit error();
    }
    else {
      DebugOutput::instance() << tr("Launcher: sflphoned launched.\n");
      emit started();
    }
  }
}

void
Launcher::stop()
{
  if(mProc) {
    mProc->kill();
    delete mProc;
    mProc = NULL;
    emit stopped();
  }
}

void
Launcher::readOutput()
{
  if(mProc) {
    //emit daemonOutputAvailable(mProc->readLineStdout());
    DebugOutput::instance() << tr("%1\n").arg(mProc->readLineStdout());
  }
  else {
    DebugOutput::instance() << tr("Launcher: Trying to read output without "
				  "a valid process.\n");
  }
}

void
Launcher::readError()
{
  if(mProc) {
    DebugOutput::instance() << tr("%1\n").arg(mProc->readLineStderr());
  }
  else {
    DebugOutput::instance() << tr("Launcher: Trying to read error without "
				  "a valid process.\n");
  }
}
