/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
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


AudioRecord::AudioRecord() : fp (NULL)
        , channels_ (1)
        , byteCounter_ (0)
        , sndSmplRate_ (8000)
        , nbSamplesMic_ (0)
        , nbSamplesSpk_ (0)
        , nbSamplesMax_ (3000)
        , recordingEnabled_ (false)
        , mixBuffer_ (NULL)
        , micBuffer_ (NULL)
        , spkBuffer_ (NULL)
{

    mixBuffer_ = new SFLDataFormat[nbSamplesMax_];
    micBuffer_ = new SFLDataFormat[nbSamplesMax_];
    spkBuffer_ = new SFLDataFormat[nbSamplesMax_];

    createFilename();
}

AudioRecord::~AudioRecord()
{
    delete [] mixBuffer_;
    delete [] micBuffer_;
    delete [] spkBuffer_;
}


void AudioRecord::setSndSamplingRate (int smplRate)
{
    sndSmplRate_ = smplRate;
}

void AudioRecord::setRecordingOption (FILE_TYPE type, SOUND_FORMAT format, int sndSmplRate, std::string path)
{

    fileType_ = type;
    sndFormat_ = format;
    channels_ = 1;
    sndSmplRate_ = sndSmplRate;

    savePath_ = path + "/";

}



void AudioRecord::initFileName (std::string peerNumber)
{

    std::string fName;

    fName = fileName_;
    fName.append ("-"+peerNumber);

    if (fileType_ == FILE_RAW) {
        if (strstr (fileName_, ".raw") == NULL) {
            _debug ("AudioRecord: concatenate .raw file extension: name : %s", fileName_);
            fName.append (".raw");
        }
    } else if (fileType_ == FILE_WAV) {
        if (strstr (fileName_, ".wav") == NULL) {
            _debug ("AudioRecord: concatenate .wav file extension: name : %s", fileName_);
            fName.append (".wav");
        }
    }

    savePath_.append (fName);
}

void AudioRecord::openFile()
{

    bool result = false;

    _debug ("AudioRecord: Open file()");

    if (isFileExist()) {
        _debug ("AudioRecord: Filename does not exist, creating one");
        byteCounter_ = 0;

        if (fileType_ == FILE_RAW) {
            result = setRawFile();
        } else if (fileType_ == FILE_WAV) {
            result = setWavFile();
        }
    } else {
        _debug ("AudioRecord: Filename already exist opening it");

        if (fileType_ == FILE_RAW) {
            result = openExistingRawFile();
        } else if (fileType_ == FILE_WAV) {
            result = openExistingWavFile();
        }
    }
}


void AudioRecord::closeFile()
{

    if (fp == 0) return;

    if (fileType_ == FILE_RAW)
        fclose (fp);
    else if (fileType_ == FILE_WAV)
        this->closeWavFile();



}


bool AudioRecord::isOpenFile()
{

    if (fp) {
        return true;
    } else {
        return false;
    }
}


bool AudioRecord::isFileExist()
{
    _info ("AudioRecord: Try to open name : %s ", fileName_);

    if (fopen (fileName_,"rb") ==0) {
        return true;
    }

    return false;
}

bool AudioRecord::isRecording()
{

    if (recordingEnabled_)
        return true;
    else
        return false;
}


bool AudioRecord::setRecording()
{

    if (isOpenFile()) {
        if (!recordingEnabled_) {
            _info ("AudioRecording: Start recording");
            recordingEnabled_ = true;
        } else {
            recordingEnabled_ = false;
            _info ("AudioRecording: Stop recording");
        }
    } else {
        openFile();

        recordingEnabled_ = true; // once opend file, start recording
    }

    // WARNING: Unused return value
    return true;

}

void AudioRecord::stopRecording()
{
    _info ("AudioRecording: Stop recording");

    if (recordingEnabled_)
        recordingEnabled_ = false;
}


void AudioRecord::createFilename()
{

    time_t rawtime;

    struct tm * timeinfo;

    rawtime = time (NULL);
    timeinfo = localtime (&rawtime);

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
    strncpy (fileName_, out.str().c_str(), 8192);

    _info ("AudioRecord: create filename for this call %s ", fileName_);
}

bool AudioRecord::setRawFile()
{

    fp = fopen (savePath_.c_str(), "wb");

    if (!fp) {
        _warn ("AudioRecord::setRawFile() : could not create RAW file!");
        return false;
    }

    if (sndFormat_ != INT16) {   // TODO need to change INT16 to SINT16
        sndFormat_ = INT16;
        _debug ("AudioRecord::setRawFile() : using 16-bit signed integer data format for file.");
    }

    _debug ("AudioRecord:setRawFile() : created RAW file.");

    return true;
}


bool AudioRecord::setWavFile()
{
    _debug ("AudioRecord: Create wave file %s", savePath_.c_str());

    fp = fopen (savePath_.c_str(), "wb");

    if (!fp) {
        _warn ("AudioRecord: Error: could not create WAV file.");
        return false;
    }

    struct wavhdr hdr = {"RIF", 44, "WAV", "fmt", 16, 1, 1,
        sndSmplRate_, 0, 2, 16, "dat", 0
    };

    hdr.riff[3] = 'F';

    hdr.wave[3] = 'E';

    hdr.fmt[3]  = ' ';

    hdr.data[3] = 'a';

    hdr.num_chans = channels_;

    if (sndFormat_ == INT16) {   //  TODO need to write INT16 to SINT16
        hdr.bits_per_samp = 16;
    }

    hdr.bytes_per_samp = (SINT16) (channels_ * hdr.bits_per_samp / 8);

    hdr.bytes_per_sec = (SINT32) (hdr.sample_rate * hdr.bytes_per_samp);


    if (fwrite (&hdr, 4, 11, fp) != 11) {
        _warn ("AudioRecord: Error: could not write WAV header for file. ");
        return false;
    }

    _debug ("AudioRecord: created WAV file successfully.");

    return true;
}


