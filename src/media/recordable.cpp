/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "audio/ringbufferpool.h"
#include "fileutils.h"
#include "logger.h"
#include "manager.h"
#include "recordable.h"

#include <iomanip>

namespace jami {

Recordable::Recordable()
    : recorder_(std::make_shared<MediaRecorder>())
{}

Recordable::~Recordable() {}

std::string
Recordable::getPath() const
{
    if (recorder_)
        return recorder_->getPath();
    else
        return "";
}

bool
Recordable::toggleRecording()
{
    if (!recorder_) {
        JAMI_ERR("Unable to toggle recording, non existent recorder");
        return false;
    }

    if (!recording_) {
        const auto& audioPath = Manager::instance().audioPreference.getRecordPath();
        auto dir = audioPath.empty() ? fileutils::get_home_dir() : std::filesystem::path(audioPath);
        dhtnet::fileutils::check_dir(dir);
        auto timeStamp = fmt::format("{:%Y%m%d-%H%M%S}", std::chrono::system_clock::now());
        startRecording((dir / timeStamp).string());
    } else {
        stopRecording();
    }
    return recording_;
}

bool
Recordable::startRecording(const std::string& path)
{
    std::lock_guard lk {apiMutex_};
    if (!recorder_) {
        JAMI_ERR("Unable to start recording, non existent recorder");
        return false;
    }

    if (!recording_) {
        if (path.empty()) {
            JAMI_ERR("Unable to start recording, path is empty");
            return false;
        }

        recorder_->audioOnly(isAudioOnly_);
        recorder_->setPath(path);
        recorder_->startRecording();
        recording_ = recorder_->isRecording();
    }

    return recording_;
}

void
Recordable::stopRecording()
{
    std::lock_guard lk {apiMutex_};
    if (!recorder_) {
        JAMI_WARN("Unable to stop recording, non existent recorder");
        return;
    }

    if (not recording_) {
        JAMI_WARN("Unable to stop non-running recording");
        return;
    }

    recorder_->stopRecording();
    recording_ = false;
}

bool
Recordable::isAudioOnly() const
{
    return isAudioOnly_;
}

} // namespace jami
