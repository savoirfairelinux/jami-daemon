/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "audiorecord.h"
#include <unistd.h>
#include <sstream> // for stringstream
#include <algorithm>
#include <cstdio>
#include "logger.h"
#include "fileutils.h"

// structure for the wave header

struct wavhdr {
    char riff[4];           // "RIFF"
    int32_t file_size;       // in bytes
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    int32_t chunk_size;      // in bytes (16 for PCM)
    int16_t format_tag;      // 1=PCM, 2=ADPCM, 3=IEEE float, 6=A-Law, 7=Mu-Law
    int16_t num_chans;       // 1=mono, 2=stereo
    int32_t sample_rate;
    int32_t bytes_per_sec;
    int16_t bytes_per_samp;  // 2=16-bit mono, 4=16-bit stereo
    int16_t bits_per_samp;
    char data[4];           // "data"
    int32_t data_length;     // in bytes
};

namespace {
std::string
createFilename()
{
    time_t rawtime = time(NULL);
    struct tm * timeinfo = localtime(&rawtime);

    std::stringstream out;

    // DATE
    out << timeinfo->tm_year + 1900;

    if (timeinfo->tm_mon < 9) // january is 01, not 1
        out << 0;

    out << timeinfo->tm_mon + 1;

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
    return out.str();
}
}


AudioRecord::AudioRecord() : fileHandle_(NULL)
    , fileType_(FILE_INVALID)
    , channels_(1)
    , byteCounter_(0)
    , sndSmplRate_(8000)
    , nbSamplesMic_(0)
    , nbSamplesSpk_(0)
    , recordingEnabled_(false)
    , mixBuffer_()
    , micBuffer_()
    , spkBuffer_()
    , filename_(createFilename())
    , savePath_()
{
    WARN("Generate filename for this call %s ", filename_.c_str());
}

void AudioRecord::setSndSamplingRate(int smplRate)
{
    sndSmplRate_ = smplRate;
}

void AudioRecord::setRecordingOption(FILE_TYPE type, int sndSmplRate, const std::string &path)
{
    std::string filePath;

    // use HOME directory if path is empty, or if path does not exist
    if (path.empty() or not fileutils::check_dir(path.c_str())) {
        filePath = fileutils::get_home_dir();
    } else {
        filePath = path;
    }

    fileType_ = type;
    channels_ = 1;
    sndSmplRate_ = sndSmplRate;
    savePath_ = (*filePath.rbegin() == DIR_SEPARATOR_CH) ? filePath : filePath + DIR_SEPARATOR_STR;
}

namespace {
bool
nonFilenameCharacter(char c)
{
    return not(std::isalnum(c) or c == '_' or c == '.');
}

// Replace any character that is inappropriate for a filename with '_'
std::string
sanitize(std::string s)
{
    std::replace_if(s.begin(), s.end(), nonFilenameCharacter, '_');
    return s;
}
}

