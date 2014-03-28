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

#include "dcblocker.h"

DcBlocker::DcBlocker(unsigned channels /* = 1 */)
    : states(channels, (struct StreamState){0, 0, 0, 0})
{}

void DcBlocker::reset()
{
    states.assign(states.size(), (struct StreamState){0, 0, 0, 0});
}

void DcBlocker::doProcess(SFLAudioSample *out, SFLAudioSample *in, unsigned samples, struct StreamState * state)
{
    for (unsigned i = 0; i < samples; ++i) {
        state->x_ = in[i];


        state->y_ = (SFLAudioSample) ((float) state->x_ - (float) state->xm1_ + 0.9999 * (float) state->y_);
        state->xm1_ = state->x_;
        state->ym1_ = state->y_;

        out[i] = state->y_;
    }
}

void DcBlocker::process(SFLAudioSample *out, SFLAudioSample *in, int samples)
{
    if (out == NULL or in == NULL or samples == 0) return;
    doProcess(out, in, samples, &states[0]);
}

void DcBlocker::process(AudioBuffer& buf)
{
    const size_t chans = buf.channels();
    const size_t samples = buf.frames();
    if (chans > states.size())
        states.resize(buf.channels(), (struct StreamState){0, 0, 0, 0});

    unsigned i;
    for(i=0; i<chans; i++) {
        SFLAudioSample *chan = buf.getChannel(i)->data();
        doProcess(chan, chan, samples, &states[i]);
    }
}

