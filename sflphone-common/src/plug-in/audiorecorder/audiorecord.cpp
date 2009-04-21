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

// structure for the wave header
struct wavhdr {
  char riff[4];           // "RIFF"
  SINT32 file_size;       // in bytes
  char wave[4];           // "WAVE"
  char fmt[4];            // "fmt "
  SINT32 chunk_size;      // in bytes (16 for PCM)
  SINT16 format_tag;      // 1=PCM, 2=ADPCM, 3=IEEE float, 6=A-Law, 7=Mu-Law
  SINT16 num_chans;       // 1=mono, 2=stereo
  SINT32 sample_rate;
  SINT32 bytes_per_sec;
  SINT16 bytes_per_samp;  // 2=16-bit mono, 4=16-bit stereo
  SINT16 bits_per_samp;
  char data[4];           // "data"
  SINT32 data_length;     // in bytes
};


AudioRecord::AudioRecord(){
  
  sndSmplRate_ = 44100;
  channels_ = 1;
  byteCounter_ = 0;
  recordingEnabled_ = false;
  fp = 0;
  nbSamplesMax_ = 3000;

  createFilename();

  mixBuffer_ = new SFLDataFormat[nbSamplesMax_];
  micBuffer_ = new SFLDataFormat[nbSamplesMax_];
  spkBuffer_ = new SFLDataFormat[nbSamplesMax_];
}

AudioRecord::~AudioRecord() {
    delete [] mixBuffer_;
    delete [] micBuffer_;
    delete [] spkBuffer_;
}


void AudioRecord::setSndSamplingRate(int smplRate){
  sndSmplRate_ = smplRate;  
}

void AudioRecord::setRecordingOption(FILE_TYPE type, SOUND_FORMAT format, int sndSmplRate, std::string path, std::string id){
 
 
  fileType_ = type;
  sndFormat_ = format;
  channels_ = 1;
  sndSmplRate_ = sndSmplRate;
  call_id_ = id;

  savePath_ = path + "/";
   
}



void AudioRecord::initFileName( std::string peerNumber){

   std::string fName;
   
   fName = fileName_;
   fName.append("-"+peerNumber);

    if (fileType_ == FILE_RAW){
     if ( strstr(fileName_, ".raw") == NULL){
       printf("AudioRecord::openFile::concatenate .raw file extension: name : %s \n", fileName_); 
       fName.append(".raw");
     }
   }
   else if (fileType_ == FILE_WAV){
     if ( strstr(fileName_, ".wav") == NULL){ 
       printf("AudioRecord::openFile::concatenate .wav file extension: name : %s \n", fileName_);
       fName.append(".wav");
     }
   }
   
   savePath_.append(fName);
}

void AudioRecord::openFile(){
  
   
   _debug("AudioRecord::openFile()\n");  
  
   bool result = false;
   
   _debug("AudioRecord::openFile()\n");
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
  _debug("AudioRecord::isFileExist(): try to open name : %s \n", fileName_);
  
  if(fopen(fileName_,"rb")==0) {
    return true;
  }
  
  return false;  
}

bool AudioRecord::isRecording() {
  _debug("AudioRecording::isRecording() %i \n", recordingEnabled_);
  
  
  if(recordingEnabled_)
    return true;
  else 
    return false;
}


bool AudioRecord::setRecording() {
  _debug("AudioRecord::setRecording() \n");
  
  if (isOpenFile()){
    _debug("AuioRecord::setRecording()::file already opened \n");
    if(!recordingEnabled_)
      recordingEnabled_ = true;
    else 
      recordingEnabled_ = false;
  }
  else {
    _debug("AudioRecord::setRecording():Opening the wave file in call during call instantiation \n");
    openFile();

    recordingEnabled_ = true; // once opend file, start recording
  }
  
}

void AudioRecord::stopRecording() {
  _debug("AudioRecording::stopRecording() \n");

  if(recordingEnabled_)
    recordingEnabled_ = false;
}


void AudioRecord::createFilename(){
    
    time_t rawtime;
    struct tm * timeinfo;

    rawtime = time(NULL);
    timeinfo = localtime ( &rawtime );

    stringstream out;
    
    // DATE
    out << timeinfo->tm_year+1900;
    if (timeinfo->tm_mon < 9) // january is 01, not 1
      out << 0;
    out << timeinfo->tm_mon+1;
    if (timeinfo->tm_mday < 10) // 01 02 03, not 1 2 3
      out << 0;
    out << timeinfo->tm_mday;
 
    out << '-';
   
    // hour
    if (timeinfo->tm_hour < 10) // 01 02 03, not 1 2 3
      out << 0;
    out << timeinfo->tm_hour;
    out << ':';
    if (timeinfo->tm_min < 10) // 01 02 03, not 1 2 3
      out << 0;
    out << timeinfo->tm_min;
    out << ':';
    if (timeinfo->tm_sec < 10) // 01 02 03,  not 1 2 3
      out << 0;
    out << timeinfo->tm_sec;

    // fileName_ = out.str();
    strncpy(fileName_, out.str().c_str(), 8192);

    printf("AudioRecord::createFilename::filename for this call %s \n",fileName_);
}

bool AudioRecord::setRawFile() {

  fp = fopen(savePath_.c_str(), "wb");
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
  
  fp = fopen(savePath_.c_str(), "wb");
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
 

}

void AudioRecord::recSpkrData(SFLDataFormat* buffer, int nSamples) {

  if (recordingEnabled_) {
   
    nbSamplesMic_ = nSamples;

    for(int i = 0; i < nbSamplesMic_; i++)
      micBuffer_[i] = buffer[i];
  }

  return;
}


void AudioRecord::recMicData(SFLDataFormat* buffer, int nSamples) {

  if (recordingEnabled_) {

    nbSamplesSpk_ = nSamples;

    for(int i = 0; i < nbSamplesSpk_; i++)
      spkBuffer_[i] = buffer[i];

  }

  return;
}


void AudioRecord::recData(SFLDataFormat* buffer, int nSamples) {

  if (recordingEnabled_) {

    if (fp == 0){
      _debug("AudioRecord: Can't record data, a file has not yet been opened!\n");
      return;
    }
 
   
 
    if ( sndFormat_ == INT16 ) { // TODO change INT16 to SINT16
      if ( fwrite(buffer, sizeof(SFLDataFormat), nSamples, fp) != nSamples)
        _debug("AudioRecord: Could not record data! \n");
      else {
        fflush(fp);
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


    if ( sndFormat_ == INT16 ) { // TODO change INT16 to SINT16
      for (int k=0; k<nSamples_1; k++){
      
        mixBuffer_[k] = (buffer_1[k]+buffer_2[k]);
        
      
        if ( fwrite(&mixBuffer_[k], 2, 1, fp) != 1)
          _debug("AudioRecord: Could not record data!\n");
        else {
          fflush(fp);
        }
      }
    }
   
    byteCounter_ += (unsigned long)(nSamples_1*sizeof(SFLDataFormat));

  }

  return;
}

