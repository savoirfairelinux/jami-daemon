/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include <unistd.h>
#include <sstream> // for stringstream

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

AudioRecord::AudioRecord() : fileHandle_(NULL)
    , fileType_(FILE_INVALID)
    , channels_(1)
    , byteCounter_(0)
    , sndSmplRate_(8000)
    , nbSamplesMic_(0)
    , nbSamplesSpk_(0)
    , nbSamplesMax_(3000)
    , recordingEnabled_(false)
    , mixBuffer_(new SFLDataFormat[nbSamplesMax_])
    , micBuffer_(new SFLDataFormat[nbSamplesMax_])
    , spkBuffer_(new SFLDataFormat[nbSamplesMax_])
    , filename_()
    , savePath_()
{
    createFilename();
}

AudioRecord::~AudioRecord()
{
    delete [] mixBuffer_;
    delete [] micBuffer_;
    delete [] spkBuffer_;
}

void AudioRecord::setSndSamplingRate(int smplRate)
{
    sndSmplRate_ = smplRate;
}

int AudioRecord::getSndSamplingRate() const
{
    return sndSmplRate_;
}

void AudioRecord::setRecordingOption(FILE_TYPE type, int sndSmplRate, const std::string &path)
{
    fileType_ = type;
    channels_ = 1;
    sndSmplRate_ = sndSmplRate;
    savePath_ = path + "/";
}

void AudioRecord::initFilename(const std::string &peerNumber)
{
    std::string fName(filename_);
    fName.append("-" + peerNumber);

    if (fileType_ == FILE_RAW) {
        if (filename_.find(".raw") == std::string::npos) {
            DEBUG("AudioRecord: concatenate .raw file extension: name : %s", filename_.c_str());
            fName.append(".raw");
        }
    } else if (fileType_ == FILE_WAV) {
        if (filename_.find(".wav") == std::string::npos) {
            DEBUG("AudioRecord: concatenate .wav file extension: name : %s", filename_.c_str());
            fName.append(".wav");
        }
    }

    savePath_.append(fName);
}

std::string AudioRecord::getFilename() const
{
    return savePath_;
}

bool AudioRecord::openFile()
{
    bool result = false;

    DEBUG("AudioRecord: Open file()");

    if (not fileExists()) {
        DEBUG("AudioRecord: Filename does not exist, creating one");
        byteCounter_ = 0;

        if (fileType_ == FILE_RAW)
            result = setRawFile();
        else if (fileType_ == FILE_WAV)
            result = setWavFile();
    } else {
        DEBUG("AudioRecord: Filename already exists, opening it");
        if (fileType_ == FILE_RAW)
            result = openExistingRawFile();
        else if (fileType_ == FILE_WAV)
            result = openExistingWavFile();
    }

    return result;
}

void AudioRecord::closeFile()
{
    if (fileHandle_ == 0) return;

    if (fileType_ == FILE_RAW)
        fclose(fileHandle_);
    else if (fileType_ == FILE_WAV)
        closeWavFile();
}

bool AudioRecord::isOpenFile() const
{
    return fileHandle_ != 0;
}

bool AudioRecord::fileExists() const
{
    return access(savePath_.c_str(), F_OK) != -1;
}

bool AudioRecord::isRecording() const
{
    return recordingEnabled_;
}

