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

#ifndef __YAMLPARSER_H__
#define __YAMLPARSER_H__

#include "yamlnode.h"
#include <yaml.h>
#include <stdio.h>
#include <exception>
#include <string>

namespace Conf {

#define PARSER_BUFFERSIZE 65536
#define PARSER_MAXEVENT 1024

class YamlParserException : public std::exception
{
 public:
  YamlParserException(const std::string& str="") throw() : errstr(str) {}
  
  virtual ~YamlParserException() throw() {}

  virtual const char *what() const throw() {
    std::string expt("YamlParserException occured: ");
    expt.append(errstr);

    return expt.c_str();
  }
 private:
  std::string errstr;
};


class YamlParser {

 public:

  YamlParser(const char *file);

  ~YamlParser();

  void open();

  void close();

  void serializeEvents();

  YamlDocument *composeEvents();

  void constructNativeData();

  SequenceNode *getAccountSequence(void) { return accountSequence; };

  SequenceNode *getPreferenceSequence(void) { return preferenceSequence; }

  SequenceNode *getAddressbookSequence(void) { return addressbookSequence; }

  SequenceNode *getAudioSequence(void) { return audioSequence; }

  SequenceNode *getHookSequence(void) { return hooksSequence; }

  SequenceNode *getVoipPreferenceSequence(void) { return voiplinkSequence; }

 private:

  /**
   * Copy yaml parser event in event_to according to their type.
   */
  int copyEvent(yaml_event_t *event_to, yaml_event_t *event_from);

  void processStream(void);

  void processDocument(void);

  void processScalar(YamlNode *topNode);

  void processSequence(YamlNode *topNode);

  void processMapping(YamlNode *topNode);

  void mainNativeDataMapping(MappingNode *map);

  //   void buildAccounts(SequenceNode *map);

  std::string filename;

  FILE *fd;

  /**
   * The parser structure. 
   */
  yaml_parser_t parser;

  /**
   * The event structure array.
   */ 
  yaml_event_t events[PARSER_MAXEVENT];

  /**
   * 
   */
  unsigned char buffer[PARSER_BUFFERSIZE];

  /**
   * Number of event actually parsed
   */
  int eventNumber;

  YamlDocument *doc;

  int eventIndex;

  SequenceNode *accountSequence;

  SequenceNode *preferenceSequence;

  SequenceNode *addressbookSequence;

  SequenceNode *audioSequence;

  SequenceNode *hooksSequence;

  SequenceNode *voiplinkSequence;

};

}

#endif
