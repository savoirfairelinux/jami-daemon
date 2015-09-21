/*
 *  Copyright (C) 2011-2015 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include <unistd.h> // for sleep
#include "test_video_input.h"
#include "video_input.h"
#include <map>
#include <string>

namespace ring { namespace video { namespace test {

void VideoInputTest::testInput()
{
    std::string resource = "display://" + std::string(getenv("DISPLAY") ? : ":0.0");
    VideoInput video;
    video.switchInput(resource);
    usleep(10000);
}

}}} // namespace ring::video::test

int main ()
{
    for (int i = 0; i < 20; ++i) {
        ring::video::test::VideoInputTest test;
        test.testInput();
    }
    return 0;
}