bool AudioRecord::setRecording()
{
    if (isOpenFile()) {
        if (!recordingEnabled_) {
            DEBUG("AudioRecording: Start recording");
            recordingEnabled_ = true;
        } else {
            DEBUG("AudioRecording: Stop recording");
            recordingEnabled_ = false;
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
    DEBUG("AudioRecording: Stop recording");
    recordingEnabled_ = false;
}

void AudioRecord::createFilename()
{
    time_t rawtime;

    struct tm * timeinfo;

    rawtime = time(NULL);
    timeinfo = localtime(&rawtime);

    std::stringstream out;

    // DATE
    out << timeinfo->tm_year + 1900;

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
    filename_ = out.str();

    DEBUG("AudioRecord: create filename for this call %s ", filename_.c_str());
}

bool AudioRecord::setRawFile()
{
    fileHandle_ = fopen(savePath_.c_str(), "wb");

    if (!fileHandle_) {
        WARN("AudioRecord: Could not create RAW file!");
        return false;
    }

    DEBUG("AudioRecord:setRawFile() : created RAW file.");

    return true;
}

bool AudioRecord::setWavFile()
{
    DEBUG("AudioRecord: Create new wave file %s, sampling rate: %d", savePath_.c_str(), sndSmplRate_);

    fileHandle_ = fopen(savePath_.c_str(), "wb");

    if (!fileHandle_) {
        WARN("AudioRecord: Error: could not create WAV file.");
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

    hdr.bits_per_samp = 16;

    hdr.bytes_per_samp = (SINT16)(channels_ * hdr.bits_per_samp / 8);

    hdr.bytes_per_sec = (SINT32)(hdr.sample_rate * hdr.bytes_per_samp);


    if (fwrite(&hdr, 4, 11, fileHandle_) != 11) {
        WARN("AudioRecord: Error: could not write WAV header for file. ");
        return false;
    }

    DEBUG("AudioRecord: created WAV file successfully.");

    return true;
}

bool AudioRecord::openExistingRawFile()
{
    fileHandle_ = fopen(filename_.c_str(), "ab+");

    if (!fileHandle_) {
        WARN("AudioRecord: could not create RAW file!");
        return false;
    }

    return true;
}

bool AudioRecord::openExistingWavFile()
{
    DEBUG("%s(%s)\n", __PRETTY_FUNCTION__, filename_.c_str());

    fileHandle_ = fopen(filename_.c_str(), "rb+");

    if (!fileHandle_) {
        WARN("AudioRecord: Error: could not open WAV file!");
        return false;
    }

    if (fseek(fileHandle_, 40, SEEK_SET) != 0)  // jump to data length
        WARN("AudioRecord: Error: Couldn't seek offset 40 in the file ");

    if (fread(&byteCounter_, 4, 1, fileHandle_))
        WARN("AudioRecord: Error: bytecounter Read successfully ");

    if (fseek(fileHandle_, 0 , SEEK_END) != 0)
        WARN("AudioRecord: Error: Couldn't seek at the en of the file ");


    if (fclose(fileHandle_) != 0)
        WARN("AudioRecord: Error: Can't close file r+ ");

    fileHandle_ = fopen(filename_.c_str(), "ab+");

    if (!fileHandle_) {
        WARN("AudioRecord: Error: Could not createopen WAV file ab+!");
        return false;
    }

    if (fseek(fileHandle_, 4 , SEEK_END) != 0)
        WARN("AudioRecord: Error: Couldn't seek at the en of the file ");

    return true;

}

void AudioRecord::closeWavFile()
{
    if (fileHandle_ == 0) {
        DEBUG("AudioRecord: Can't closeWavFile, a file has not yet been opened!");
        return;
    }

    DEBUG("AudioRecord: Close wave file");

    SINT32 bytes = byteCounter_ * channels_;

    fseek(fileHandle_, 40, SEEK_SET);  // jump to data length

    if (ferror(fileHandle_))
        WARN("AudioRecord: Error: can't reach offset 40 while closing");

    fwrite(&bytes, sizeof(SINT32), 1, fileHandle_);

    if (ferror(fileHandle_))
        WARN("AudioRecord: Error: can't write bytes for data length ");

    bytes = byteCounter_ * channels_ + 44; // + 44 for the wave header

    fseek(fileHandle_, 4, SEEK_SET);  // jump to file size

    if (ferror(fileHandle_))
        WARN("AudioRecord: Error: can't reach offset 4");

    fwrite(&bytes, 4, 1, fileHandle_);

    if (ferror(fileHandle_))
        WARN("AudioRecord: Error: can't reach offset 4");

    if (fclose(fileHandle_) != 0)
        WARN("AudioRecord: Error: can't close file");
}

void AudioRecord::recSpkrData(SFLDataFormat* buffer, int nSamples)
{
    if (recordingEnabled_) {
        nbSamplesMic_ = nSamples;

        for (int i = 0; i < nbSamplesMic_; i++)
            micBuffer_[i] = buffer[i];
    }
}

void AudioRecord::recMicData(SFLDataFormat* buffer, int nSamples)
{
    if (recordingEnabled_) {
        nbSamplesSpk_ = nSamples;

        for (int i = 0; i < nbSamplesSpk_; i++)
            spkBuffer_[i] = buffer[i];

    }
}

void AudioRecord::recData(SFLDataFormat* buffer, int nSamples)
{
    if (recordingEnabled_) {
        if (fileHandle_ == 0) {
            DEBUG("AudioRecord: Can't record data, a file has not yet been opened!");
            return;
        }

        if (fwrite(buffer, sizeof(SFLDataFormat), nSamples, fileHandle_) != (unsigned int) nSamples)
            WARN("AudioRecord: Could not record data! ");
        else {
            fflush(fileHandle_);
            byteCounter_ += (unsigned long)(nSamples*sizeof(SFLDataFormat));
        }
    }
}

void AudioRecord::recData(SFLDataFormat* buffer_1, SFLDataFormat* buffer_2,
                          int nSamples_1, int /*nSamples_2*/)
{
    if (recordingEnabled_) {
        if (fileHandle_ == 0) {
            DEBUG("AudioRecord: Can't record data, a file has not yet been opened!");
            return;
        }

        for (int k = 0; k < nSamples_1; k++) {
            mixBuffer_[k] = (buffer_1[k]+buffer_2[k]);

            if (fwrite(&mixBuffer_[k], 2, 1, fileHandle_) != 1)
                WARN("AudioRecord: Could not record data!");
            else
                fflush(fileHandle_);
        }

        byteCounter_ += (unsigned long)(nSamples_1 * sizeof(SFLDataFormat));
    }
}

