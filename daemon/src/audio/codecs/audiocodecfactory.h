/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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

#ifndef __CODEC_DESCRIPTOR_H__
#define __CODEC_DESCRIPTOR_H__

#include <map>
#include <vector>
#include <dirent.h>

#include "global.h"
#include "audiocodec.h"

/*
 * @file codecdescriptor.h
 * @brief Handle audio codecs, load them in memory
 */

class AudioCodecFactory {
    public:
        AudioCodecFactory();

        ~AudioCodecFactory();

        /**
         * Get codec name by its payload
         * @param payload the payload looked for
         *                same as getPayload()
         * @return std::string  The name of the codec
         */
        std::string getCodecName(int payload) const;

        std::vector<int32_t > getCodecList() const;
        /**
         * Get the codec object associated with the payload
         * @param payload The payload looked for
         * @return AudioCodec* A pointer on a AudioCodec object
         */
        sfl::AudioCodec* getCodec(int payload) const;

        /**
         * Get the codec object associated with the codec attribute
         * @param string The name to compare, should be in the form speex/16000
         * @return AudioCodec* A pointer to an AudioCodec object
         */
        sfl::AudioCodec* getCodec(const std::string &name) const;

        /**
         * Set the default codecs order.
         * This order will be apply to each account by default
         */
        void setDefaultOrder();

        /**
         * Get the bit rate of the specified codec.
         * @param payload The payload of the codec
         * @return double The bit rate
         */
        double getBitRate(int payload) const;

        /**
         * Get the clock rate of the specified codec
         * @param payload The payload of the codec
         * @return int The clock rate of the specified codec
         */
        int getSampleRate(int payload) const;

        /**
         * Get the number of channels of the specified codec
         * @param payload The payload of the codec
         * @return int The number of channels of the specified codec
         */
        unsigned getChannels(int payload) const;

        /**
         * Set the order of codecs by their payload
         * @param list The ordered list sent by DBus
         */
        void saveActiveCodecs(const std::vector<std::string>& list);

        /**
         * Instantiate a codec, used in AudioRTP to get an instance of Codec per call
         * @param CodecHandlePointer	The map containing the pointer on the object and the pointer on the handle function
         */
        sfl::AudioCodec* instantiateCodec(int payload) const;

        /**
         * For a given codec, return its specification
         *
         * @param payload	The RTP payload of the codec
         * @return std::vector <std::string>	A vector containing codec's name, sample rate, bandwidth and bit rate
         */
        std::vector <std::string> getCodecSpecifications(const int32_t& payload) const;

        /**
         *  Check if the audiocodec object has been successfully created
         *  @param payload  The payload of the codec
         *  @return bool  True if the audiocodec has been created
         *		false otherwise
         */
        bool isCodecLoaded(int payload) const;

    private:
        /** Maps a pointer on an audiocodec object to a payload */
        typedef std::map<int, sfl::AudioCodec*> AudioCodecsMap;

        /** Enable us to keep the handle pointer on the codec dynamicaly loaded so that we could destroy when we dont need it anymore */
        typedef std::pair<sfl::AudioCodec*, void*> AudioCodecHandlePointer;

        /**
         * Scan the installation directory ( --prefix configure option )
         * And load the dynamic library
         * @return std::vector<AudioCodec*> The list of the codec object successfully loaded in memory
         */
        std::vector<sfl::AudioCodec *> scanCodecDirectory();

        /**
         * Load a codec
         * @param std::string	The path of the shared ( dynamic ) library.
         * @return AudioCodec*  the pointer of the object loaded.
         */
        sfl::AudioCodec* loadCodec(const std::string &path);

        /**
         * Unload a codec
         * @param AudioCodecHandlePointer The map containing the pointer on the object and the pointer on the handle function
         */
        void unloadCodec(AudioCodecHandlePointer &ptr);

        /**
         * Check if the files found in searched directories seems valid
         * @param std::string	The name of the file
         * @return bool True if the file name begins with libcodec_ and ends with .so
         *		false otherwise
         */
        static bool seemsValid(const std::string &lib);

        /**
         * Check if the codecs shared library has already been scanned during the session
         * Useful not to load twice the same codec saved in the different directory
         * @param std::string	The complete name of the shared directory ( without the path )
         * @return bool True if the codecs has been scanned
         *	    false otherwise
         */
        bool alreadyInCache(const std::string &lib);

        /**
         * Map the payload of a codec and the object associated ( AudioCodec * )
         */
        AudioCodecsMap codecsMap_;

        /**
         * Vector containing a default order for the codecs
         */
        std::vector<int> defaultCodecList_;

        /**
         * Vector containing the complete name of the codec shared library scanned
         */
        std::vector<std::string> libCache_;

        /**
         * Vector containing pairs
         * Pair between pointer on function handle and pointer on audiocodec object
         */
        std::vector<AudioCodecHandlePointer> codecInMemory_;
};

#endif // __CODEC_DESCRIPTOR_H__
