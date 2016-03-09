/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Edric Ladent-Milaret <edric.ladent-milaret@savoirfairelinux.com>
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

#ifndef PORTAUDIO_LAYER_H
#define PORTAUDIO_LAYER_H

#include <portaudio.h>

#include "audio/audiolayer.h"
#include "noncopyable.h"

namespace ring {

class PortAudioLayer : public AudioLayer {

public:
    PortAudioLayer(const AudioPreference &pref);
    ~PortAudioLayer();

    virtual std::vector<std::string> getCaptureDeviceList() const;
    virtual std::vector<std::string> getPlaybackDeviceList() const;

    virtual int getAudioDeviceIndex(const std::string& name, DeviceType type) const;
    virtual std::string getAudioDeviceName(int index, DeviceType type) const;
    virtual int getIndexCapture() const;
    virtual int getIndexPlayback() const;
    virtual int getIndexRingtone() const;

    /**
     * Start the capture stream and prepare the playback stream.
     * The playback starts accordingly to its threshold
     */
    virtual void startStream();

    /**
     * Stop the playback and capture streams.
     * Drops the pending frames and put the capture and playback handles to PREPARED state
     */
    virtual void stopStream();

    virtual void updatePreference(AudioPreference &pref, int index, DeviceType type);

 private:
    NON_COPYABLE(PortAudioLayer);

    void handleError(const PaError& err) const;
    void init(void);
    void terminate(void) const;
    void initStream(void);
    std::vector<std::string> getDeviceByType(const bool& playback) const;

    PaDeviceIndex   indexIn_;
    PaDeviceIndex   indexOut_;
    PaDeviceIndex   indexRing_;

    AudioBuffer playbackBuff_;

    std::shared_ptr<RingBuffer> mainRingBuffer_;

    enum Direction {Input=0, Output=1, End=2};
    PaStream*   streams[(int)Direction::End];

    static int paOutputCallback(const void *inputBuffer, void* outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData);

    static int paInputCallback(const void *inputBuffer, void* outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData);
 };
 }

#endif
