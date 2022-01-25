/****************************************************************************
 *    Copyright (C) 2018-2022 Savoir-faire Linux Inc.                       *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include "videomanager_interface.h"

namespace DRing {
class DRING_PUBLIC GenericVideoFrameBuffer : public VideoFrameBufferIf
{
public:
    GenericVideoFrameBuffer() = delete;
    GenericVideoFrameBuffer(const GenericVideoFrameBuffer&) = delete;
    GenericVideoFrameBuffer(const GenericVideoFrameBuffer&&) = delete;

    GenericVideoFrameBuffer(std::size_t size);
    GenericVideoFrameBuffer(uint8_t* buf, size_t size);
    ~GenericVideoFrameBuffer();

    void allocateMemory(int format, int width, int height, int align) override;
    std::size_t size() const override;
    int width() const override;
    int height() const override;
    int planes() const override;
    int stride(int plane = 0) const override;
    uint8_t* ptr(int plane = 0) override;
    int format() const override;
    AVFrame* avframe() override;

private:
    int format_ {0};
    int width_ {0};
    int height_ {0};
    // This implementation support only 1 plane.
    int planes_ {1};
    std::vector<int> strides_;
    // True if the instance own the inner buffer.
    bool owner_ {false};
    VideoBufferType bufferType_ {VideoBufferType::DEFAULT};
    uint8_t* videoBuffer_ {nullptr};
    std::size_t bufferSize_ {0};
};

} // namespace DRing

namespace DRing {
class DRING_PUBLIC AVVideoFrameBuffer : public VideoFrameBufferIf
{
public:
    AVVideoFrameBuffer() = delete;
    AVVideoFrameBuffer(const AVVideoFrameBuffer&) = delete;
    AVVideoFrameBuffer(const AVVideoFrameBuffer&&) = delete;

    AVVideoFrameBuffer(std::size_t size);
    AVVideoFrameBuffer(uint8_t* buf, size_t size);
    ~AVVideoFrameBuffer();

    void allocateMemory(int format, int width, int height, int align) override;
    std::size_t size() const override;
    int width() const override;
    int height() const override;
    int planes() const override;
    int stride(int plane = 0) const override;
    uint8_t* ptr(int plane = 0) override;
    int format() const override;
    AVFrame* avframe() override;

private:
    int planes_ {0};
    VideoBufferType bufferType_ {VideoBufferType::AV_FRAME};
    std::size_t bufferSize_ {0};
    std::unique_ptr<AVFrame, void (*)(AVFrame*)> avframe_ {nullptr, [](AVFrame* frame) {
                                                               av_frame_free(&frame);
                                                           }};
};

} // namespace DRing