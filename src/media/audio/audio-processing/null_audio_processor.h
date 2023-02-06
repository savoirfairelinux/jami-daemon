/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
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

#include "audio_processor.h"

namespace jami {

class NullAudioProcessor final : public AudioProcessor
{
public:
    NullAudioProcessor(AudioFormat format, unsigned frameSize);
    ~NullAudioProcessor() = default;

    std::shared_ptr<AudioFrame> getProcessed() override;

    void enableEchoCancel(bool) override {};

    void enableNoiseSuppression(bool) override {};

    void enableAutomaticGainControl(bool) override {};

    void enableVoiceActivityDetection(bool) override {};
};

} // namespace jami
