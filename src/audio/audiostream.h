/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 */

#ifndef _AUDIO_STREAM_H
#define _AUDIO_STREAM_H

#include <pulse/pulseaudio.h>
#include <string>

#include "../global.h"
#include "ringbuffer.h"
#include "audioloop.h"

enum STREAM_TYPE {
  PLAYBACK_STREAM,
  CAPTURE_STREAM,
  UPLOAD_STREAM
};


class AudioStream {
  public:
    AudioStream(pa_context* context , int type, std::string desc);
    ~AudioStream();

    int putMain( void* buffer , int toCopy );
    int putUrgent( void* buffer , int toCopy );

    pa_stream* pulseStream(){ return _audiostream; }

  private:
    pa_stream* createStream( pa_context* c ); 

    static void stream_state_callback( pa_stream* s, void* user_data );	
    static void audioCallback ( pa_stream* s, size_t bytes, void* userdata );
    void write( void );

    int _streamType;
    std::string _streamDescription;


    pa_stream* _audiostream;
    pa_stream_flags_t flag;
    pa_sample_spec sample_spec ;
    //pa_channel_map channel_map;
    pa_volume_t volume;

};

#endif // _AUDIO_STREAM_H
