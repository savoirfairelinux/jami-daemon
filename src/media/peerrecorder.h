/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

#include "recordable.h"

namespace jami {

/*
 * @file remoterecorder.h
 * @brief Handles the peer's recording states.
 */

class PeerRecorder
{
public:
    PeerRecorder() {};
    virtual ~PeerRecorder() {};

    virtual void peerRecording(bool state) = 0;

    virtual bool isPeerRecording() const { return peerRecording_; }

    virtual void peerMuted(bool muted, int streamIdx) = 0;

    virtual bool isPeerMuted() const { return peerMuted_; }

    virtual void peerVoice(bool voice) = 0;

    virtual bool hasPeerVoice() const { return peerVoice_; }

protected:
    bool peerRecording_ {false};
    bool peerMuted_ {false};
    bool peerVoice_ {false};
};

} // namespace jami
