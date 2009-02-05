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
  recordingEnabled_ = false;
  fp = 0;

}


void AudioRecord::setSndSamplingRate(int smplRate){
  sndSmplRate_ = smplRate;  
}

void AudioRecord::setRecordingOption(std::string name, FILE_TYPE type, SOUND_FORMAT format, int sndSmplRate){

  strncpy(fileName_, name.c_str(), 8192);
 
  fileType_ = type;
  sndFormat_ = format;
  channels_ = 1;
  sndSmplRate_ = sndSmplRate;
  
  if (fileType_ == FILE_RAW){
     if ( strstr(fileName_, ".raw") == NULL){
       printf("AudioRecord::openFile::concatenate .raw file extension: name : %s \n", fileName_); 
       strcat(fileName_, ".raw");
     }
   }
   else if (fileType_ == FILE_WAV){
     if ( strstr(fileName_, ".wav") == NULL){ 
       printf("AudioRecord::openFile::concatenate .wav file extension: name : %s \n", fileName_);
       strcat(fileName_, ".wav");
     }
   }
}

void AudioRecord::openFile(){
  
   _debug("AudioRecord::openFile()\n");  
  
   bool result = false;
   
   if(isFileExist()) {
     _debug("AudioRecord::Filename does not exist, creating one \n");
     byteCounter_ = 0;

     if(fileType_ == FILE_RAW){
       result = setRawFile();
     }
     else if (fileType_ == FILE_WAV){
       result = setWavFile();
     }
   }
   else {
     _debug("AudioRecord::Filename already exist opening it \n");
     if(fileType_ == FILE_RAW){
       result = openExistingRawFile();
     }   
     else if (fileType_ == FILE_WAV){
       result = openExistingWavFile();      
     }
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
  
  if(fp){
    _debug("AudioRecord::isOpenFile(): file already openend\n");
    return true;
  }
  else {
    _debug("AudioRecord::isOpenFIle(): file not openend \n");
    return false;
  }
}


bool AudioRecord::isFileExist() {
  
  printf("AudioRecord::isFileExist(): try to open name : %s \n", fileName_);
  if(fopen(fileName_,"rb")==0) {
    return true;
  }
  
  return false;  
}

bool AudioRecord::isRecording() {
  _debug("AudioRecording::setRecording() \n");
  
  if(recordingEnabled_)
    return true;
  else 
    return false;
}


bool AudioRecord::setRecording() {
  _debug("AudioRecord::setRecording()\n");
  
  if (isOpenFile()){
    _debug("AuioRecord::setRecording()::file already opened\n");
    if(!recordingEnabled_)
      recordingEnabled_ = true;
    else 
      recordingEnabled_ = false;
  }
  else {
    _debug("AudioRecord::setRecording():Opening the wave file in call during call instantiation\n");
    openFile();

    recordingEnabled_ = true; // once opend file, start recording
  }
  
}

void AudioRecord::stopRecording() {
  _debug("AudioRecording::stopRecording() \n");

  if(recordingEnabled_)
    recordingEnabled_ = false;
}


bool AudioRecord::setRawFile() {

  fp = fopen(fileName_, "wb");
  if ( !fp ) {
    _debug("AudioRecord::setRawFile() : could not create RAW file!\n");
    return false;
  }

  if ( sndFormat_ != INT16 ) { // TODO need to change INT16 to SINT16
    sndFormat_ = INT16;
    _debug("AudioRecord::setRawFile() : using 16-bit signed integer data format for file.\n");
  }

  _debug("AudioRecord:setRawFile() : created RAW file.\n");
  return true;
}


bool AudioRecord::setWavFile() {
  
  fp = fopen(fileName_, "wb");
  if ( !fp ) {
    _debug("AudioRecord::setWavFile() : could not create WAV file.\n");
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
    _debug("AudioRecord::setWavFile() : could not write WAV header for file. \n");
    return false;
  }

  _debug("AudioRecord::setWavFile() : created WAV file. \n");
  return true;
}


bool AudioRecord::openExistingRawFile()
{ 
  fp = fopen(fileName_, "ab+");
  if ( !fp ) {
    _debug("AudioRecord::openExistingRawFile() : could not create RAW file!\n");
    return false;
  }
}


bool AudioRecord::openExistingWavFile()
{ 
  _debug("AudioRecord::openExistingWavFile() \n");

  fp = fopen(fileName_, "rb+");
  if ( !fp ) {
    _debug("AudioRecord::openExistingWavFile() : could not open WAV file rb+!\n");
    return false;
  }

  printf("AudioRecord::openExistingWavFile()::Tried to open %s \n",fileName_);
  
  if(fseek(fp, 40, SEEK_SET) != 0) // jump to data length
    _debug("AudioRecord::OpenExistingWavFile: 1.Couldn't seek offset 40 in the file \n");
  
  if(fread(&byteCounter_, 4, 1, fp))
    _debug("AudioRecord::OpenExistingWavFile : bytecounter Read successfully \n");
  
  if(fseek (fp, 0 , SEEK_END) != 0)
    _debug("AudioRecors::OpenExistingWavFile : 2.Couldn't seek at the en of the file \n");

  printf("AudioRecord::OpenExistingWavFile : Byte counter after oppening : %d \n",(int)byteCounter_);

  if ( fclose( fp ) != 0)
    _debug("AudioRecord::openExistingWavFile()::ERROR: can't close file r+ \n");


  
  fp = fopen(fileName_, "ab+");
  if ( !fp ) {
    _debug("AudioRecord::openExistingWavFile() : could not createopen WAV file ab+!\n");
    return false;
  }

  if(fseek (fp, 4 , SEEK_END) != 0)
    _debug("AudioRecors::OpenExistingWavFile : 2.Couldn't seek at the en of the file \n");
  
}


void AudioRecord::closeWavFile() 
{
  if (fp == 0){
    _debug("AudioRecord:: Can't closeWavFile, a file has not yet been opened!\n");
    return;
  }
 
  _debug("AudioRecord::closeWavFile() \n");

  if ( fclose( fp ) != 0)
    _debug("AudioRecord::closeWavFile()::ERROR: can't close file ab \n");

  

  fp = fopen(fileName_, "rb+");
  if ( !fp ) {
    _debug("AudioRecord::closeWavFile() : could not open WAV file rb+!\n");
    return;
  }


  SINT32 bytes = byteCounter_ * channels_;
  fseek(fp, 40, SEEK_SET); // jump to data length
  if (ferror(fp))perror("AudioRecord::closeWavFile()::ERROR: can't reach offset 40\n");
  
  fwrite(&bytes, sizeof(SINT32), 1, fp);
  if (ferror(fp))perror("AudioRecord::closeWavFile()::ERROR: can't write bytes for data length \n");
  printf("AudioRecord::closeWavFile : data bytes: %i \n",(int)bytes);

  bytes = byteCounter_ * channels_ + 44; // + 44 for the wave header 
  fseek(fp, 4, SEEK_SET);  // jump to file size
  if (ferror(fp))perror("AudioRecord::closeWavFile()::ERROR: can't reach offset 4\n");
  
  fwrite(&bytes, 4, 1, fp);
  if (ferror(fp))perror("AudioRecord::closeWavFile()::ERROR: can't reach offset 4\n");
  
  printf("AudioRecord::closeWavFile : bytes : %i \n",(int)bytes);
  
  if ( fclose( fp ) != 0)
    _debug("AudioRecord::closeWavFile()::ERROR: can't close file\n");
 
  // i = fclose(fp);
  // printf("AudioRecord::closeWavFile : indicator i : %i \n",i);

}


void AudioRecord::recData(SFLDataFormat* buffer, int nSamples) {

  if (recordingEnabled_) {

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
        _debug("AudioRecord: Could not record data! \n");
      else {
        // printf("Buffer : %x \n",*buffer);
        fflush(fp);
        // _debug("Flushing!\n");
        byteCounter_ += (unsigned long)(nSamples*sizeof(SFLDataFormat));
      }
    } 
  }

  return;
}


void AudioRecord::recData(SFLDataFormat* buffer_1, SFLDataFormat* buffer_2, int nSamples_1, int nSamples_2) {

  if (recordingEnabled_) {

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

    printf("AudioRecord::recData():: byteCounter_ : %i \n",(int)byteCounter_ );

    delete [] mixBuffer_;
  }

  return;
}
