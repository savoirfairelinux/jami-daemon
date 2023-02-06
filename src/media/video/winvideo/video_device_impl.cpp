/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Edric Milaret <edric.ladent-milaret@savoirfairelinux.com>
 *  Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>
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

#include <algorithm>
#include <cassert>
#include <climits>
#include <map>
#include <string>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>

#include "logger.h"
#include "../video_device.h"
#include "capture_graph_interfaces.h"

#include <dshow.h>

namespace jami {
namespace video {

class VideoDeviceImpl
{
public:
    /**
     * @throw std::runtime_error
     */
    VideoDeviceImpl(const std::string& id);
    std::string id;
    std::string name;

    std::vector<std::string> getChannelList() const;
    std::vector<VideoSize> getSizeList(const std::string& channel) const;
    std::vector<VideoSize> getSizeList() const;
    std::vector<FrameRate> getRateList(const std::string& channel, VideoSize size) const;

    DeviceParams getDeviceParams() const;
    void setDeviceParams(const DeviceParams&);

private:
    std::unique_ptr<CaptureGraphInterfaces> cInterface;

    void setup();
    std::vector<VideoSize> sizeList_;
    std::map<VideoSize, std::vector<FrameRate>> rateList_;
    std::map<VideoSize, AM_MEDIA_TYPE*> capMap_;
    FrameRate desktopFrameRate_ = {30};

    void fail(const std::string& error);
};

VideoDeviceImpl::VideoDeviceImpl(const std::string& id)
    : id(id)
    , name()
    , cInterface(new CaptureGraphInterfaces())
{
    setup();
}

void
VideoDeviceImpl::setup()
{
    if (id == DEVICE_DESKTOP) {
        name = DEVICE_DESKTOP;
        VideoSize size {0, 0};
        sizeList_.emplace_back(size);
        rateList_[size] = {FrameRate(5),
                           FrameRate(10),
                           FrameRate(15),
                           FrameRate(20),
                           FrameRate(25),
                           FrameRate(30),
                           FrameRate(60),
                           FrameRate(120),
                           FrameRate(144)};
        return;
    }
    HRESULT hr = CoCreateInstance(CLSID_CaptureGraphBuilder2,
                                  nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_ICaptureGraphBuilder2,
                                  (void**) &cInterface->captureGraph_);
    if (FAILED(hr))
        return fail("Could not create the Filter Graph Manager");

    hr = CoCreateInstance(CLSID_FilterGraph,
                          nullptr,
                          CLSCTX_INPROC_SERVER,
                          IID_IGraphBuilder,
                          (void**) &cInterface->graph_);
    if (FAILED(hr))
        return fail("Could not add the graph builder!");

    hr = cInterface->captureGraph_->SetFiltergraph(cInterface->graph_);
    if (FAILED(hr))
        return fail("Could not set filtergraph.");

    ICreateDevEnum* pDevEnum;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum,
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&pDevEnum));

    IEnumMoniker* pEnum = NULL;
    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
    if (hr == S_FALSE) {
        hr = VFW_E_NOT_FOUND;
    }
    pDevEnum->Release();
    if (FAILED(hr) || pEnum == nullptr) {
        JAMI_ERR() << "No webcam found";
        return;
    }

    // Auto-deletion at exception
    auto IEnumMonikerDeleter = [](IEnumMoniker* p) {
        p->Release();
    };
    std::unique_ptr<IEnumMoniker, decltype(IEnumMonikerDeleter)&> pEnumGuard {pEnum,
                                                                              IEnumMonikerDeleter};

