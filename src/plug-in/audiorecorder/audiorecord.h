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


#include <iostream>
#include <string.h>
#include <sndfile.h>
#include <stdlib.h>
#include <sstream>

#include "global.h"
#include "plug-in/plugin.h"
using namespace std;


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

typedef std::string CallID;

class AudioRecord
{

public:

  AudioRecord();

  void setSndSamplingRate(int smplRate);

  void setRecordingOption(FILE_TYPE type, SOUND_FORMAT format, int sndSmplRate);

  /** 
   * Check if no otehr file is opened, then create a new one
   * @param fileName A string containing teh file (with/without extension)
   * @param type     The sound file format (FILE_RAW, FILE_WAVE)
   * @param format   Internal sound format (INT16 / INT32)
   */
  void openFile();

  /**
   * Close the opend recording file. If wave: cout the number of byte
   */
  void closeFile();

  /**
   * Check if a file is already opened
   */
  bool isOpenFile();

  /** 
   * Check if a file already exist
   */
  bool isFileExist();

  /**
   * Check recording state 
   */ 
  bool isRecording();

  /**
   * Set recording flag
   */
  bool setRecording();

  /**
   * Stop recording flag
   */
  void stopRecording();

  /**
   * Record a chunk of data in an openend file
   * @param buffer  The data chunk to be recorded
   * @param nSamples Number of samples (number of bytes) to be recorded 
   */
  void recData(SFLDataFormat* buffer, int nSamples);

  /**
   * Record a chunk of data in an openend file, Mix two differnet buffer
   * @param buffer_1  The first data chunk to be recorded
   * @param buffer_2  The second data chunk to be recorded
   * @param nSamples_1 Number of samples (number of bytes) of buffer_1
   * @param nSamples_2 Number of samples (number of bytes) of buffer_2
   */
  void recData(SFLDataFormat* buffer_1, SFLDataFormat* buffer_2, int nSamples_1, int nSamples_2);

protected:

  /**
   * Create name file according to current date
   */
  void createFilename();
  
  /**
   * Set the header for raw files
   */
  bool setRawFile();

  /**
   * Set the header for wave files
   */
  bool setWavFile();

  /**
   * Open an existing raw file, used when the call is set on hold    
   */
  bool openExistingRawFile();

  /**
   * Open an existing wav file, used when the call is set on hold
   */
  bool openExistingWavFile();

  /**
   * Compute the number of byte recorded and close the file
   */
  void closeWavFile();

  /**
   * Given two buffers, return one mixed audio buffer
   */
  void mixBuffers(SFLDataFormat* buffer_1, SFLDataFormat* buffer_2, int nSamples_1, int nSamples_2);

  /**
   * Pointer to the recorded file
   */
  FILE *fp;                      //file pointer

  /**
   * File format (RAW / WAVE)
   */
  FILE_TYPE fileType_;

  /**
   * Sound format (SINT16/SINT32)
   */
  SOUND_FORMAT sndFormat_;

  /**
   * Number of channels
   */
  int channels_;

  /**
   * Number of byte recorded
   */
  unsigned long byteCounter_;

  /**
   * Sampling rate
   */
  int sndSmplRate_;

  /**
   * Recording flage
   */
  bool recordingEnabled_;

  /**
   * Buffer used for mixing two channels
   */
  SFLDataFormat* mixBuffer_;
  
  /**
   * Filename for this recording
   */
  char fileName_[8192];

  /**
   * Path for this recording
   */
  std::string savePath_;


};
