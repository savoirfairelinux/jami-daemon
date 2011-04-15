/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#ifndef __AUDIOFILE_H__
#define __AUDIOFILE_H__

#include <fstream>

#include "audio/audioloop.h"
#include "audio/codecs/audiocodec.h"
#include "audio/codecs/codecDescriptor.h"


/**
 * @brief Abstract interface for file readers
 */
class AudioFile : public AudioLoop
{
    public:

        /**
        * Load a sound file in memory
        * @param filename  The absolute path to the file
        * @param codec     The codec to decode and encode it
        * @param sampleRate	The sample rate to read it
        * @return bool   True on success
        */
        virtual bool loadFile (const std::string& filename, AudioCodec *codec , unsigned int sampleRate) = 0;

        /**
         * Start the sound file
         */
        void start() {
            _start = true;
        }

        /**
         * Stop the sound file
         */
        void stop() {
            _start = false;
        }

        /**
         * Tells whether or not the file is playing
         * @return bool True if yes
         *		  false otherwise
         */
        bool isStarted() {
            return _start;
        }

    protected:

        /** start or not */
        bool _start;
};



/**
 * @file audiofile.h
 * @brief A class to manage sound files
 */

class RawFile : public AudioFile
{
    public:
        /**
         * Constructor
         */
        RawFile();

        /**
         * Destructor
         */
        ~RawFile();


        /**
         * Load a sound file in memory
         * @param filename  The absolute path to the file
         * @param codec     The codec to decode and encode it
         * @param sampleRate	The sample rate to read it
         * @return bool   True on success
         */
        virtual bool loadFile (const std::string& filename, AudioCodec *codec , unsigned int sampleRate);

    private:
        // Copy Constructor
        RawFile (const RawFile& rh);

        // Assignment Operator
        RawFile& operator= (const RawFile& rh);

        /** The absolute path to the sound file */
        std::string _filename;

        /** Your preferred codec */
        AudioCodec* _codec;
};


class WaveFile : public AudioFile
{

    public:

        WaveFile ();

        ~WaveFile();

        bool openFile (const std::string& fileName, int audioSamplingRate);

        bool closeFile();

        bool isFileExist (const std::string& fileName);

        bool isFileOpened();

        /**
             * Load a sound file in memory
             * @param filename  The absolute path to the file
             * @param codec     The codec to decode and encode it
             * @param sampleRate	The sample rate to read it
             * @return bool   True on success
             */
        virtual bool loadFile (const std::string& filename, AudioCodec *codec , unsigned int sampleRate);

    private:

        bool setWaveFile();

        bool openExistingWaveFile (const std::string& fileName, int audioSamplingRate);

        SOUND_FORMAT _snd_format;

        long _byte_counter;

        int _nb_channels;

        unsigned long _fileLength;

        unsigned long _data_offset;

        SINT16 _channels;

        SOUND_FORMAT _data_type;

        double _file_rate;

        std::fstream _file_stream;

        std::string _fileName;

};

#endif