    IMoniker* pMoniker = NULL;
    while ((pEnumGuard->Next(1, &pMoniker, NULL) == S_OK)) {
        IPropertyBag* pPropBag;
        hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**) &pPropBag);
        if (FAILED(hr)) {
            pMoniker->Release();
            continue;
        }

        IBindCtx* bind_ctx = NULL;
        LPOLESTR olestr = NULL;

        hr = CreateBindCtx(0, &bind_ctx);
        if (hr != S_OK) {
            continue;
        }
        hr = pMoniker->GetDisplayName(bind_ctx, NULL, &olestr);
        if (hr != S_OK) {
            continue;
        }
        auto unique_name = to_string(olestr);
        if (unique_name.empty()) {
            continue;
        }

        // replace ':' with '_' since ffmpeg uses : to delineate between sources */
        std::replace(unique_name.begin(), unique_name.end(), ':', '_');
        unique_name = std::string("video=") + unique_name;

        // We want to get the capabilities of a device with the unique_name
        // that corresponds to what was enumerated by the video device monitor.
        if (unique_name.find(this->id) == std::string::npos) {
            continue;
        }

        this->id = unique_name;

        // get friendly name
        VARIANT var;
        VariantInit(&var);
        hr = pPropBag->Read(L"Description", &var, 0);
        if (FAILED(hr)) {
            hr = pPropBag->Read(L"FriendlyName", &var, 0);
        }
        if (SUCCEEDED(hr)) {
            this->name = to_string(var.bstrVal);
        }
        pPropBag->Write(L"FriendlyName", &var);

        hr = pMoniker->BindToObject(nullptr,
                                    nullptr,
                                    IID_IBaseFilter,
                                    (void**) &cInterface->videoInputFilter_);
        if (SUCCEEDED(hr))
            hr = cInterface->graph_->AddFilter(cInterface->videoInputFilter_, var.bstrVal);
        else {
            fail("Could not add filter to video device.");
        }
        hr = cInterface->captureGraph_->FindInterface(&PIN_CATEGORY_PREVIEW,
                                                      &MEDIATYPE_Video,
                                                      cInterface->videoInputFilter_,
                                                      IID_IAMStreamConfig,
                                                      (void**) &cInterface->streamConf_);
        if (FAILED(hr)) {
            hr = cInterface->captureGraph_->FindInterface(&PIN_CATEGORY_CAPTURE,
                                                          &MEDIATYPE_Video,
                                                          cInterface->videoInputFilter_,
                                                          IID_IAMStreamConfig,
                                                          (void**) &cInterface->streamConf_);
            if (FAILED(hr)) {
                fail("Couldn't config the stream!");
            }
        }

        VariantClear(&var);
        pPropBag->Release();

        // Device found.
        break;
    }
    pMoniker->Release();

    if (FAILED(hr) || cInterface->streamConf_ == NULL) {
        fail("Could not find the video device.");
    }

    int piCount;
    int piSize;
    cInterface->streamConf_->GetNumberOfCapabilities(&piCount, &piSize);
    AM_MEDIA_TYPE* pmt;
    VIDEO_STREAM_CONFIG_CAPS pSCC;
    std::map<std::pair<jami::video::VideoSize, jami::video::FrameRate>, LONG> bitrateList;
    for (int i = 0; i < piCount; i++) {
        cInterface->streamConf_->GetStreamCaps(i, &pmt, (BYTE*) &pSCC);
        if (pmt->formattype != FORMAT_VideoInfo) {
            continue;
        }
        auto videoInfo = (VIDEOINFOHEADER*) pmt->pbFormat;
        auto size = jami::video::VideoSize(videoInfo->bmiHeader.biWidth,
                                           videoInfo->bmiHeader.biHeight);
        // use 1e7 / MinFrameInterval to get maximum fps
        auto rate = jami::video::FrameRate(1e7, pSCC.MinFrameInterval);
        auto bitrate = videoInfo->dwBitRate;
        // Only add configurations with positive bitrates.
        if (bitrate == 0)
            continue;
        // Avoid adding multiple rates with different bitrates.
        auto ratesIt = rateList_.find(size);
        if (ratesIt != rateList_.end()
            && std::find(ratesIt->second.begin(), ratesIt->second.end(), rate)
                   != ratesIt->second.end()) {
            // Update bitrate and cap map if the bitrate is greater.
            auto key = std::make_pair(size, rate);
            if (bitrate > bitrateList[key]) {
                bitrateList[key] = bitrate;
                capMap_[size] = pmt;
            }
            continue;
        }
        // Add new size, rate, bitrate, and cap map.
        sizeList_.emplace_back(size);
        rateList_[size].emplace_back(rate);
        bitrateList[std::make_pair(size, rate)] = bitrate;
        capMap_[size] = pmt;
    }
}

void
VideoDeviceImpl::fail(const std::string& error)
{
    throw std::runtime_error(error);
}

DeviceParams
VideoDeviceImpl::getDeviceParams() const
{
    DeviceParams params;

    params.name = name;
    params.unique_id = id;
    params.input = id;
    if (id == DEVICE_DESKTOP) {
        params.format = "dxgigrab";
        params.framerate = desktopFrameRate_;
        return params;
    }

    params.format = "dshow";

    AM_MEDIA_TYPE* pmt;
    HRESULT hr = cInterface->streamConf_->GetFormat(&pmt);
    if (SUCCEEDED(hr)) {
        if (pmt->formattype == FORMAT_VideoInfo) {
            auto videoInfo = (VIDEOINFOHEADER*) pmt->pbFormat;
            params.width = videoInfo->bmiHeader.biWidth;
            params.height = videoInfo->bmiHeader.biHeight;
            params.framerate = {1e7, static_cast<double>(videoInfo->AvgTimePerFrame)};
        }
    }
    return params;
}

void
VideoDeviceImpl::setDeviceParams(const DeviceParams& params)
{
    if (id == DEVICE_DESKTOP) {
        desktopFrameRate_ = params.framerate;
        return;
    }
    if (params.width and params.height) {
        auto pmt = capMap_.at(std::make_pair(params.width, params.height));
        if (pmt != nullptr) {
            ((VIDEOINFOHEADER*) pmt->pbFormat)->AvgTimePerFrame
                = (FrameRate(1e7) / params.framerate).real();
            if (FAILED(cInterface->streamConf_->SetFormat(pmt))) {
                JAMI_ERR("Could not set settings.");
            }
        }
    }
}

std::vector<VideoSize>
VideoDeviceImpl::getSizeList() const
{
    return sizeList_;
}

std::vector<FrameRate>
VideoDeviceImpl::getRateList(const std::string& channel, VideoSize size) const
{
    (void) channel;
    return rateList_.at(size);
}

std::vector<VideoSize>
VideoDeviceImpl::getSizeList(const std::string& channel) const
{
    (void) channel;
    return sizeList_;
}

std::vector<std::string>
VideoDeviceImpl::getChannelList() const
{
    return {"default"};
}

VideoDevice::VideoDevice(const std::string& path,
                         const std::vector<std::map<std::string, std::string>>&)
    : deviceImpl_(new VideoDeviceImpl(path))
{
    id_ = path;
    name = deviceImpl_->name;
}

DeviceParams
VideoDevice::getDeviceParams() const
{
    return deviceImpl_->getDeviceParams();
}

void
VideoDevice::setDeviceParams(const DeviceParams& params)
{
    return deviceImpl_->setDeviceParams(params);
}

std::vector<std::string>
VideoDevice::getChannelList() const
{
    return deviceImpl_->getChannelList();
}

std::vector<VideoSize>
VideoDevice::getSizeList(const std::string& channel) const
{
    return deviceImpl_->getSizeList(channel);
}

std::vector<FrameRate>
VideoDevice::getRateList(const std::string& channel, VideoSize size) const
{
    return deviceImpl_->getRateList(channel, size);
}

VideoDevice::~VideoDevice() {}

} // namespace video
} // namespace jami
