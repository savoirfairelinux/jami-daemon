/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
 *  Inspired by ringbuffer of Audacity Project
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
#ifndef __AUDIOFILE_H__
#define __AUDIOFILE_H__

#include <stdexcept>
#include "audio/audioloop.h"

namespace jami {

class AudioFileException : public std::runtime_error
{
public:
    AudioFileException(const std::string& str)
        : std::runtime_error("AudioFile: AudioFileException occurred: " + str)
    {}
};

/**
 * @brief Abstract interface for file readers
 */
class AudioFile : public AudioLoop
{
public:
    AudioFile(const std::string& filepath, unsigned int sampleRate);

    std::string getFilePath() const { return filepath_; }

protected:
    /** The absolute path to the sound file */
    std::string filepath_;

private:
    // override
    void onBufferFinish();
    unsigned updatePlaybackScale_;
};

} // namespace jami

#endif // __AUDIOFILE_H__
