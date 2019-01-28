/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
 *
 *  Authors: Damien Riegel <damien.riegel@savoirfairelinux.com>
 *           Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *           Ciro Santilli <ciro.santilli@savoirfairelinux.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

%header %{
#include <functional>
#include <list>
#include <mutex>

#include "dring/dring.h"
#include "dring/videomanager_interface.h"

class VideoCallback {
public:
    virtual ~VideoCallback(){}
    virtual void getCameraInfo(const std::string& device, std::vector<int> *formats, std::vector<unsigned> *sizes, std::vector<unsigned> *rates) {}
    virtual void setParameters(const std::string, const int format, const int width, const int height, const int rate) {}
    virtual void startCapture(const std::string& camid) {}
    virtual void stopCapture() {}
    virtual void decodingStarted(const std::string& id, const std::string& shm_path, int w, int h, bool is_mixer) {}
    virtual void decodingStopped(const std::string& id, const std::string& shm_path, bool is_mixer) {}
    virtual std::string startLocalRecorder(const bool& audioOnly, const std::string& filepath) {}
    virtual void stopLocalRecorder(const std::string& filepath) {}
    virtual void setDeviceOrientation(const std::string& name, int angle) {}
};
%}

%feature("director") VideoCallback;

namespace DRing {

void setDefaultDevice(const std::string& name);
std::string getDefaultDevice();

void startCamera();
void stopCamera();
bool hasCameraStarted();
void startAudioDevice();
void stopAudioDevice();
bool switchInput(const std::string& resource);
bool switchToCamera();
std::map<std::string, std::string> getSettings(const std::string& name);
void applySettings(const std::string& name, const std::map<std::string, std::string>& settings);

void registerSinkTarget(const std::string& sinkId, const DRing::SinkTarget& target);
void setDeviceOrientation(const std::string& name, int angle);
}

class VideoCallback {
public:
    virtual ~VideoCallback(){}
    virtual void getCameraInfo(const std::string& device, std::vector<int> *formats, std::vector<unsigned> *sizes, std::vector<unsigned> *rates){}
    virtual void setParameters(const std::string, const int format, const int width, const int height, const int rate) {}
    virtual void startCapture(const std::string& camid) {}
    virtual void stopCapture() {}
    virtual void decodingStarted(const std::string& id, const std::string& shm_path, int w, int h, bool is_mixer) {}
    virtual void decodingStopped(const std::string& id, const std::string& shm_path, bool is_mixer) {}
};
