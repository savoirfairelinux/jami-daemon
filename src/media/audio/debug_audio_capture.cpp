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
     session->sessionId = sessionId.empty() ?
         std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count()) : sessionId;
     session->frameCount = 0;
     session->startTime = std::chrono::steady_clock::now();
     session->active = true;

     if (debugLogging_) {
         JAMI_DBG("Started audio capture for %s (session: %s)",
                  tracePointToString(point).c_str(), session->sessionId.c_str());
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
                      tracePointToString(point).c_str(), duration.count(), it->second->frameCount);
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
                       tracePointToString(point).c_str(), maxCaptureFrames_);
         }
         session->active = false;
         return;
     }

     // Store the format from the first frame
     if (session->frames.empty()) {
         session->format = frame->getFormat();
         if (debugLogging_) {
             JAMI_DBG("Audio capture %s format: %s",
                      tracePointToString(point).c_str(), session->format.toString().c_str());
         }
     }

     // Create a copy of the frame to avoid any threading issues
     auto frameCopy = std::make_shared<AudioFrame>(frame->getFormat(), frame->getFrameSize());
     if (frameCopy->pointer() && frameCopy->pointer()->extended_data &&
         frame->pointer()->extended_data) {

         // Copy the audio data
         for (unsigned int i = 0; i < session->format.nb_channels; ++i) {
             if (frame->pointer()->extended_data[i] && frameCopy->pointer()->extended_data[i]) {
                 std::memcpy(frameCopy->pointer()->extended_data[i],
                            frame->pointer()->extended_data[i],
                            frame->getFrameSize() * av_get_bytes_per_sample(session->format.sampleFormat));
             }
         }
         frameCopy->has_voice = frame->has_voice;

         session->frames.push_back(frameCopy);
         session->frameCount++;

         if (debugLogging_ && (session->frameCount % 100 == 0)) {
             JAMI_DBG("Captured %zu frames for %s (context: %s)",
                      session->frameCount, tracePointToString(point).c_str(), context.c_str());
         }
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
     std::string outputFile = filename.empty() ?
         generateFilename(point, session->sessionId) : filename;

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
             totalSamples += frame->getFrameSize();
         }

         writeWavHeader(file, session->format, totalSamples);
         writeWavData(file, session->frames);

         file.close();

         JAMI_LOG("Exported %zu frames (%zu samples) to WAV file: %s",
                  session->frames.size(), totalSamples, outputFile.c_str());

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

 void
 DebugAudioCapture::writeWavHeader(std::ofstream& file, const AudioFormat& format, size_t totalSamples)
 {
     // Only support float32 planar for now (can be extended)
     if (format.sampleFormat != AV_SAMPLE_FMT_FLTP) {
         throw std::runtime_error("Unsupported sample format for WAV export");
     }

     uint16_t channels = format.nb_channels;
     uint32_t sampleRate = format.sample_rate;
     uint16_t bitsPerSample = 32; // float32
     uint16_t audioFormat = 3; // IEEE float

     uint32_t dataSize = totalSamples * channels * (bitsPerSample / 8);
     uint32_t fileSize = 36 + dataSize;

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
 DebugAudioCapture::writeWavData(std::ofstream& file, const std::vector<std::shared_ptr<AudioFrame>>& frames)
 {
     // Convert planar float32 to interleaved float32 for WAV
     for (const auto& frame : frames) {
         if (!frame || !frame->pointer()) continue;

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
         case AudioTracePoint::INPUT_START: return "INPUT_START";
         case AudioTracePoint::INPUT_CAP: return "INPUT_CAP";
         case AudioTracePoint::RESAMPLER_IN: return "RESAMPLER_IN";
         case AudioTracePoint::RESAMPLER_OUT: return "RESAMPLER_OUT";
         case AudioTracePoint::PROCESSOR_IN: return "PROCESSOR_IN";
         case AudioTracePoint::PROCESSOR_OUT: return "PROCESSOR_OUT";
         case AudioTracePoint::OUTPUT_START: return "OUTPUT_START";
         case AudioTracePoint::OUTPUT_RENDER: return "OUTPUT_RENDER";
         default: return "UNKNOWN";
     }
 }

 std::string
 DebugAudioCapture::generateFilename(AudioTracePoint point, const std::string& prefix) const
 {
     auto now = std::chrono::system_clock::now();
     auto time_t = std::chrono::system_clock::to_time_t(now);

     std::stringstream ss;
     ss << outputDir_ << "/" << prefix << "_" << tracePointToString(point)
        << "_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".wav";

     return ss.str();
 }

 } // namespace jami