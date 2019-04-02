/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Adrien Beraud <adrien.beraud@gmail.com>
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

#ifndef DCBLOCKER_H
#define DCBLOCKER_H

#include "ring_types.h"
#include "audiobuffer.h"

namespace jami {

class DcBlocker {
    public:
        DcBlocker(unsigned channels = 1);
        void reset();

        void process(AudioSample *out, AudioSample *in, int samples);

        /**
         * In-place processing of all samples in buf (each channel treated independently)
         */
        void process(AudioBuffer& buf);

    private:
        struct StreamState {
            AudioSample y_, x_, xm1_, ym1_;
        };

        void doProcess(AudioSample *out, AudioSample *in, unsigned samples, struct StreamState * state);

        std::vector<StreamState> states;
};

} // namespace jami

#endif
