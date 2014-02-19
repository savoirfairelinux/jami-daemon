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

VideoInputSelector::VideoInputSelector(const std::string& device) :
    VideoFramePassiveReader::VideoFramePassiveReader()
    , VideoFrameActiveWriter::VideoFrameActiveWriter()
    , currentInput_()
{
	openInput(device);
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
VideoInputSelector::openInput(const std::string& device)
{
	currentInput_ = new VideoInput(device);
	currentInput_->attach(this);
}

void
VideoInputSelector::closeInput(void)
{
	currentInput_->detach(this);
	delete currentInput_;
}

void
VideoInputSelector::switchInput(const std::string& device)
{
	DEBUG("Switching input to %s", device.c_str());
	closeInput();
	openInput(device);
}

} // end namespace sfl_video
