/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
 *
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

#pragma once

#include "preferences.h"
#include "audio/sound/tone.h"  // for Tone::TONEID declaration
#include "audio/sound/tonelist.h"
#include "audio/sound/audiofile.h"

#include <mutex>

namespace ring {

/**
 * ToneControl is a class to handle application wide business logic
 * to control audio tones played at various application events.
 * Having an application wide instance gives a way to handle
 * complexes interactions occuring in a multi-call context.
 */

class ToneControl {
    public:
        ToneControl() = delete;
        ToneControl(const Preferences& preferences);
        ~ToneControl();

        void setSampleRate(unsigned rate);
        AudioLoop* getTelephoneTone();
        AudioLoop* getTelephoneFile(void);
        bool setAudioFile(const std::string& file);
        void stopAudioFile();
        void stop();
        void play(Tone::TONEID toneId);
        void seek(double value);

    private:
        const Preferences& prefs_;

        std::mutex mutex_; // protect access to following members
        unsigned sampleRate_;
        std::unique_ptr<TelephoneTone> telephoneTone_;
        std::unique_ptr<AudioFile> audioFile_;
};

} // namespace ring
