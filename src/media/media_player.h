/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

//#include "dring/datatransfer_interface.h"
//#include "ip_utils.h"
//#include "generic_io.h"
//#include "security/diffie-hellman.h"
//#include "opendht/crypto.h"
//#include "ice_transport.h"
//#include "security/tls_session.h"

#include <functional>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "audio/audio_input.h"
#include "video/video_input.h"

class MediaPlayer
{
public:
    MediaPlayer(std::string path);

    ~MediaPlayer();

    void toglePause();
    const std::string& getSinkId();

private:
    std::string path_;

    // media inputs
    std::shared_ptr<jami::video::VideoInput> videoInput_;
    std::shared_ptr<jami::AudioInput> audioInput_;
    std::string sinkId_;
    bool paused_ = false;
};

