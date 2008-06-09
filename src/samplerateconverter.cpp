/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "samplerateconverter.h"

SamplerateConverter::SamplerateConverter( void ) {
  
  // libSamplerateConverter-related
  // Set the converter type for the upsampling and the downsampling
  // interpolator SRC_SINC_BEST_QUALITY
  _src_state_mic  = src_new(SRC_SINC_BEST_QUALITY, 1, &_src_err);
  _src_state_spkr = src_new(SRC_SINC_BEST_QUALITY, 1, &_src_err);

  int nbSamplesMax = (int) ( 44100 * 20 /1000); // TODO Make this generic
  _floatBufferDown  = new float32[nbSamplesMax];
  _floatBufferUp = new float32[nbSamplesMax];

}

SamplerateConverter::~SamplerateConverter( void ) {

  delete [] _floatBufferUp; _floatBufferUp = NULL;
  delete [] _floatBufferDown; _floatBufferDown = NULL;

  // libSamplerateConverter-related
  _src_state_mic  = src_delete(_src_state_mic);
  _src_state_spkr = src_delete(_src_state_spkr);
}

int SamplerateConverter::upsampleData(  SFLDataFormat* dataIn , SFLDataFormat* dataOut, int samplerate1 , int samplerate2 , int nbSamples ){
  
  double upsampleFactor = (double)samplerate2 / samplerate1 ;
  int nbSamplesMax = (int) (samplerate2 * 20 /1000);  // TODO get the value from the constructor
  if( upsampleFactor != 1 )
  {
    _debug("Begin upsample data\n");
    SRC_DATA src_data;
    src_data.data_in = _floatBufferDown;
    src_data.data_out = _floatBufferUp;
    src_data.input_frames = nbSamples;
    src_data.output_frames = (int) floor(upsampleFactor * nbSamples);
    src_data.src_ratio = upsampleFactor;
    src_data.end_of_input = 0; // More data will come
    src_short_to_float_array( dataIn , _floatBufferDown, nbSamples);
    src_process(_src_state_spkr, &src_data);
    nbSamples  = ( src_data.output_frames_gen > nbSamplesMax) ? nbSamplesMax : src_data.output_frames_gen;		
    src_float_to_short_array(_floatBufferUp, dataOut, nbSamples);
  }
  return nbSamples;

}

int SamplerateConverter::downsampleData(  SFLDataFormat* dataIn , SFLDataFormat* dataOut , int samplerate1 , int samplerate2 , int nbSamples ){

  double downsampleFactor = (double) samplerate2 / samplerate1;
  int nbSamplesMax = (int) (samplerate1 * 20 / 1000); // TODO get the value from somewhere
  if ( downsampleFactor != 1)
  {
    SRC_DATA src_data;	
    src_data.data_in = _floatBufferUp;
    src_data.data_out = _floatBufferDown;
    src_data.input_frames = nbSamples;
    src_data.output_frames = (int) floor(downsampleFactor * nbSamples);
    src_data.src_ratio = downsampleFactor;
    src_data.end_of_input = 0; // More data will come
    src_short_to_float_array(dataIn, _floatBufferUp, nbSamples);
    src_process(_src_state_mic, &src_data);
    nbSamples  = ( src_data.output_frames_gen > nbSamplesMax) ? nbSamplesMax : src_data.output_frames_gen;
    _debug( "return %i samples\n" , nbSamples );
    src_float_to_short_array(_floatBufferDown, dataOut, nbSamples);
    _debug("Begin downsample data\n");
  }
  return nbSamples;
}
