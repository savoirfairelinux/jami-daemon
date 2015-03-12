/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
 *  Author: Edric Milaret <edric.ladent-milaret@savoirfairelinux.com>
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

#include <algorithm>
#include <cassert>
#include <climits>
#include <map>
#include <string>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "logger.h"
#include "../video_device.h"
#include "capture_graph_interfaces.h"

#include <dshow.h>

namespace ring { namespace video {

class VideoDeviceImpl {
    public:
        /**
        * @throw std::runtime_error
        */
        VideoDeviceImpl(const std::string& path);
        std::string device;
        std::string name;
        unsigned int id;

        std::vector<std::string> getChannelList() const;
        std::vector<std::string> getSizeList(const std::string& channel) const;
        std::vector<std::string> getSizeList() const;
        std::vector<std::string> getRateList(const std::string& channel,
            const std::string& size) const;
        float getRate(unsigned rate) const;

        VideoSettings getSettings() const;
        void applySettings(VideoSettings settings);

        DeviceParams getDeviceParams() const;

    private:
        std::unique_ptr<CaptureGraphInterfaces> cInterface;

        void setup();
        std::vector<std::string> sizeList_;
        std::map<std::string, std::vector<std::string> > rateList_;
        std::map<std::string, AM_MEDIA_TYPE*> capMap_;
};

VideoDeviceImpl::VideoDeviceImpl(const std::string& id)
    : id(atoi(id.c_str()))
    , cInterface(new CaptureGraphInterfaces())
{
    setup();

    applySettings(VideoSettings());
}

void
VideoDeviceImpl::setup()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
        return RING_ERR("Could not initialize video device.");

    hr = CoCreateInstance(
        CLSID_CaptureGraphBuilder2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ICaptureGraphBuilder2,
        (void**) &cInterface->captureGraph_);
    if (FAILED(hr))
        return RING_ERR("Could not create the Filter Graph Manager");

    hr = CoCreateInstance(CLSID_FilterGraph,
        nullptr,
        CLSCTX_INPROC_SERVER,IID_IGraphBuilder,
        (void**) &cInterface->graph_);
    if (FAILED(hr))
      return RING_ERR("Could not add the graph builder!");

    hr = cInterface->captureGraph_->SetFiltergraph(cInterface->graph_);
    if (FAILED(hr))
        return RING_ERR("Could not set filtergraph.");

    ICreateDevEnum *pSysDevEnum = nullptr;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ICreateDevEnum,
        (void **)&pSysDevEnum);
    if (FAILED(hr))
        return RING_ERR("Could not create the enumerator!");

    IEnumMoniker *pEnumCat = nullptr;
    hr = pSysDevEnum->CreateClassEnumerator(
        CLSID_VideoInputDeviceCategory,
        &pEnumCat,
        0);
    if (SUCCEEDED(hr)) {
        IMoniker *pMoniker = nullptr;
        ULONG cFetched;
        unsigned int deviceCounter = 0;
        bool done = false;
        while ((pEnumCat->Next(1, &pMoniker, &cFetched) == S_OK) && (not done))
        {
            if (deviceCounter == this->id) {
                IPropertyBag *pPropBag;
                hr = pMoniker->BindToStorage(
                    0,
                    0,
                    IID_IPropertyBag,
                    (void **)&pPropBag);
                if (SUCCEEDED(hr)) {
                    VARIANT varName;
                    VariantInit(&varName);
                    hr = pPropBag->Read(L"FriendlyName", &varName, 0);
                    if (SUCCEEDED(hr)) {
                        int l = WideCharToMultiByte(
                            CP_UTF8,
                            0,
                            varName.bstrVal,
                            -1,
                            0, 0, 0, 0);
                        auto tmp = new char[l];
                        WideCharToMultiByte(
                            CP_UTF8,
                            0,
                            varName.bstrVal,
                            -1,
                            tmp,
                            l,
                            0, 0);
                        this->name = std::string(tmp);
                        this->device = std::string("video=") + this->name;
                        hr = pMoniker->BindToObject(
                            nullptr, nullptr,
                            IID_IBaseFilter,
                            (void**)&cInterface->videoInputFilter_);
                        if (SUCCEEDED(hr))
                            hr = cInterface->graph_->AddFilter(
                                cInterface->videoInputFilter_,
                                varName.bstrVal);
                        else {
                            RING_ERR("Could not add filter to device.");
                            break;
                        }
                        hr = cInterface->captureGraph_->FindInterface(
                            &PIN_CATEGORY_PREVIEW,
                            &MEDIATYPE_Video,
                            cInterface->videoInputFilter_,
                            IID_IAMStreamConfig,
                            (void **)&cInterface->streamConf_);
                        if(FAILED(hr)) {
                            hr = cInterface->captureGraph_->FindInterface(
                                &PIN_CATEGORY_CAPTURE,
                                &MEDIATYPE_Video,
                                cInterface->videoInputFilter_,
                                IID_IAMStreamConfig,
                                (void **)&cInterface->streamConf_);
                            if (FAILED(hr)) {
                                RING_ERR("Couldn't config the stream!");
                                break;
                            }
                        }
                        done = true;
                    }
                    VariantClear(&varName);
                    pPropBag->Release();
                    pPropBag = nullptr;
                    pMoniker->Release();
                    pMoniker = nullptr;
                }
            }
            deviceCounter++;
        }
        pEnumCat->Release();
        pEnumCat = nullptr;
        if (done && SUCCEEDED(hr)) {
            int piCount;
            int piSize;
            cInterface->streamConf_->GetNumberOfCapabilities(&piCount, &piSize);
            AM_MEDIA_TYPE *pmt;
            VIDEO_STREAM_CONFIG_CAPS pSCC;
            for (int i = 0; i < piCount; i++) {
                cInterface->streamConf_->GetStreamCaps(i, &pmt, (BYTE*)&pSCC);
                if (pmt->formattype == FORMAT_VideoInfo) {
                    auto videoInfo = (VIDEOINFOHEADER*) pmt->pbFormat;
                    sizeList_.push_back(
                        std::to_string(videoInfo->bmiHeader.biWidth) + "x" +
                        std::to_string(videoInfo->bmiHeader.biHeight));
                    rateList_[sizeList_.back()].push_back(
                        std::to_string(1e7 / pSCC.MinFrameInterval));
                    rateList_[sizeList_.back()].push_back(
                        std::to_string(1e7 / pSCC.MaxFrameInterval));
                    capMap_[sizeList_.back()] = pmt;
                }
            }
        }
    }
    pSysDevEnum->Release();
    pSysDevEnum = NULL;
}

