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

#include <samplecache.h>

SampleCache::SampleCache( pa_stream* s ):_stream(s)
{
  //_stream = s ;
}

SampleCache::~SampleCache()
{
  //delete _pulse;
}

bool
SampleCache::uploadSample( SFLDataFormat* buffer UNUSED, size_t size UNUSED )
{
  //pa_stream_write( pulse->caching , buffer , size  , pa_xfree, 0 , PA_SEEK_RELATIVE);
  //pa_stream_finish_upload( pulse->caching );
  return true;
}  
