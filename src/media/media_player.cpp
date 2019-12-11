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

#include "media_player.h"
#include "client/videomanager.h"

//#include "data_transfer.h"
//#include "manager.h"
//#include "jamidht/jamiaccount.h"
//#include "string_utils.h"
//#include "channel.h"
//#include "turn_transport.h"
//#include "security/tls_session.h"
//
//#include <algorithm>
//#include <future>
//#include <vector>
//#include <atomic>
//#include <stdexcept>
//#include <istream>
//#include <ostream>
//#include <unistd.h>
//#include <cstdio>
//
//#ifdef _WIN32
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#else
//#include <sys/select.h>
//#endif
//
//#ifndef _MSC_VER
//#include <sys/time.h>
//#endif
namespace jami {
MediaPlayer::MediaPlayer(std::string path)
{
    audioInput_ = jami::getAudioInput(path);
    videoInput_ = std::static_pointer_cast<video::VideoInput>(jami::getVideoCamera());
    path_ = path;
    sinkId_ = rand();
    videoInput_.setSink(sinkId_);
    audioInput_.switchInput(path);
    videoInput_.switchInput(path);
}

MediaPlayer::~MediaPlayer()
{}

void
MediaPlayer::toglePause()
{
    paused_ = !paused_;
    videoInput_.setPaused(paused_);
    audioInput_.setPaused(paused_);
}

const std::string&
MediaPlayer::getSinkId() {
    return sinkId_;
}
}

