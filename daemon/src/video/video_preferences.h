/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef VIDEO_PREFERENCE_H__
#define VIDEO_PREFERENCE_H__

#include "video/video_v4l2_list.h"
#include "video/video_v4l2.h"

namespace sflvideo {
    class VideoV4l2ListThread;
}
// video preferences
static const char * const videoDeviceKey = "v4l2Dev";
static const char * const videoChannelKey = "v4l2Channel";
static const char * const videoSizeKey = "v4l2Size";
static const char * const videoRateKey = "v4l2Rate";

class VideoPreference : public Serializable
{
    public:

        VideoPreference();
        ~VideoPreference();

        virtual void serialize(Conf::YamlEmitter *emitter);

        virtual void unserialize(Conf::MappingNode *map);

        std::map<std::string, std::string> getVideoSettings();

        std::string getDevice() const {
            return device_;
        }

        void setDevice(const std::string &device) {
            device_ = device;
        }

        std::string getChannel() const {
            return channel_;
        }

        void setChannel(const std::string & input) {
            channel_ = input;
        }

        std::string getSize() const {
            return size_;
        }

        void setSize(const std::string & size) {
            size_ = size;
        }

        const std::string & getRate() const {
            return rate_;
        }

        void setRate(const std::string & rate) {
            rate_ = rate;
        }

        std::vector<std::string> getDeviceList() const {
        	return v4l2_list_->getDeviceList();
        }

        std::vector<std::string> getChannelList(const std::string &dev) const {
        	return v4l2_list_->getChannelList(dev);
        }

        std::vector<std::string> getSizeList(const std::string &dev, const std::string &channel) const {
        	return v4l2_list_->getSizeList(dev, channel);
        }

        std::vector<std::string> getRateList(const std::string &dev, const std::string &channel, const std::string &size) const {
        	return v4l2_list_->getRateList(dev, channel, size);
        }

    private:
        NON_COPYABLE(VideoPreference);

        // V4L2 devices
        sfl_video::VideoV4l2ListThread *v4l2_list_;

        std::string device_;
        std::string channel_;
        std::string size_;
        std::string rate_;
};

#endif
