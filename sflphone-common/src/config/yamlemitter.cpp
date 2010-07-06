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

#include "yamlemitter.h"
#include <stdio.h>
#include "../global.h"

namespace Conf {

YamlEmitter::YamlEmitter(const char *file) : filename(file) 
{
  open();
}

YamlEmitter::~YamlEmitter() 
{
  close();
}

void YamlEmitter::open() 
{
  fd = fopen(filename.c_str(), "wb");

  if(!fd)
    throw YamlEmitterException("Could not open file descriptor");

  if(!yaml_emitter_initialize(&emitter))
    throw YamlEmitterException("Could not initialize emitter");

  // Use unicode format
  yaml_emitter_set_unicode(&emitter, 1);

  yaml_emitter_set_output_file(&emitter, fd);

  yaml_document_initialize(&document, NULL, NULL, NULL, 0, 0);
}

void YamlEmitter::close() 
{
  // yaml_emitter_delete(&emitter);

  if(!fd)
    throw YamlEmitterException("File descriptor not valid");
 
  fclose(fd);
  /*
  if(!fclose(fd))
    throw YamlEmitterException("Error closing file descriptor");
  */

  yaml_document_delete(&document);
}

void YamlEmitter::read() {}

void YamlEmitter::write() 
{
  serializeData();

  for(int i = 0; i < eventNumber; i++) {
    if(!yaml_emitter_emit(&emitter, &(events[i])))
      throw YamlEmitterException("Falied to emit event");
    
     yaml_emitter_flush(&emitter);
  }

}

void YamlEmitter::serializeData()
{

  unsigned char sclr[20];
  snprintf((char *)sclr, 20, "%s", "value");
  yaml_char_t *value = (yaml_char_t *)sclr;
  

  // yaml_document_add_scalar(&document, NULL, value, -1, YAML_PLAIN_SCALAR_STYLE);
  // yaml_emitter_dump(&emitter, &document);
  eventNumber = 0;

  yaml_event_t event;

  yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
  events[eventNumber++] = event;

  yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
  events[eventNumber++] = event;

  
  yaml_document_end_event_initialize(&event, 0);
  events[eventNumber++] = event;

  // yaml_scalar_event_initialize(event, yaml_char_t  *anchor, yaml_char_t  *tag, yaml_char_t  *value, int length, int plain_implicit, int quoted_implicit, yaml_scalar_style_t  style)
  yaml_scalar_event_initialize(&event, NULL, NULL, value, 5, 0, 0, YAML_PLAIN_SCALAR_STYLE);
  events[eventNumber++] = event;

  //  yaml_sequence_start_event_initialize

  //  yaml_sequence_end_event_initialize  

  //  yaml_mapping_start_event_initialize

  //  yaml_mapping_end_event_initialize

  //  yaml_event_delete

  yaml_stream_end_event_initialize(&event);
  events[eventNumber++] = event;

}


void YamlEmitter::writeDocument()
{
  unsigned char sclr[20];
  snprintf((char *)sclr, 20, "%s", "value");
  yaml_char_t *value = (yaml_char_t *)sclr;

  yaml_document_add_scalar(&document, NULL, value, -1, YAML_PLAIN_SCALAR_STYLE);
  yaml_emitter_dump(&emitter, &document);

}

}
