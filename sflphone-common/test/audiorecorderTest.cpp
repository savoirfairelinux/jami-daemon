/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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

#include <stdio.h>
#include <sstream>

#include "audiorecorderTest.h"

using std::cout;
using std::endl;

void AudioRecorderTest::setUp(){
    // Instanciate the object
    _ar = new AudioRecord();
}

void AudioRecorderTest::testRecordData(){
  
/*
  FILE_TYPE ft = FILE_WAV;
  SOUND_FORMAT sf = INT16;
  _ar->setSndSamplingRate(44100);
  _ar->openFile("theWavFile.wav",ft,sf);

  cout << "file opened!\n";

  SFLDataFormat buf [2];
  for (SFLDataFormat i = -32768; i < 32767; i++ ){
    buf[0] = i;
    buf[1] = i;
    _ar->recData(buf,2);
  }

  _ar->closeFile();
*/
}

void AudioRecorderTest::tearDown(){
    // Delete the audio recorder module
    delete _ar; _ar = NULL;
}
