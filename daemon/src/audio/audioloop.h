/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *
 *  Inspired by tonegenerator of
 *   Laurielle Lea <laurielle.lea@savoirfairelinux.com> (2004)
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
#ifndef __AUDIOLOOP_H__
#define __AUDIOLOOP_H__

#include "sfl_types.h"
#include <cstring>
#include "noncopyable.h"
#include "audiobuffer.h"

/**
 * @file audioloop.h
 * @brief Loop on a sound file
 */

class AudioLoop {
    public:
        AudioLoop(unsigned int sampleRate);

        virtual ~AudioLoop();

        /**
         * Get the next fragment of the tone
         * the function change the intern position, and will loop
         * @param output  The data buffer
         * @param nb of int16 to send
         * @param gain The gain [-1.0, 1.0]
         */
        void getNext(AudioBuffer& output, double gain);

        void seek(double relative_position);

        /**
         * Reset the pointer position
         */
        void reset() {
            pos_ = 0;
        }

        /**
         * Accessor to the size of the buffer
         * @return unsigned int The size
         */
        size_t getSize() {
            return buffer_->frames();
        }

    protected:
        /** The data buffer */
        AudioBuffer * buffer_;

        /** current position, set to 0, when initialize */
        size_t pos_;

    private:
        NON_COPYABLE(AudioLoop);
        virtual void onBufferFinish();
};

#endif // __AUDIOLOOP_H__