void AudioRecord::initFilename(const std::string &peerNumber)
{
    std::string fName(filename_);
    fName.append("-" + sanitize(peerNumber) + "-" PACKAGE);

    if (fileType_ == FILE_RAW) {
        if (filename_.find(".raw") == std::string::npos) {
            DEBUG("Concatenate .raw file extension: name : %s", filename_.c_str());
            fName.append(".raw");
        }
    } else if (fileType_ == FILE_WAV) {
        if (filename_.find(".wav") == std::string::npos) {
            DEBUG("Concatenate .wav file extension: name : %s", filename_.c_str());
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

    if (not fileExists()) {
        DEBUG("Filename does not exist, creating one");
        byteCounter_ = 0;

        if (fileType_ == FILE_RAW)
            result = setRawFile();
        else if (fileType_ == FILE_WAV)
            result = setWavFile();
    } else {
        DEBUG("Filename already exists, opening it");

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

bool AudioRecord::toggleRecording()
{
    if (isOpenFile()) {
        recordingEnabled_ = !recordingEnabled_;
    } else {
        openFile();
        recordingEnabled_ = true;
    }

    return recordingEnabled_;
}

void AudioRecord::stopRecording()
{
    DEBUG("Stop recording");
    recordingEnabled_ = false;
}

bool AudioRecord::setRawFile()
{
    fileHandle_ = fopen(savePath_.c_str(), "wb");

    if (!fileHandle_) {
        WARN("Could not create RAW file!");
        return false;
    }

    DEBUG("created RAW file.");

    return true;
}

namespace {
std::string header_to_string(const wavhdr &hdr)
{
    std::stringstream ss;
    ss << hdr.riff << "\0 "
       << hdr.file_size << " "
       << hdr.wave << "\0 "
       << hdr.fmt << "\0 "
       << hdr.chunk_size << " "
       << hdr.format_tag << " "
       << hdr.num_chans << " "
       << hdr.sample_rate << " "
       << hdr.bytes_per_sec << " "
       << hdr.bytes_per_samp << " "
       << hdr.bits_per_samp << " "
       << hdr.data << "\0 "
       << hdr.data_length;
    return ss.str();
}
}

bool AudioRecord::setWavFile()
{
    DEBUG("Create new wave file %s, sampling rate: %d", savePath_.c_str(), sndSmplRate_);

    fileHandle_ = fopen(savePath_.c_str(), "wb");

    if (!fileHandle_) {
        WARN("Could not create WAV file.");
        return false;
    }

    /* The text fields are NOT supposed to be null terminated, so we have to
     * write them as arrays since strings enclosed in quotes include a
     * null character */
    wavhdr hdr = {{'R', 'I', 'F', 'F'},
        44,
        {'W', 'A', 'V', 'E'},
        {'f','m', 't', ' '},
        16,
        1,
        channels_,
        sndSmplRate_,
        -1, /* initialized below */
        -1, /* initialized below */
        16,
        {'d', 'a', 't', 'a'},
        0
    };

    hdr.bytes_per_samp = channels_ * hdr.bits_per_samp / 8;
    hdr.bytes_per_sec = hdr.sample_rate * hdr.bytes_per_samp;

    if (fwrite(&hdr, 4, 11, fileHandle_) != 11) {
        WARN("Could not write WAV header for file. ");
        return false;
    }

    DEBUG("Wrote wave header \"%s\"", header_to_string(hdr).c_str());
    return true;
}

bool AudioRecord::openExistingRawFile()
{
    fileHandle_ = fopen(filename_.c_str(), "ab+");

    if (!fileHandle_) {
        WARN("could not create RAW file!");
        return false;
    }

    return true;
}

bool AudioRecord::openExistingWavFile()
{
    DEBUG("Opening %s", filename_.c_str());

    fileHandle_ = fopen(filename_.c_str(), "rb+");

    if (!fileHandle_) {
        WARN("Could not open WAV file!");
        return false;
    }

    if (fseek(fileHandle_, 40, SEEK_SET) != 0)  // jump to data length
        WARN("Couldn't seek offset 40 in the file ");

    if (fread(&byteCounter_, 4, 1, fileHandle_))
        WARN("bytecounter Read successfully ");

    if (fseek(fileHandle_, 0 , SEEK_END) != 0)
        WARN("Couldn't seek at the en of the file ");


    if (fclose(fileHandle_) != 0)
        WARN("Can't close file r+ ");

    fileHandle_ = fopen(filename_.c_str(), "ab+");

    if (!fileHandle_) {
        WARN("Could not createopen WAV file ab+!");
        return false;
    }

    if (fseek(fileHandle_, 4 , SEEK_END) != 0)
        WARN("Couldn't seek at the en of the file ");

    return true;

}

void AudioRecord::closeWavFile()
{
    if (fileHandle_ == 0) {
        DEBUG("Can't closeWavFile, a file has not yet been opened!");
        return;
    }

    DEBUG("Close wave file");

    int32_t bytes = byteCounter_ * channels_;

    // jump to data length
    if (fseek(fileHandle_, 40, SEEK_SET) != 0)
        WARN("Could not seek in file");

    if (ferror(fileHandle_))
        WARN("Can't reach offset 40 while closing");

    fwrite(&bytes, sizeof(int32_t), 1, fileHandle_);

    if (ferror(fileHandle_))
        WARN("Can't write bytes for data length ");

    bytes = byteCounter_ * channels_ + 44; // + 44 for the wave header

    // jump to file size
    if (fseek(fileHandle_, 4, SEEK_SET) != 0)
        WARN("Could not seek in file");

    if (ferror(fileHandle_))
        WARN("Can't reach offset 4");

    fwrite(&bytes, 4, 1, fileHandle_);

    if (ferror(fileHandle_))
        WARN("Can't reach offset 4");

    if (fclose(fileHandle_) != 0)
        WARN("Can't close file");
}

void AudioRecord::recData(AudioBuffer& buffer)
{
    if (not recordingEnabled_)
        return;

    if (fileHandle_ == 0) {
        DEBUG("Can't record data, a file has not yet been opened!");
        return;
    }

    const size_t nSamples = buffer.samples();

    if (fwrite(buffer.getChannel(0), sizeof(SFLAudioSample), nSamples, fileHandle_) != nSamples) {
        WARN("Could not record data! ");
    } else {
        fflush(fileHandle_);
        byteCounter_ += nSamples * sizeof(SFLAudioSample);
    }
}
