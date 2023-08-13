/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "audio/tonecontrol.h"
#include "sound/tonelist.h"
#include "client/ring_signal.h"
#include "jami/callmanager_interface.h" // for CallSignal

namespace jami {

static constexpr unsigned DEFAULT_SAMPLE_RATE = 8000;

ToneControl::ToneControl(const Preferences& preferences)
    : prefs_(preferences)
    , sampleRate_(DEFAULT_SAMPLE_RATE)
{}

ToneControl::~ToneControl() {}

void
ToneControl::setSampleRate(unsigned rate, AVSampleFormat sampleFormat)
{
    std::lock_guard<std::mutex> lk(mutex_);
    sampleRate_ = rate;
    sampleFormat_ = sampleFormat;
    if (!telephoneTone_)
        telephoneTone_.reset(new TelephoneTone(prefs_.getZoneToneChoice(), rate, sampleFormat));
    else
        telephoneTone_->setSampleRate(rate, sampleFormat);
    if (!audioFile_) {
        return;
    }
    auto path = audioFile_->getFilePath();
    try {
        audioFile_.reset(new AudioFile(path, sampleRate_, sampleFormat));
    } catch (const AudioFileException& e) {
        JAMI_WARN("Audio file error: %s", e.what());
    }
}

std::shared_ptr<AudioLoop>
ToneControl::getTelephoneTone()
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (telephoneTone_)
        return telephoneTone_->getCurrentTone();
    return nullptr;
}

std::shared_ptr<AudioLoop>
ToneControl::getTelephoneFile(void)
{
    std::lock_guard<std::mutex> lk(mutex_);
    return audioFile_;
}

bool
ToneControl::setAudioFile(const std::string& file)
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (audioFile_) {
        emitSignal<libjami::CallSignal::RecordPlaybackStopped>(audioFile_->getFilePath());
        audioFile_.reset();
    }

    try {
        audioFile_.reset(new AudioFile(file, sampleRate_, sampleFormat_));
    } catch (const AudioFileException& e) {
        JAMI_WARN("Audio file error: %s", e.what());
    }

    return static_cast<bool>(audioFile_);
}

void
ToneControl::stopAudioFile()
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (audioFile_) {
        emitSignal<libjami::CallSignal::RecordPlaybackStopped>(audioFile_->getFilePath());
        audioFile_.reset();
    }
}

void
ToneControl::stop()
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (telephoneTone_)
        telephoneTone_->setCurrentTone(Tone::ToneId::TONE_NULL);

    if (audioFile_) {
        emitSignal<libjami::CallSignal::RecordPlaybackStopped>(audioFile_->getFilePath());
        audioFile_.reset();
    }
}

void
ToneControl::play(Tone::ToneId toneId)
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (telephoneTone_)
        telephoneTone_->setCurrentTone(toneId);
}

void
ToneControl::seek(double value)
{
    std::lock_guard<std::mutex> lk(mutex_);

    if (audioFile_)
        audioFile_->seek(value);
}

} // namespace jami
