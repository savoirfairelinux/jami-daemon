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
#pragma once

/*
 *  Example usage:
 *
 *  #include "debug_audio_capture.h"
 *
 *  void exampleUsage() {
 *      auto& debugCapture = DebugAudioCapture::instance();
 *      debugCapture.setOutputDirectory("./debug_audio_output");
 *      debugCapture.setMaxCaptureFrames(500); // ~5 seconds at 100fps
 *      debugCapture.enableDebugLogging(true);
 *
 *      WAV_LOG_START_CAPTURE(AudioTracePoint::INPUT_START, "test_session_44khz");
 *
 *      // Your audio processing code here...
 *      // The macros will automatically capture frames:
 *      // WAV_LOG_INPUT_START(frame);
 *      // WAV_LOG_RESAMPLER_IN(frame);
 *      // WAV_LOG_RESAMPLER_OUT(frame);
 *      // etc.
 *
 *      WAV_LOG_INPUT_STOP();
 *
 *      WAV_LOG_EXPORT_ALL("artifact_investigation");
 *
 *      // Or export specific trace point
 *      debugCapture.exportToWav(AudioTracePoint::INPUT_START, "./custom_input.wav");
 *  }
 *
 *  Note:
 *  - The macros will automatically capture frames at the specified trace points.
 *  - The frames are captured in the order of the macros.
 *  - The frames are captured in the order of the macros.
 */

#include "audio_format.h"
#include "media_buffer.h"
#include "noncopyable.h"

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <atomic>
#include <map>

namespace jami {

enum class AudioTracePoint {
    INPUT_START,   // Raw input from hardware
    INPUT_CAP,     // After initial capture processing
    RESAMPLER_IN,  // Before resampling
    RESAMPLER_OUT, // After resampling
    PROCESSOR_IN,  // Before audio processing
    PROCESSOR_OUT, // After audio processing
    OUTPUT_START,  // Before output processing
    OUTPUT_RENDER  // Final output to hardware
};

class DebugAudioCapture
{
private:
    NON_COPYABLE(DebugAudioCapture);

public:
    static DebugAudioCapture& instance();

    // Control capture
    void startCapture(AudioTracePoint point, const std::string& sessionId = "");
    void stopCapture(AudioTracePoint point);
    void stopAllCaptures();

    // Capture audio frame at specific trace point
    void captureFrame(AudioTracePoint point,
                      const std::shared_ptr<AudioFrame>& frame,
                      const std::string& context = "");

    // Output captured data as WAV files
    void exportToWav(AudioTracePoint point, const std::string& filename = "");
    void exportAllToWav(const std::string& prefix = "jami_debug");

    // Configuration
    void setMaxCaptureFrames(size_t maxFrames);
    void setOutputDirectory(const std::string& dir);
    void enableDebugLogging(bool enable);

    // Debugging and status
    std::string getCaptureStatus() const;
    size_t getCapturedFrameCount(AudioTracePoint point) const;

private:
    DebugAudioCapture();
    ~DebugAudioCapture();

    struct CaptureSession
    {
        std::vector<std::shared_ptr<AudioFrame>> frames;
        AudioFormat format {AudioFormat::NONE()};
        std::string sessionId;
        std::atomic<bool> active {false};
        size_t frameCount {0};
        std::chrono::steady_clock::time_point startTime;
    };

    std::map<AudioTracePoint, std::unique_ptr<CaptureSession>> sessions_;
    mutable std::mutex capturesMutex_;
    size_t maxCaptureFrames_ {1000}; // ~10 seconds at 100fps
    std::string outputDir_ {"./debug_audio"};
    std::atomic<bool> debugLogging_ {false};

    // WAV file writing
    void writeWavHeader(std::ofstream& file, const AudioFormat& format, size_t totalSamples);
    void writeWavData(std::ofstream& file, const std::vector<std::shared_ptr<AudioFrame>>& frames);
    void writeWavDataS16(std::ofstream& file,
                         const std::vector<std::shared_ptr<AudioFrame>>& frames);
    void writeWavDataS32(std::ofstream& file,
                         const std::vector<std::shared_ptr<AudioFrame>>& frames);
    void writeWavDataFloat(std::ofstream& file,
                           const std::vector<std::shared_ptr<AudioFrame>>& frames);
    void writeWavDataFloatPlanar(std::ofstream& file,
                                 const std::vector<std::shared_ptr<AudioFrame>>& frames);
    std::string tracePointToString(AudioTracePoint point) const;
    std::string generateFilename(AudioTracePoint point, const std::string& prefix) const;
};

} // namespace jami

// Convenience macros for audio debugging
#define WAV_LOG_INPUT_START(frame) \
    jami::DebugAudioCapture::instance().captureFrame(jami::AudioTracePoint::INPUT_START, \
                                                     frame, \
                                                     __FUNCTION__)

#define WAV_LOG_INPUT_CAP(frame) \
    jami::DebugAudioCapture::instance().captureFrame(jami::AudioTracePoint::INPUT_CAP, \
                                                     frame, \
                                                     __FUNCTION__)

#define WAV_LOG_RESAMPLER_IN(frame) \
    jami::DebugAudioCapture::instance().captureFrame(jami::AudioTracePoint::RESAMPLER_IN, \
                                                     frame, \
                                                     __FUNCTION__)

#define WAV_LOG_RESAMPLER_OUT(frame) \
    jami::DebugAudioCapture::instance().captureFrame(jami::AudioTracePoint::RESAMPLER_OUT, \
                                                     frame, \
                                                     __FUNCTION__)

#define WAV_LOG_PROCESSOR_IN(frame) \
    jami::DebugAudioCapture::instance().captureFrame(jami::AudioTracePoint::PROCESSOR_IN, \
                                                     frame, \
                                                     __FUNCTION__)

#define WAV_LOG_PROCESSOR_OUT(frame) \
    jami::DebugAudioCapture::instance().captureFrame(jami::AudioTracePoint::PROCESSOR_OUT, \
                                                     frame, \
                                                     __FUNCTION__)

#define WAV_LOG_OUTPUT_START(frame) \
    jami::DebugAudioCapture::instance().captureFrame(jami::AudioTracePoint::OUTPUT_START, \
                                                     frame, \
                                                     __FUNCTION__)

#define WAV_LOG_OUTPUT_RENDER(frame) \
    jami::DebugAudioCapture::instance().captureFrame(jami::AudioTracePoint::OUTPUT_RENDER, \
                                                     frame, \
                                                     __FUNCTION__)

// Control macros
#define WAV_LOG_START_CAPTURE(point, sessionId) \
    jami::DebugAudioCapture::instance().startCapture(point, sessionId)

#define WAV_LOG_STOP_CAPTURE(point) jami::DebugAudioCapture::instance().stopCapture(point)

#define WAV_LOG_EXPORT_ALL(prefix) jami::DebugAudioCapture::instance().exportAllToWav(prefix)

#define WAV_LOG_INPUT_STOP() WAV_LOG_STOP_CAPTURE(jami::AudioTracePoint::INPUT_START)

// Debugging macros
#define WAV_LOG_GET_STATUS() jami::DebugAudioCapture::instance().getCaptureStatus()
#define WAV_LOG_GET_FRAME_COUNT(point) \
    jami::DebugAudioCapture::instance().getCapturedFrameCount(point)
