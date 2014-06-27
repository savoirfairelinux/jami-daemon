/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include "logger.h"
#include "fileutils.h"

#include <sndfile.hh>

#include <algorithm>
#include <sstream> // for stringstream
#include <cstdio>
#include <unistd.h>

static std::string
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

AudioRecord::AudioRecord() : fileHandle_(nullptr)
    , sndFormat_(AudioFormat::MONO)
    , recordingEnabled_(false)
    , filename_(createFilename())
    , savePath_()
{
    WARN("Generate filename for this call %s ", filename_.c_str());
}

AudioRecord::~AudioRecord()
{
    delete fileHandle_;
}

void AudioRecord::setSndFormat(AudioFormat format)
{
    sndFormat_ = format;
}

void AudioRecord::setRecordingOptions(AudioFormat format, const std::string &path)
{
    std::string filePath;

    // use HOME directory if path is empty, or if path does not exist
    if (path.empty() or not fileutils::check_dir(path.c_str())) {
        filePath = fileutils::get_home_dir();
    } else {
        filePath = path;
    }

    sndFormat_ = format;
    savePath_ = (*filePath.rbegin() == DIR_SEPARATOR_CH) ? filePath : filePath + DIR_SEPARATOR_STR;
}

static bool
nonFilenameCharacter(char c)
{
    return not(std::isalnum(c) or c == '_' or c == '.');
}

// Replace any character that is inappropriate for a filename with '_'
static std::string
sanitize(std::string s)
{
    std::replace_if(s.begin(), s.end(), nonFilenameCharacter, '_');
    return s;
}

void AudioRecord::initFilename(const std::string &peerNumber)
{
    std::string fName(filename_);
    fName.append("-" + sanitize(peerNumber) + "-" PACKAGE);

    if (filename_.find(".wav") == std::string::npos) {
        DEBUG("Concatenate .wav file extension: name : %s", filename_.c_str());
        fName.append(".wav");
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
    delete fileHandle_;
    const bool doAppend = fileExists();
    const int access = doAppend ? SFM_RDWR : SFM_WRITE;

    DEBUG("Opening file %s with format %s", savePath_.c_str(), sndFormat_.toString().c_str());
    fileHandle_ = new SndfileHandle(savePath_.c_str(), access, SF_FORMAT_WAV | SF_FORMAT_PCM_16, sndFormat_.nb_channels, sndFormat_.sample_rate);

    // check overloaded boolean operator
    if (!*fileHandle_) {
        WARN("Could not open WAV file!");
        delete fileHandle_;
        fileHandle_ = 0;
        return false;
    }

    if (doAppend and fileHandle_->seek(0, SEEK_END) < 0)
        WARN("Couldn't seek to the end of the file ");

    return result;
}

void AudioRecord::closeFile()
{
    delete fileHandle_;
    fileHandle_ = 0;
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

void AudioRecord::recData(AudioBuffer& buffer)
{
    if (not recordingEnabled_)
        return;

    if (fileHandle_ == 0) {
        DEBUG("Can't record data, a file has not yet been opened!");
        return;
    }

    auto interleaved = buffer.interleave();
    const int nSamples = interleaved.size();
    if (fileHandle_->write(interleaved.data(), nSamples) != nSamples) {
        WARN("Could not record data!");
    } else {
        fileHandle_->writeSync();
    }
}
