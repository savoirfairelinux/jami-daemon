/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
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

#include "video_input_selector.h"
#include "video_input.h"
#include "check.h"

#include "manager.h"
#include "client/video_controls.h"

#include <map>
#include <string>

namespace sfl_video {

VideoInputSelector::VideoInputSelector(const std::string& resource) :
    VideoFramePassiveReader::VideoFramePassiveReader()
    , VideoFrameActiveWriter::VideoFrameActiveWriter()
    , currentInput_(nullptr)
{
	switchInput(resource);
}

VideoInputSelector::~VideoInputSelector()
{
	closeInput();
}

void
VideoInputSelector::update(Observable<std::shared_ptr<sfl_video::VideoFrame>>* /* input */, std::shared_ptr<VideoFrame>& frame_ptr)
{
	notify(frame_ptr);
}

void
VideoInputSelector::openInput(const std::map<std::string, std::string>& map)
{
	currentInput_ = new VideoInput(map);
	currentInput_->attach(this);
}

void
VideoInputSelector::closeInput()
{
    if (currentInput_ == nullptr)
        return;

    currentInput_->detach(this);
    delete currentInput_;
    currentInput_ = nullptr;
}

static std::map<std::string, std::string>
initCamera(const std::string& device)
{
    std::map<std::string, std::string> map =
        Manager::instance().getVideoControls()->getSettingsFor(device);

    map["format"] = "video4linux2";
    map["mirror"] = "true"; // only the key matters

    return map;
}

static std::map<std::string, std::string>
initX11(std::string display)
{
    std::map<std::string, std::string> map;
    size_t space = display.find(' ');

    if (space != std::string::npos) {
        map["video_size"] = display.substr(space + 1);
        map["input"] = display.erase(space);
    } else {
        map["input"] = display;
        map["video_size"] = "vga";
    }

    map["format"] = "x11grab";
    map["framerate"] = "25";

    return map;
}

bool
VideoInputSelector::switchInput(const std::string& resource)
{
    DEBUG("Switching input to MRL '%s'", resource.c_str());

    // Supported MRL schemes
    static const std::string v4l2("v4l2://");
    static const std::string display("display://");

    std::map<std::string, std::string> map;

    /* Video4Linux2 */
    if (resource.compare(0, v4l2.size(), v4l2) == 0)
        map = initCamera(resource.substr(v4l2.size()));

    /* X11 display name */
    else if (resource.compare(0, display.size(), display) == 0)
        map = initX11(resource.substr(display.size()));

    /* Unsupported MRL or failed initialization */
    if (map.empty()) {
        ERROR("Failed to init input map for MRL '%s'\n", resource.c_str());
        return false;
    }

    closeInput();
    openInput(map);
    return true;
}

} // end namespace sfl_video
