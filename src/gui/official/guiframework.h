/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#ifndef __GUI_FRAMEWORK_H__
#define __GUI_FRAMEWORK_H__

/* Inherited class by GUI classes */
/* The GuiFramework class is the base of all user interface */

#include <list>
#include <string>
#include <vector>


class Account;

class SFLPhoneClient
{
 public:
  /**
   * This function just return the dotted formated
   * version. like: "0.5.6"
   */
  std::string version();
  
  /**
   * This function will connect the client to the 
   * sflphone server.
   */
  void connect();

  /**
   * This function will disconnect the client from 
   * the sflphone server.
   */
  void disconnect();

  /**
   * This function returns the current list of config 
   * entries of sflphone's server.  
   */
  std::list< ConfigEntry > listConfig();

  /**
   * This function will save the current config. Then,
   * next time sflphone will start, it will have the
   * same configuration has it is right now.
   */
  void saveConfig();

  /**
   * This function will set the specified option
   * to the value given in argument. You need to
   * call the saveConfig() function if you want 
   * sflphone's server to keep this option for
   * the next restart.
   *
   * Note: Be carefull because when you want
   * to set the audio device, or the audio codec, you
   * need to set the value of the position
   * in the vector, not the description of the device,
   * or the codec.
   */
  void setOption(const std::string &name, const std::string &value);

  /**
   * This function will return the value of 
   * the specified option.
   */
  std::string getOption(const std::string &name);

  /**
   * This function returns a vector containing the 
   * audio devices. See setOption's description note 
   * if you want to set the audio device.
   */
  std::vector< std::string > listAudioDevices();

  /**
   * This function returns a vector containing the 
   * audio codecs. See setOption's description note 
   * if you want to set the audio codec.
   */
  std::vector< std::string > listAudioCodecs();

  /**
   * This function returns a list of accounts available
   * on the system.
   */
  std::list< std::string > listAccounts();

  /**
   * This function returns the Account structure identified
   * by the string given in argument. Use listAccounts() function
   * if you want to know which ones are availables.
   */
  Account *getAccount(const std::string &name);
}

#endif // __GUI_FRAMEWORK_H__
