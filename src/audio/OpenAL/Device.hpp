/*
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

#ifndef __SFLAUDIO_DEVICE_HPP__
#define __SFLAUDIO_DEVICE_HPP__

#include <string>

namespace SFLAudio
{
  class Context;

  class Device 
  {
  public:
    Device();
    Device(const std::string &name);

    /**
     * This will load the device. You shouldn't use
     * this function directly. It returns true if
     * the load is successfull.
     */
    virtual bool load() = 0;

    /**
     * This will create a context for the device.
     * If there's no current context, it will be 
     * set as the current;
     */
    virtual Context *createContext() = 0;

    /**
     * This will set the current context. If NULL is 
     * given, the context isn't changed. It returns the
     * current context.
     */
    Context *currentContext(Context *context = 0);

    std::string getName();
    void setName(const std::string &name);

    /**
     * Return true if the device is the NullDevice.
     */
    bool isNull();

  private:
    std::string mName;
    Context *mContext;

  };
}

#endif
