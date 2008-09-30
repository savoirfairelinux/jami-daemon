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

#ifndef _SAMPLE_CACHE_H
#define _SAMPLE_CACHE_H

#include <pulse/pulseaudio.h>
#include <audiolayer.h>

class SampleCache {
 
  public:
    SampleCache( pa_stream* stream );
    ~SampleCache();

    // Copy Constructor
    SampleCache(const SampleCache& rh):_stream(rh._stream) {
      _debug("SampleCache copy constructor hasn't been implemented yet. Quit!");
      exit(0); 
    }

    // Assignment Operator
    SampleCache& operator=( const SampleCache& rh){
	_debug("SampleCache assignment operator hasn't been implemented yet. Quit!");
   	exit(0);
    }  

    bool uploadSample( SFLDataFormat* buffer, size_t size );
    bool removeSample( );
    bool isSampleCached( );

  private:
    pa_stream* _stream;

};

#endif // _SAMPLE_CACHE_H
