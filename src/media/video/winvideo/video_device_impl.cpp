/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
 *
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
        std::vector<VideoSize> getSizeList(const std::string& channel) const;
        std::vector<VideoSize> getSizeList() const;
        std::vector<FrameRate> getRateList(const std::string& channel, VideoSize size) const;

        DeviceParams getDeviceParams() const;
        void setDeviceParams(const DeviceParams&);

    private:
        std::unique_ptr<CaptureGraphInterfaces> cInterface;

        void setup();
        std::vector<VideoSize> sizeList_;
        std::map<VideoSize, std::vector<FrameRate> > rateList_;
        std::map<VideoSize, AM_MEDIA_TYPE*> capMap_;

        //AM_MEDIA_TYPE* findCap(const std::string& size);
        void fail(const std::string& error);
};

VideoDeviceImpl::VideoDeviceImpl(const std::string& id)
    : id(atoi(id.c_str()))
    , cInterface(new CaptureGraphInterfaces())
{
    setup();
}

void
VideoDeviceImpl::setup()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
        return fail("Could not initialize video device.");

    hr = CoCreateInstance(
        CLSID_CaptureGraphBuilder2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ICaptureGraphBuilder2,
        (void**) &cInterface->captureGraph_);
    if (FAILED(hr))
        return fail("Could not create the Filter Graph Manager");

    hr = CoCreateInstance(CLSID_FilterGraph,
        nullptr,
        CLSCTX_INPROC_SERVER,IID_IGraphBuilder,
        (void**) &cInterface->graph_);
    if (FAILED(hr))
        return fail("Could not add the graph builder!");

    hr = cInterface->captureGraph_->SetFiltergraph(cInterface->graph_);
    if (FAILED(hr))
        return fail("Could not set filtergraph.");

    ICreateDevEnum *pSysDevEnum = nullptr;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ICreateDevEnum,
        (void **)&pSysDevEnum);
    if (FAILED(hr))
        return fail("Could not create the enumerator!");

    IEnumMoniker* pEnumCat = nullptr;
    hr = pSysDevEnum->CreateClassEnumerator(
        CLSID_VideoInputDeviceCategory,
        &pEnumCat,
        0);
    if (SUCCEEDED(hr)) {
        // Auto-deletion at if {} exist or at exception
        auto IEnumMonikerDeleter = [](IEnumMoniker* p){ p->Release(); };
        std::unique_ptr<IEnumMoniker, decltype(IEnumMonikerDeleter)&> pEnumCatGuard {pEnumCat, IEnumMonikerDeleter};

        IMoniker *pMoniker = nullptr;
        ULONG cFetched;
        unsigned int deviceCounter = 0;
        while ((pEnumCatGuard->Next(1, &pMoniker, &cFetched) == S_OK))
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
                            fail("Could not add filter to video device.");
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
                                fail("Couldn't config the stream!");
                            }
                        }
                        break; // Device found
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
        if (SUCCEEDED(hr)) {
            int piCount;
            int piSize;
            cInterface->streamConf_->GetNumberOfCapabilities(&piCount, &piSize);
            AM_MEDIA_TYPE *pmt;
            VIDEO_STREAM_CONFIG_CAPS pSCC;
            for (int i = 0; i < piCount; i++) {
                cInterface->streamConf_->GetStreamCaps(i, &pmt, (BYTE*)&pSCC);
                if (pmt->formattype == FORMAT_VideoInfo) {
                    auto videoInfo = (VIDEOINFOHEADER*) pmt->pbFormat;
                    sizeList_.emplace_back(videoInfo->bmiHeader.biWidth, videoInfo->bmiHeader.biHeight);
                    rateList_[sizeList_.back()].emplace_back(1e7, pSCC.MinFrameInterval);
                    rateList_[sizeList_.back()].emplace_back(1e7, pSCC.MaxFrameInterval);
                    capMap_[sizeList_.back()] = pmt;
                }
            }
        }
    }
    pSysDevEnum->Release();
    pSysDevEnum = NULL;
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
    params.input = device;
    params.format = "dshow";

    AM_MEDIA_TYPE *pmt;
    HRESULT hr = cInterface->streamConf_->GetFormat(&pmt);
    if (SUCCEEDED(hr)) {
        if (pmt->formattype == FORMAT_VideoInfo) {
            auto videoInfo = (VIDEOINFOHEADER*) pmt->pbFormat;
            params.width = videoInfo->bmiHeader.biWidth;
            params.height = videoInfo->bmiHeader.biHeight;
            params.framerate = {1e7,  videoInfo->AvgTimePerFrame};
        }
    }
    return params;
}

void
VideoDeviceImpl::setDeviceParams(const DeviceParams& params)
{
    if (params.width and params.height) {
        auto pmt = capMap_.at(std::make_pair(params.width, params.height));
        if (pmt != nullptr) {
            ((VIDEOINFOHEADER*) pmt->pbFormat)->AvgTimePerFrame = (FrameRate(1e7) / params.framerate).real();
            if (FAILED(cInterface->streamConf_->SetFormat(pmt))) {
                RING_ERR("Could not set settings.");
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

VideoDevice::VideoDevice(const std::string& path, const std::vector<std::map<std::string, std::string>>&)
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

VideoDevice::~VideoDevice()
{}

}} // namespace ring::video