bool AudioRecord::openExistingRawFile()
{
    fp = fopen (fileName_, "ab+");

    if (!fp) {
        _warn ("AudioRecord: could not create RAW file!");
        return false;
    }

    return true;
}


bool AudioRecord::openExistingWavFile()
{
    _info ("AudioRecord: Open existing wave file");

    fp = fopen (fileName_, "rb+");

    if (!fp) {
        _warn ("AudioRecord: Error: could not open WAV file!");
        return false;
    }

    printf ("AudioRecord::openExistingWavFile()::Tried to open %s ",fileName_);

    if (fseek (fp, 40, SEEK_SET) != 0) // jump to data length
        _warn ("AudioRecord: Error: Couldn't seek offset 40 in the file ");

    if (fread (&byteCounter_, 4, 1, fp))
        _warn ("AudioRecord: Error: bytecounter Read successfully ");

    if (fseek (fp, 0 , SEEK_END) != 0)
        _warn ("AudioRecord: Error: Couldn't seek at the en of the file ");


    if (fclose (fp) != 0)
        _warn ("AudioRecord: Error: Can't close file r+ ");



    fp = fopen (fileName_, "ab+");

    if (!fp) {
        _warn ("AudioRecord: Error: Could not createopen WAV file ab+!");
        return false;
    }

    if (fseek (fp, 4 , SEEK_END) != 0)
        _warn ("AudioRecord: Error: Couldn't seek at the en of the file ");

    return true;

}


void AudioRecord::closeWavFile()
{
    if (fp == 0) {
        _debug ("AudioRecord: Can't closeWavFile, a file has not yet been opened!");
        return;
    }

    _debug ("AudioRecord: Close wave file");


    SINT32 bytes = byteCounter_ * channels_;

    fseek (fp, 40, SEEK_SET); // jump to data length

    if (ferror (fp))
        _warn ("AudioRecord: Error: can't reach offset 40 while closing");

    fwrite (&bytes, sizeof (SINT32), 1, fp);

    if (ferror (fp))
        _warn ("AudioRecord: Error: can't write bytes for data length ");


    bytes = byteCounter_ * channels_ + 44; // + 44 for the wave header

    fseek (fp, 4, SEEK_SET); // jump to file size

    if (ferror (fp))
        _warn ("AudioRecord: Error: can't reach offset 4");

    fwrite (&bytes, 4, 1, fp);

    if (ferror (fp))
        _warn ("AudioRecord: Error: can't reach offset 4");


    if (fclose (fp) != 0)
        _warn ("AudioRecord: Error: can't close file");


}

void AudioRecord::recSpkrData (SFLDataFormat* buffer, int nSamples)
{

    if (recordingEnabled_) {

        nbSamplesMic_ = nSamples;

        for (int i = 0; i < nbSamplesMic_; i++)
            micBuffer_[i] = buffer[i];
    }

    return;
}


void AudioRecord::recMicData (SFLDataFormat* buffer, int nSamples)
{

    if (recordingEnabled_) {

        nbSamplesSpk_ = nSamples;

        for (int i = 0; i < nbSamplesSpk_; i++)
            spkBuffer_[i] = buffer[i];

    }

    return;
}


void AudioRecord::recData (SFLDataFormat* buffer, int nSamples)
{

    if (recordingEnabled_) {

        if (fp == 0) {
            _debug ("AudioRecord: Can't record data, a file has not yet been opened!");
            return;
        }



        if (sndFormat_ == INT16) {   // TODO change INT16 to SINT16
            if (fwrite (buffer, sizeof (SFLDataFormat), nSamples, fp) != (unsigned int) nSamples)
                _warn ("AudioRecord: Could not record data! ");
            else {
                fflush (fp);
                byteCounter_ += (unsigned long) (nSamples*sizeof (SFLDataFormat));
            }
        }
    }

    return;
}


void AudioRecord::recData (SFLDataFormat* buffer_1, SFLDataFormat* buffer_2, int nSamples_1, int nSamples_2 UNUSED)
{

    if (recordingEnabled_) {

        _debug ("Recording enabled");

        if (fp == 0) {
            _debug ("AudioRecord: Can't record data, a file has not yet been opened!");
            return;
        }


        if (sndFormat_ == INT16) {   // TODO change INT16 to SINT16
            for (int k=0; k<nSamples_1; k++) {

                mixBuffer_[k] = (buffer_1[k]+buffer_2[k]);


                if (fwrite (&mixBuffer_[k], 2, 1, fp) != 1)
                    _warn ("AudioRecord: Could not record data!");
                else {
                    fflush (fp);
                }
            }
        }

        byteCounter_ += (unsigned long) (nSamples_1*sizeof (SFLDataFormat));

    }

    return;
}

