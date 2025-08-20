/*
 *  Copyright (C) 2025 Savoir-faire Linux Inc.
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

#include "debug_audio_capture.h"
#include "logger.h"
#include "libav_deps.h"

#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

extern "C" {
#include <libavutil/samplefmt.h>
}

namespace jami {

DebugAudioCapture&
DebugAudioCapture::instance()
{
    static DebugAudioCapture instance_;
    return instance_;
}

DebugAudioCapture::DebugAudioCapture()
{
    // Create output directory if it doesn't exist
    try {
        std::filesystem::create_directories(outputDir_);
    } catch (const std::exception& e) {
        JAMI_WARN("Failed to create debug audio directory %s: %s", outputDir_.c_str(), e.what());
    }
}

DebugAudioCapture::~DebugAudioCapture()
{
    stopAllCaptures();
}

void
DebugAudioCapture::startCapture(AudioTracePoint point, const std::string& sessionId)
{
    std::lock_guard lock(capturesMutex_);

    auto& session = sessions_[point];
    if (!session) {
        session = std::make_unique<CaptureSession>();
    }

    // Reset session
    session->frames.clear();
    session->sessionId = sessionId.empty()
                             ? std::to_string(
                                   std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now().time_since_epoch())
                                       .count())
                             : sessionId;
    session->frameCount = 0;
    session->startTime = std::chrono::steady_clock::now();
    session->active = true;

    if (debugLogging_) {
        JAMI_DBG("Started audio capture for %s (session: %s)",
                 tracePointToString(point).c_str(),
                 session->sessionId.c_str());
    }
}

void
DebugAudioCapture::stopCapture(AudioTracePoint point)
{
    std::lock_guard lock(capturesMutex_);

    auto it = sessions_.find(point);
    if (it != sessions_.end() && it->second) {
        it->second->active = false;

        if (debugLogging_) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - it->second->startTime);
            JAMI_DBG("Stopped audio capture for %s after %lld ms (%zu frames)",
                     tracePointToString(point).c_str(),
                     duration.count(),
                     it->second->frameCount);
        }
    }
}

void
DebugAudioCapture::stopAllCaptures()
{
    std::lock_guard lock(capturesMutex_);

    for (auto& [point, session] : sessions_) {
        if (session) {
            session->active = false;
        }
    }

    if (debugLogging_) {
        JAMI_DBG("Stopped all audio captures");
    }
}

void
DebugAudioCapture::captureFrame(AudioTracePoint point,
                                const std::shared_ptr<AudioFrame>& frame,
                                const std::string& context)
{
    if (!frame || !frame->pointer()) {
        if (debugLogging_) {
            JAMI_WARN("Attempted to capture null or invalid frame for %s",
                      tracePointToString(point).c_str());
        }
        return;
    }

    std::lock_guard lock(capturesMutex_);

    auto it = sessions_.find(point);
    if (it == sessions_.end() || !it->second || !it->second->active) {
        return;
    }

    auto& session = it->second;

    // Check if we've exceeded the maximum number of frames
    if (session->frameCount >= maxCaptureFrames_) {
        if (debugLogging_) {
            JAMI_WARN("Audio capture for %s reached maximum frames (%zu), stopping",
                      tracePointToString(point).c_str(),
                      maxCaptureFrames_);
        }
        session->active = false;
        return;
    }

    // Store the format from the first frame
    if (session->frames.empty()) {
        session->format = frame->getFormat();
        if (debugLogging_) {
            JAMI_DBG("Audio capture %s format: %s",
                     tracePointToString(point).c_str(),
                     session->format.toString().c_str());
        }
    }

    // Verify format consistency
    if (frame->getFormat() != session->format) {
        if (debugLogging_) {
            JAMI_WARN("Format mismatch in capture %s: expected %s, got %s",
                      tracePointToString(point).c_str(),
                      session->format.toString().c_str(),
                      frame->getFormat().toString().c_str());
        }
        // Continue with the new format if it's the first frame
        if (session->frames.empty()) {
            session->format = frame->getFormat();
        } else {
            // Skip frames with mismatched format to prevent corruption
            return;
        }
    }

    // Create a copy of the frame to avoid any threading issues
    auto frameCopy = std::make_shared<AudioFrame>(frame->getFormat(), frame->getFrameSize());
    if (!frameCopy->pointer()) {
        JAMI_ERR("Failed to create frame copy for %s", tracePointToString(point).c_str());
        return;
    }

    // Copy the audio data based on the sample format
    bool copySuccess = false;
    auto avFrame = frame->pointer();
    auto avFrameCopy = frameCopy->pointer();

    if (avFrame->extended_data && avFrameCopy->extended_data) {
        copySuccess = true;

        // Copy the audio data based on format
        switch (session->format.sampleFormat) {
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S32:
            // For packed formats, copy directly
            for (unsigned int i = 0; i < session->format.nb_channels; ++i) {
                if (avFrame->data[i] && avFrameCopy->data[i]) {
                    size_t bytesPerSample = av_get_bytes_per_sample(session->format.sampleFormat);
                    size_t frameBytes = frame->getFrameSize() * bytesPerSample;
                    std::memcpy(avFrameCopy->data[i], avFrame->data[i], frameBytes);
                } else {
                    copySuccess = false;
                    break;
                }
            }
            break;

        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            // For float formats, copy extended data
            for (unsigned int i = 0; i < session->format.nb_channels; ++i) {
                if (avFrame->extended_data[i] && avFrameCopy->extended_data[i]) {
                    size_t frameBytes = frame->getFrameSize() * sizeof(float);
                    std::memcpy(avFrameCopy->extended_data[i],
                                avFrame->extended_data[i],
                                frameBytes);
                } else {
                    copySuccess = false;
                    break;
                }
            }
            break;

        default:
            copySuccess = false;
            JAMI_WARN("Unsupported sample format for capture: %s",
                      av_get_sample_fmt_name(session->format.sampleFormat));
            break;
        }
    }

    if (copySuccess) {
        frameCopy->has_voice = frame->has_voice;
        session->frames.push_back(frameCopy);
        session->frameCount++;

        if (debugLogging_ && (session->frameCount % 100 == 0)) {
            JAMI_DBG("Captured %zu frames for %s (context: %s)",
                     session->frameCount,
                     tracePointToString(point).c_str(),
                     context.c_str());
        }
    } else {
        JAMI_ERR("Failed to copy audio data for %s", tracePointToString(point).c_str());
    }
}

void
DebugAudioCapture::exportToWav(AudioTracePoint point, const std::string& filename)
{
    std::lock_guard lock(capturesMutex_);

    auto it = sessions_.find(point);
    if (it == sessions_.end() || !it->second || it->second->frames.empty()) {
        JAMI_WARN("No captured frames for %s", tracePointToString(point).c_str());
        return;
    }

    auto& session = it->second;

    // Validate the captured data
    if (debugLogging_) {
        JAMI_DBG("Exporting %s: %zu frames, format: %s, session: %s",
                 tracePointToString(point).c_str(),
                 session->frames.size(),
                 session->format.toString().c_str(),
                 session->sessionId.c_str());

        // Check for any null frames
        size_t nullFrames = 0;
        for (const auto& frame : session->frames) {
            if (!frame || !frame->pointer()) {
                nullFrames++;
            }
        }
        if (nullFrames > 0) {
            JAMI_WARN("Found %zu null frames in capture %s",
                      nullFrames,
                      tracePointToString(point).c_str());
        }
    }

    std::string outputFile = filename.empty() ? generateFilename(point, session->sessionId)
                                              : filename;

    try {
        std::filesystem::create_directories(std::filesystem::path(outputFile).parent_path());

        std::ofstream file(outputFile, std::ios::binary);
        if (!file) {
            JAMI_ERR("Failed to create WAV file: %s", outputFile.c_str());
            return;
        }

        // Calculate total samples
        size_t totalSamples = 0;
        for (const auto& frame : session->frames) {
            if (frame && frame->pointer()) {
                totalSamples += frame->getFrameSize();
            }
        }

        if (totalSamples == 0) {
            JAMI_ERR("No valid samples found in capture %s", tracePointToString(point).c_str());
            return;
        }

        writeWavHeader(file, session->format, totalSamples);
        writeWavData(file, session->frames);

        file.close();

        JAMI_LOG("Exported %zu frames (%zu samples) to WAV file: %s",
                 session->frames.size(),
                 totalSamples,
                 outputFile.c_str());

    } catch (const std::exception& e) {
        JAMI_ERR("Failed to export WAV file %s: %s", outputFile.c_str(), e.what());
    }
}

void
DebugAudioCapture::exportAllToWav(const std::string& prefix)
{
    std::lock_guard lock(capturesMutex_);

    for (const auto& [point, session] : sessions_) {
        if (session && !session->frames.empty()) {
            std::string filename = generateFilename(point, prefix + "_" + session->sessionId);
            exportToWav(point, filename);
        }
    }
}

void
DebugAudioCapture::setMaxCaptureFrames(size_t maxFrames)
{
    maxCaptureFrames_ = maxFrames;
    JAMI_DBG("Set max capture frames to %zu", maxFrames);
}

void
DebugAudioCapture::setOutputDirectory(const std::string& dir)
{
    outputDir_ = dir;
    try {
        std::filesystem::create_directories(outputDir_);
        JAMI_DBG("Set debug audio output directory to: %s", dir.c_str());
    } catch (const std::exception& e) {
        JAMI_WARN("Failed to create debug audio directory %s: %s", dir.c_str(), e.what());
    }
}

void
DebugAudioCapture::enableDebugLogging(bool enable)
{
    debugLogging_ = enable;
    JAMI_DBG("Debug audio capture logging: %s", enable ? "enabled" : "disabled");
}

std::string
DebugAudioCapture::getCaptureStatus() const
{
    std::lock_guard lock(capturesMutex_);

    std::stringstream ss;
    ss << "Debug Audio Capture Status:\n";
    ss << "Output Directory: " << outputDir_ << "\n";
    ss << "Max Frames: " << maxCaptureFrames_ << "\n";
    ss << "Debug Logging: " << (debugLogging_ ? "enabled" : "disabled") << "\n";
    ss << "Active Sessions:\n";

    for (const auto& [point, session] : sessions_) {
        if (session) {
            ss << "  " << tracePointToString(point) << ": ";
            if (session->active) {
                ss << "ACTIVE (" << session->frameCount
                   << " frames, session: " << session->sessionId << ")";
            } else {
                ss << "INACTIVE (" << session->frameCount
                   << " frames, session: " << session->sessionId << ")";
            }
            ss << "\n";
        }
    }

    return ss.str();
}

size_t
DebugAudioCapture::getCapturedFrameCount(AudioTracePoint point) const
{
    std::lock_guard lock(capturesMutex_);

    auto it = sessions_.find(point);
    if (it != sessions_.end() && it->second) {
        return it->second->frameCount;
    }

    return 0;
}

void
DebugAudioCapture::writeWavHeader(std::ofstream& file,
                                  const AudioFormat& format,
                                  size_t totalSamples)
{
    uint16_t channels = format.nb_channels;
    uint32_t sampleRate = format.sample_rate;
    uint16_t bitsPerSample;
    uint16_t audioFormat;
    uint32_t dataSize;
    uint32_t fileSize;

    // Determine format based on sample format
    switch (format.sampleFormat) {
    case AV_SAMPLE_FMT_S16:
        bitsPerSample = 16;
        audioFormat = 1; // PCM
        dataSize = totalSamples * channels * (bitsPerSample / 8);
        break;
    case AV_SAMPLE_FMT_S32:
        bitsPerSample = 32;
        audioFormat = 1; // PCM
        dataSize = totalSamples * channels * (bitsPerSample / 8);
        break;
    case AV_SAMPLE_FMT_FLT:
    case AV_SAMPLE_FMT_FLTP:
        bitsPerSample = 32;
        audioFormat = 3; // IEEE float
        dataSize = totalSamples * channels * (bitsPerSample / 8);
        break;
    default:
        throw std::runtime_error("Unsupported sample format for WAV export: "
                                 + std::string(av_get_sample_fmt_name(format.sampleFormat)));
    }

    fileSize = 36 + dataSize;

    // WAV header
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file.write("WAVE", 4);

    // Format chunk
    file.write("fmt ", 4);
    uint32_t fmtChunkSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtChunkSize), 4);
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);

    uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);

    uint16_t blockAlign = channels * (bitsPerSample / 8);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // Data chunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
}

void
DebugAudioCapture::writeWavData(std::ofstream& file,
                                const std::vector<std::shared_ptr<AudioFrame>>& frames)
{
    if (frames.empty() || !frames[0] || !frames[0]->pointer()) {
        return;
    }

    auto& firstFrame = frames[0];
    auto format = firstFrame->getFormat();

    // Handle different sample formats
    switch (format.sampleFormat) {
    case AV_SAMPLE_FMT_S16:
        writeWavDataS16(file, frames);
        break;
    case AV_SAMPLE_FMT_S32:
        writeWavDataS32(file, frames);
        break;
    case AV_SAMPLE_FMT_FLT:
        writeWavDataFloat(file, frames);
        break;
    case AV_SAMPLE_FMT_FLTP:
        writeWavDataFloatPlanar(file, frames);
        break;
    default:
        throw std::runtime_error("Unsupported sample format for WAV export: "
                                 + std::string(av_get_sample_fmt_name(format.sampleFormat)));
    }
}

void
DebugAudioCapture::writeWavDataS16(std::ofstream& file,
                                   const std::vector<std::shared_ptr<AudioFrame>>& frames)
{
    // Write 16-bit signed integer data directly
    for (const auto& frame : frames) {
        if (!frame || !frame->pointer())
            continue;

        auto avFrame = frame->pointer();
        size_t frameSize = frame->getFrameSize();
        unsigned int channels = avFrame->ch_layout.nb_channels;

        // For S16, data is already interleaved
        if (channels == 1) {
            // Mono - write directly
            file.write(reinterpret_cast<const char*>(avFrame->data[0]), frameSize * sizeof(int16_t));
        } else {
            // Stereo/multi-channel - interleave if needed
            for (size_t sample = 0; sample < frameSize; ++sample) {
                for (unsigned int ch = 0; ch < channels; ++ch) {
                    if (avFrame->data[ch]) {
                        int16_t* channelData = reinterpret_cast<int16_t*>(avFrame->data[ch]);
                        file.write(reinterpret_cast<const char*>(&channelData[sample]),
                                   sizeof(int16_t));
                    }
                }
            }
        }
    }
}

void
DebugAudioCapture::writeWavDataS32(std::ofstream& file,
                                   const std::vector<std::shared_ptr<AudioFrame>>& frames)
{
    // Write 32-bit signed integer data
    for (const auto& frame : frames) {
        if (!frame || !frame->pointer())
            continue;

        auto avFrame = frame->pointer();
        size_t frameSize = frame->getFrameSize();
        unsigned int channels = avFrame->ch_layout.nb_channels;

        // Interleave the channels
        for (size_t sample = 0; sample < frameSize; ++sample) {
            for (unsigned int ch = 0; ch < channels; ++ch) {
                if (avFrame->data[ch]) {
                    int32_t* channelData = reinterpret_cast<int32_t*>(avFrame->data[ch]);
                    file.write(reinterpret_cast<const char*>(&channelData[sample]), sizeof(int32_t));
                }
            }
        }
    }
}

void
DebugAudioCapture::writeWavDataFloat(std::ofstream& file,
                                     const std::vector<std::shared_ptr<AudioFrame>>& frames)
{
    // Write interleaved float32 data
    for (const auto& frame : frames) {
        if (!frame || !frame->pointer())
            continue;

        auto avFrame = frame->pointer();
        size_t frameSize = frame->getFrameSize();
        unsigned int channels = avFrame->ch_layout.nb_channels;

        // For FLT, data is already interleaved
        if (channels == 1) {
            // Mono - write directly
            file.write(reinterpret_cast<const char*>(avFrame->data[0]), frameSize * sizeof(float));
        } else {
            // Stereo/multi-channel - interleave if needed
            for (size_t sample = 0; sample < frameSize; ++sample) {
                for (unsigned int ch = 0; ch < channels; ++ch) {
                    if (avFrame->data[ch]) {
                        float* channelData = reinterpret_cast<float*>(avFrame->data[ch]);
                        file.write(reinterpret_cast<const char*>(&channelData[sample]),
                                   sizeof(float));
                    }
                }
            }
        }
    }
}

void
DebugAudioCapture::writeWavDataFloatPlanar(std::ofstream& file,
                                           const std::vector<std::shared_ptr<AudioFrame>>& frames)
{
    // Convert planar float32 to interleaved float32 for WAV
    for (const auto& frame : frames) {
        if (!frame || !frame->pointer())
            continue;

        auto avFrame = frame->pointer();
        size_t frameSize = frame->getFrameSize();
        unsigned int channels = avFrame->ch_layout.nb_channels;

        // Interleave the channels
        for (size_t sample = 0; sample < frameSize; ++sample) {
            for (unsigned int ch = 0; ch < channels; ++ch) {
                if (avFrame->extended_data[ch]) {
                    float* channelData = reinterpret_cast<float*>(avFrame->extended_data[ch]);
                    file.write(reinterpret_cast<const char*>(&channelData[sample]), sizeof(float));
                }
            }
        }
    }
}

std::string
DebugAudioCapture::tracePointToString(AudioTracePoint point) const
{
    switch (point) {
    case AudioTracePoint::INPUT_START:
        return "INPUT_START";
    case AudioTracePoint::INPUT_CAP:
        return "INPUT_CAP";
    case AudioTracePoint::RESAMPLER_IN:
        return "RESAMPLER_IN";
    case AudioTracePoint::RESAMPLER_OUT:
        return "RESAMPLER_OUT";
    case AudioTracePoint::PROCESSOR_IN:
        return "PROCESSOR_IN";
    case AudioTracePoint::PROCESSOR_OUT:
        return "PROCESSOR_OUT";
    case AudioTracePoint::OUTPUT_START:
        return "OUTPUT_START";
    case AudioTracePoint::OUTPUT_RENDER:
        return "OUTPUT_RENDER";
    default:
        return "UNKNOWN";
    }
}

std::string
DebugAudioCapture::generateFilename(AudioTracePoint point, const std::string& prefix) const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << outputDir_ << "/" << prefix << "_" << tracePointToString(point) << "_"
       << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".wav";

    return ss.str();
}

} // namespace jami