void
VideoDeviceImpl::applySettings(VideoSettings settings)
{
    if (!settings.video_size.empty()) {
        auto pmt = capMap_[settings.video_size];
        ((VIDEOINFOHEADER*) pmt->pbFormat)->AvgTimePerFrame = settings.framerate;
        if (FAILED(cInterface->streamConf_->SetFormat(capMap_[settings.video_size]))) {
            RING_ERR("Could not set settings.");
        }
    }
}

DeviceParams
VideoDeviceImpl::getDeviceParams() const
{
    DeviceParams params;

    params.input = device;
    params.format = "dshow";

    AM_MEDIA_TYPE *pmt;
    HRESULT hr = cInterface->streamConf_->GetFormat(&pmt);
    if (SUCCEEDED(hr)) {
        if (pmt->formattype == FORMAT_VideoInfo) {
            auto videoInfo = (VIDEOINFOHEADER*) pmt->pbFormat;
            params.width = videoInfo->bmiHeader.biWidth;
            params.height = videoInfo->bmiHeader.biHeight;
            params.framerate = 1e7 /  videoInfo->AvgTimePerFrame;
        }
    }
    return params;
}

VideoSettings
VideoDeviceImpl::getSettings() const
{
    VideoSettings settings;

    settings.name = name;

    AM_MEDIA_TYPE *pmt;
    HRESULT hr = cInterface->streamConf_->GetFormat(&pmt);
    if (SUCCEEDED(hr)) {
        if (pmt->formattype == FORMAT_VideoInfo) {
            auto videoInfo = (VIDEOINFOHEADER*) pmt->pbFormat;
            settings.video_size = std::to_string(videoInfo->bmiHeader.biWidth) +
                "x" + std::to_string(videoInfo->bmiHeader.biHeight);
            settings.framerate = 1e7 / videoInfo->AvgTimePerFrame;
        }
    }
    return settings;
}

VideoDevice::VideoDevice(const std::string& path)
    : deviceImpl_(new VideoDeviceImpl(path))
{
    node_ = path;
    name = deviceImpl_->name;
}

DeviceParams
VideoDevice::getDeviceParams() const
{
    return deviceImpl_->getDeviceParams();
}

void
VideoDevice::applySettings(VideoSettings settings)
{
    deviceImpl_->applySettings(settings);
}

VideoSettings
VideoDevice::getSettings() const
{
    return deviceImpl_->getSettings();
}

std::vector<std::string>
VideoDeviceImpl::getSizeList() const
{
    return sizeList_;
}

std::vector<std::string>
VideoDeviceImpl::getRateList(const std::string& channel,
    const std::string& size) const
{
    (void) channel;
    return rateList_.at(size);
}

std::vector<std::string>
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

DRing::VideoCapabilities
VideoDevice::getCapabilities() const
{
    DRing::VideoCapabilities cap;

    for (const auto& chan : deviceImpl_->getChannelList()) {
        for (const auto& size : deviceImpl_->getSizeList(chan)) {
            cap[chan][size] = deviceImpl_->getRateList(chan, size);
        }
    }
    return cap;
}

VideoDevice::~VideoDevice()
{}

}} // namespace ring::video