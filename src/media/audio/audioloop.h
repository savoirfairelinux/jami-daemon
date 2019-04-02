/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
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
 */

#pragma once

#include "ring_types.h"
#include "noncopyable.h"
#include "audiobuffer.h"

/**
 * @file audioloop.h
 * @brief Loop on a sound file
 */

namespace jami {

class AudioLoop {
    public:
        AudioLoop() {}

        AudioLoop(unsigned int sampleRate);

        AudioLoop& operator=(AudioLoop&& o) noexcept {
            std::swap(buffer_, o.buffer_);
            std::swap(pos_, o.pos_);
            return *this;
        }

        virtual ~AudioLoop();

        /**
         * Get the next fragment of the tone
         * the function change the intern position, and will loop
         * @param output  The data buffer
         * @param nb of int16 to send
         * @param gain The gain [-1.0, 1.0]
         */
        void getNext(AudioBuffer& output, double gain);
        std::unique_ptr<AudioFrame> getNext(size_t samples = 0);

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
        size_t getSize() const {
            return buffer_->frames();
        }
        AudioFormat getFormat() const {
            return buffer_->getFormat();
        }
    protected:
        /** The data buffer */
        AudioBuffer * buffer_ {nullptr};

        /** current position, set to 0, when initialize */
        size_t pos_ {0};

    private:
        NON_COPYABLE(AudioLoop);
        virtual void onBufferFinish();
};

} // namespace jami

