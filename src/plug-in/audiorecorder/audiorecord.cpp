/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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
 */

#include "audiorecord.h"

AudioRecord::AudioRecord(){
  sndSmplRate_ = 44100;
  channels_ = 1;
  byteCounter_ = 0;

}


void AudioRecord::setSndSamplingRate(int smplRate){
  sndSmplRate_ = smplRate;
}


void AudioRecord::openFile(std::string fileName, FILE_TYPE type, SOUND_FORMAT format) {

  channels_ =1;
  fileType_ = type;
  byteCounter_ = 0;
  sndFormat_ = format;

  bool result = false;

  if(fileType_ == FILE_RAW){
    result = setRawFile( fileName.c_str() );
  }
  else if (fileType_ == FILE_WAV){
    result = setWavFile( fileName.c_str() );
  }

}


void AudioRecord::closeFile() {

  if (fp == 0) return;

  if (fileType_ == FILE_RAW)
    fclose(fp);
  else if (fileType_ == FILE_WAV)
    this->closeWavFile();

}


bool AudioRecord::isOpenFile() {
  
  if(fp)
    return true;
  else
    return false;
}


bool AudioRecord::setRawFile(const char *fileName) {

  char name[8192];
  strncpy(name, fileName, 8192);
  if ( strstr(name, ".raw") == NULL) strcat(name, ".raw");
  fp = fopen(name, "wb");
  if ( !fp ) {
    _debug("AudioRecord: could not create RAW file!\n");
    return false;
  }

  if ( sndFormat_ != INT16 ) { // TODO need to change INT16 to SINT16
    sndFormat_ = INT16;
    _debug("AudioRecord: using 16-bit signed integer data format for file.\n");
  }

  _debug("AudioRecord: creating RAW file.\n");
  return true;
}


bool AudioRecord::setWavFile(const char *fileName) {
  
  char name[8192];
  strncpy(name, fileName, 8192);
  if ( strstr(name, ".wav") == NULL) strcat(name, ".wav");
  fp = fopen(name, "wb");
  if ( !fp ) {
    _debug("AudioRecord: could not create WAV file.\n");
    return false;
  }

  struct wavhdr hdr = {"RIF", 44, "WAV", "fmt", 16, 1, 1,
                        44100, 0, 2, 16, "dat", 0};
  hdr.riff[3] = 'F';
  hdr.wave[3] = 'E';
  hdr.fmt[3]  = ' ';
  hdr.data[3] = 'a';
  hdr.num_chans = channels_;
  if ( sndFormat_ == INT16 ) { //  TODO need to write INT16 to SINT16
    hdr.bits_per_samp = 16;
  }
  hdr.bytes_per_samp = (SINT16) (channels_ * hdr.bits_per_samp / 8);
  hdr.bytes_per_sec = (SINT32) (hdr.sample_rate * hdr.bytes_per_samp);

  
  if ( fwrite(&hdr, 4, 11, fp) != 11) {
    _debug("AudioRecord: could not write WAV header for file.\n");
    return false;
  }

  _debug("AudioRecord: creating WAV file.\n");
  return true;
}


void AudioRecord::closeWavFile()
{
  int bytes_per_sample = 1;
  if ( sndFormat_ == INT16 )
    bytes_per_sample = 2;


  SINT32 bytes = byteCounter_ * channels_ * bytes_per_sample;
  fseek(fp, 40, SEEK_SET); // jump to data length
  fwrite(&bytes, 4, 1, fp);

  bytes = byteCounter_ * channels_ * bytes_per_sample + 44; // + 44 for the wave header
  fseek(fp, 4, SEEK_SET); // jump to file size
  fwrite(&bytes, 4, 1, fp);
  fclose( fp );
}


void AudioRecord::recData(SFLDataFormat* buffer, int nSamples) {

  if (fp == 0){
    _debug("AudioRecord: Can't record data, a file has not yet been opened!\n");
    return;
  }
 
  // int size = nSamples * (sizeof(SFLDataFormat));
  // int size = sizeof(buffer);
  // int count = sizeof(buffer) / sizeof(SFLDataFormat);
  
  // printf("AudioRecord : sizeof(buffer) : %d \n",size); 
  // printf("AudioRecord : sizeof(buffer) / sizeof(SFLDataFormat) : %d \n",count);
  // printf("AudioRecord : nSamples : %d \n",nSamples);
  // printf("AudioRecord : buffer: %x : ", buffer);
 
  if ( sndFormat_ == INT16 ) { // TODO change INT16 to SINT16
    if ( fwrite(buffer, sizeof(SFLDataFormat), nSamples, fp) != nSamples)
      _debug("AudioRecord: Could not record data!\n");
    else {
      // printf("Buffer : %x \n",*buffer);
      fflush(fp);
      // _debug("Flushing!\n");
    }
  }

  


  byteCounter_ += (unsigned long)(nSamples*sizeof(SFLDataFormat));

  return;
}


void AudioRecord::recData(SFLDataFormat* buffer_1, SFLDataFormat* buffer_2, int nSamples_1, int nSamples_2) {

  if (fp == 0){
    _debug("AudioRecord: Can't record data, a file has not yet been opened!\n");
    return;
  }

  mixBuffer_ = new SFLDataFormat[nSamples_1]; 
 
  // int size = nSamples * (sizeof(SFLDataFormat));
  // int size = sizeof(buffer);
  // int count = sizeof(buffer) / sizeof(SFLDataFormat);
  
  // printf("AudioRecord : sizeof(buffer) : %d \n",size); 
  // printf("AudioRecord : sizeof(buffer) / sizeof(SFLDataFormat) : %d \n",count);
  // printf("AudioRecord : nSamples : %d \n",nSamples);
  // printf("AudioRecord : buffer: %x : ", buffer);
 
  if ( sndFormat_ == INT16 ) { // TODO change INT16 to SINT16
    for (int k=0; k<nSamples_1; k++){
      
      mixBuffer_[k] = (buffer_1[k]+buffer_2[k])/2;
      
      if ( fwrite(&buffer_1[k], 2, 1, fp) != 1)
        _debug("AudioRecord: Could not record data!\n");
      else {
        // printf("Buffer : %x \n",*buffer);
        fflush(fp);
        // _debug("Flushing!\n");
      }
    }
  }

  


  byteCounter_ += (unsigned long)(nSamples_1*sizeof(SFLDataFormat));

  delete [] mixBuffer_;

  return;
}
