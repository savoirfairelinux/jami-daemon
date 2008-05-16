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

#include <audiostream.h>

static pa_channel_map channel_map ;

AudioStream::AudioStream( pa_context* context, int type, std::string desc )
{ 
  _streamType = type;
  _streamDescription = desc; 
  sample_spec.format = PA_SAMPLE_S16LE; 
  sample_spec.rate = 44100; 
  sample_spec.channels = 1; 
  channel_map.channels = 1; 
  flag = PA_STREAM_AUTO_TIMING_UPDATE;

  _audiostream =  createStream( context );
} 

AudioStream::~AudioStream()
{ 
} 

  void 
AudioStream::stream_state_callback( pa_stream* s, void* user_data )
{
  _debug("The state of the stream changed\n");
  assert(s);
  switch(pa_stream_get_state(s)){
    case PA_STREAM_CREATING:
    case PA_STREAM_TERMINATED:
      _debug("Stream is creating...\n");
      break;
    case PA_STREAM_READY:
      _debug("Stream successfully created\n");
      break;
    case PA_STREAM_FAILED:
    default:
      _debug("Stream error: %s\n" , pa_strerror(pa_context_errno(pa_stream_get_context(s))));
      break;
  }
}


  pa_stream*
AudioStream::createStream( pa_context* c )
{
  _debug("Creating %s stream...\n" , _streamDescription.c_str());
  pa_stream* s;

  assert(pa_sample_spec_valid(&sample_spec));
  assert(pa_channel_map_valid(&channel_map));

  if( !( s = pa_stream_new( c, _streamDescription.c_str() , &sample_spec, &channel_map ) ) ) 
    _debug("%s: pa_stream_new() failed : %s\n" , _streamDescription.c_str(), pa_strerror( pa_context_errno( c)));

  assert( s );

  if( _streamType == PLAYBACK_STREAM ){
    pa_stream_connect_playback( s , NULL , NULL , flag , NULL, NULL );
  }
  else if( _streamType == CAPTURE_STREAM ){
    pa_stream_connect_record( s , NULL , NULL , flag );
  }
  else if( _streamType == UPLOAD_STREAM ){
    pa_stream_connect_upload( s , 1024  );
  }
  else{
    _debug( "Stream type unknown \n");
  }

  pa_stream_set_state_callback( s , stream_state_callback, NULL);

  return s;
}

