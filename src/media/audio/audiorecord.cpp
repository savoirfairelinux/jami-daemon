/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "audiorecord.h"
#include "logger.h"
#include "fileutils.h"
#include "manager.h"

#ifndef RING_UWP
#include <sndfile.hh>
#endif

#include <algorithm>
#include <sstream> // for stringstream
#include <cstdio>
#include <unistd.h>

namespace ring {

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

    if (timeinfo->tm_min < 10) // 01 02 03, not 1 2 3
        out << 0;

    out << timeinfo->tm_min;

    if (timeinfo->tm_sec < 10) // 01 02 03,  not 1 2 3
        out << 0;

    out << timeinfo->tm_sec;
    return out.str();
}

AudioRecord::AudioRecord()
    : sndFormat_(AudioFormat::MONO())
    , filename_(createFilename())
    , savePath_()
    , recorder_(this, Manager::instance().getRingBufferPool())
{
    RING_DBG("Generate filename for this call %s ", filename_.c_str());
}

AudioRecord::~AudioRecord()
{
    closeFile();
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
    RING_DBG("Initialize audio record for peer  : %s", peerNumber.c_str());
    // if savePath_ don't contains filename
    if (savePath_.find(".wav") == std::string::npos) {
        filename_ = createFilename();
        filename_.append("-" + sanitize(peerNumber) + "-" PACKAGE);
        filename_.append(".wav");
    } else {
        filename_ = "";
    }
}

std::string AudioRecord::getFilename() const
{
    return savePath_ + filename_;
}

bool
AudioRecord::openFile()
{
#ifndef RING_UWP
    fileHandle_.reset(); // do it before calling fileExists()

    const bool doAppend = fileExists();
    const int access = doAppend ? SFM_RDWR : SFM_WRITE;

    RING_DBG("Opening file %s with format %s", getFilename().c_str(), sndFormat_.toString().c_str());
    fileHandle_.reset(new SndfileHandle (getFilename().c_str(),
                                         access,
                                         SF_FORMAT_WAV | SF_FORMAT_PCM_16,
                                         sndFormat_.nb_channels,
                                         sndFormat_.sample_rate));

    // check overloaded boolean operator
    if (!*fileHandle_) {
        RING_WARN("Could not open WAV file!");
        fileHandle_.reset();
        return false;
    }

    if (doAppend and fileHandle_->seek(0, SEEK_END) < 0)
        RING_WARN("Couldn't seek to the end of the file ");

    return true;
#else
    return false;
#endif
}

void
AudioRecord::closeFile()
{
    stopRecording(); // needed as recData accesses to fileHandle_
    fileHandle_.reset();
}

bool
AudioRecord::isOpenFile() const noexcept
{
    return static_cast<bool>(fileHandle_);
}

bool AudioRecord::fileExists() const
{
    return access(getFilename().c_str(), F_OK) != -1;
}

bool AudioRecord::isRecording() const
{
    return recordingEnabled_;
}

bool
AudioRecord::toggleRecording()
{
    if (isOpenFile())
        recordingEnabled_ = !recordingEnabled_;
    else if (openFile()) {
        recordingEnabled_ = true;
        recorder_.start();
    }

    return recordingEnabled_;
}

void
AudioRecord::stopRecording() const noexcept
{
    RING_DBG("Stop recording %s", getFilename().c_str());
    recordingEnabled_ = false;
}

void
AudioRecord::recData(AudioBuffer& buffer)
{
#ifndef RING_UWP
    if (not recordingEnabled_)
        return;

    auto interleaved = buffer.interleave();
    const int nSamples = interleaved.size();
    if (fileHandle_->write(interleaved.data(), nSamples) != nSamples) {
        RING_WARN("Could not record data!");
    } else {
        fileHandle_->writeSync();
    }
#endif
}

} // namespace ring
