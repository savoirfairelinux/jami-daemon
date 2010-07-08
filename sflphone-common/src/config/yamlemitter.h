/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef __YAMLEMITTER_H__
#define __YAMLEMITTER_H__

#include <yaml.h>
#include <exception>
#include <string>
#include "yamlnode.h"

namespace Conf {

#define EMITTER_BUFFERSIZE 65536
#define EMITTER_MAXEVENT 1024

class YamlEmitterException : public std::exception 
{
 public:
  YamlEmitterException(const std::string& str="") throw() : errstr(str) {}

  virtual ~YamlEmitterException() throw() {}

  virtual const char *what() const throw() {
    std::string expt("YamlParserException occured: ");
    expt.append(errstr);
    
    return expt.c_str();
  }
 private:
  std::string errstr;

};

class YamlEmitter {

 public:

  YamlEmitter(const char *file);

  ~YamlEmitter();

  void open();

  void close();

  void read();

  void write();

  void serializeAccount(MappingNode *map);

  void serializePreference(MappingNode *map);

  void serializeVoipPreference(MappingNode *map);

  void serializeAddressbookPreference(MappingNode *map);

  void serializeHooksPreference(MappingNode *map);

  void serializeAudioPreference(MappingNode *map);

  void writeAudio();

  void writeHooks();

  void writeVoiplink();

  void serializeData();

 private:

  void addMappingItem(int mappingid, Key key, YamlNode *node);

  std::string filename;

  FILE *fd;

  /**
   * The parser structure. 
   */
  yaml_emitter_t emitter;

  /**
   * The event structure array.
   */ 
  yaml_event_t events[EMITTER_MAXEVENT];

  /**
   * 
   */
  unsigned char buffer[EMITTER_BUFFERSIZE];


  /**
   * Main document for this serialization
   */
  yaml_document_t document;

  /**
   * Reference id to the top levell mapping when creating
   */
  int topLevelMapping;

  /**
   * We need to add the account sequence if this is the first account to be
   */
  bool isFirstAccount;

  /**
   * Reference to the account sequence
   */
  int accountSequence;

  friend class ConfigurationTest;

};

}

#endif
