/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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
#pragma once

#include "observer.h"
#include "logger.h"
#include "jami/videomanager_interface.h"

#if defined(ENABLE_VIDEO) && defined(ENABLE_HWACCEL)
#include "media/video/accel.h"
#endif

extern "C" {
#include <libavutil/frame.h>
}

#include <exception>
#include <memory>

namespace jami {

/**
 * @brief Subject forwarding media frames of a call/conference stream to plugins.
 *
 * Plugins receive raw AVFrame pointers and dereference them unconditionally,
 * so this subject only forwards frames that are safe to process on the CPU:
 * - empty/unallocated frames are dropped instead of being forwarded;
 * - hardware frames (e.g. D3D11/DXVA2/VAAPI surfaces) are downloaded to main
 *   memory using the native software format of their frames context.
 *
 * This is deliberately scoped to plugin streams: generic PublishMapSubject
 * behavior is left unchanged for other users.
 */
class PluginMediaStreamSubject : public PublishMapSubject<std::shared_ptr<libjami::MediaFrame>, AVFrame*>
{
public:
    using PublishMapSubject::PublishMapSubject;

    void update(Observable<std::shared_ptr<libjami::MediaFrame>>*, const std::shared_ptr<libjami::MediaFrame>& frame) override
    {
        if (!frame || getObserversCount() == 0)
            return;

        const auto* avframe = frame->pointer();
        if (!isUsable(avframe))
            return;

        if (avframe->hw_frames_ctx) {
#if defined(ENABLE_VIDEO) && defined(ENABLE_HWACCEL)
            std::unique_ptr<libjami::VideoFrame> swFrame;
            try {
                // AV_PIX_FMT_NONE lets FFmpeg pick the native transfer format
                // of the frames context instead of forcing one.
                swFrame = video::HardwareAccel::transferToMainMemory(static_cast<const libjami::VideoFrame&>(*frame),
                                                                     AV_PIX_FMT_NONE);
            } catch (const std::exception& e) {
                JAMI_WARNING("Unable to download hardware frame for plugins: {}", e.what());
                return;
            }
            auto* out = swFrame ? swFrame->pointer() : nullptr;
            if (isUsable(out) && !out->hw_frames_ctx)
                this->notify(out);
#endif
            // Hardware frames cannot be processed by plugins without a download.
            return;
        }

        if (auto* out = map_(frame))
            this->notify(out);
    }

private:
    static bool isUsable(const AVFrame* f)
    {
        if (!f || (!f->buf[0] && !f->data[0]))
            return false;
        // Video frames must have valid dimensions; audio frames carry samples.
        if (f->nb_samples <= 0 && (f->width <= 0 || f->height <= 0))
            return false;
        return true;
    }
};

} // namespace jami
