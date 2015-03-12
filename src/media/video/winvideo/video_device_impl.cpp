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

#include <dshow.h>

#include <iostream>

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
         std::vector<std::string> getRateList(const std::string& channel, const std::string& size) const;
         float getRate(unsigned rate) const;

         VideoSettings getSettings() const;
         void applySettings(VideoSettings settings);

         DeviceParams getDeviceParams() const;

     private:
        ICaptureGraphBuilder2 *captureGraph_;
        IGraphBuilder *graph_;
        IBaseFilter *videoInputFilter_;
        IAMStreamConfig *streamConf_;

        void setup();
        std::vector<std::string> sizeList_;
        std::vector<std::string> rateList_;
    };

    VideoDeviceImpl::VideoDeviceImpl(const std::string& id) : id(atoi(id.c_str()))
    {
        setup();

        applySettings(VideoSettings());
    }

    void VideoDeviceImpl::setup()
    {
        RING_DBG("SETUP");
        std::cerr << "SETUP" << std::endl;
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**) &captureGraph_);
        if (FAILED(hr))
        {
            std::cerr << "ERROR - Could not create the Filter Graph Manager\n" << std::endl;
        }
        hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,IID_IGraphBuilder, (void**) &graph_);
        if (FAILED(hr))
        {
           std::cerr << "ERROR - Could not add the graph builder!\n" << std::endl;
       }
       hr = captureGraph_->SetFiltergraph(graph_);
       if (FAILED(hr))
       {
        printf("ERROR - Could not set filtergraph\n");
    }
    ICreateDevEnum *pSysDevEnum = NULL;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)&pSysDevEnum);
    if (FAILED(hr))
    {
        std::cerr << "ERROR - Could not add create the enumerator!\n" << std::endl;
    }
    IEnumMoniker *pEnumCat = NULL;
    hr = pSysDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumCat, 0);
    if (hr == S_OK)
    {
        IMoniker *pMoniker = NULL;
        ULONG cFetched;
        unsigned int deviceCounter = 0;
        bool done;
        while ((pEnumCat->Next(1, &pMoniker, &cFetched) == S_OK) && (!done))
        {
            if(deviceCounter == this->id)
            {
                IPropertyBag *pPropBag;
                hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pPropBag);
                if (SUCCEEDED(hr))
                {
                    VARIANT varName;
                    VariantInit(&varName);
                    hr = pPropBag->Read(L"FriendlyName", &varName, 0);
                    if (SUCCEEDED(hr))
                    {
                        int count = 0;
                        char *tmp;
                        int l = WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1, 0, 0, 0, 0);
                        tmp = new char[l];
                        WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1, tmp, l, 0, 0);
                        this->name = std::string(tmp);
                        this->device = std::string("video=") + this->name;
                        hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&videoInputFilter_);
                        if (SUCCEEDED(hr))
                            hr = graph_->AddFilter(videoInputFilter_, varName.bstrVal);
                        else
                            RING_ERR("Could not add filter to device.");
                    //FIXME: THE PIN_CATEGORY_PREVIEW (Could not work on some cam but is faster)
                        hr = captureGraph_->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, videoInputFilter_, IID_IAMStreamConfig, (void **)&streamConf_);
                        if(FAILED(hr))
                        {
                            printf("ERROR: Couldn't config the stream!\n");
                        }
                        done = true;
                    }
                    VariantClear(&varName);
                    pPropBag->Release();
                    pPropBag = NULL;
                    pMoniker->Release();
                    pMoniker = NULL;
                }
            }
            deviceCounter++;
        }
        pEnumCat->Release();
        pEnumCat = NULL;
    }
    pSysDevEnum->Release();
    pSysDevEnum = NULL;

    int piCount;
    int piSize;
    streamConf_->GetNumberOfCapabilities(&piCount, &piSize);
    AM_MEDIA_TYPE *pmt;
    VIDEO_STREAM_CONFIG_CAPS pSCC;
    for (int i = 0; i < piCount; i++) {
        streamConf_->GetStreamCaps(i, &pmt, (BYTE*)&pSCC);
        if (pmt->formattype == FORMAT_VideoInfo) {
            VIDEOINFOHEADER *videoInfo = (VIDEOINFOHEADER*) pmt->pbFormat;
            sizeList_.push_back(std::to_string(videoInfo->bmiHeader.biWidth) + "x" + std::to_string(videoInfo->bmiHeader.biHeight));
            rateList_.push_back(std::to_string((double)videoInfo->AvgTimePerFrame / 1000000));
        }
    }
}

void
VideoDeviceImpl::applySettings(VideoSettings settings)
{
    RING_DBG("APPLY SETTINGS");
//TODO: not supported for now on OSX
// Set preferences or fallback to defaults.
//    channel_ = getChannel(settings["channel"]);
//    size_ = channel_.getSize(settings["size"]);
//    rate_ = size_.getRate(settings["rate"]);
}

DeviceParams
VideoDeviceImpl::getDeviceParams() const
{
    DeviceParams params;

    params.input = device;
    params.format = "dshow";

    AM_MEDIA_TYPE *pmt;
    HRESULT hr = streamConf_->GetFormat(&pmt);
    if (SUCCEEDED(hr)) {
        if (pmt->formattype == FORMAT_VideoInfo) {
            VIDEOINFOHEADER *videoInfo = (VIDEOINFOHEADER*) pmt->pbFormat;
            params.width = videoInfo->bmiHeader.biWidth;
            params.height = videoInfo->bmiHeader.biHeight;
            params.framerate = videoInfo->AvgTimePerFrame / 1000000;
        }
    }
    return params;
}

VideoSettings
VideoDeviceImpl::getSettings() const
{
    RING_DBG("GET SETTINGS %s", name.c_str());
    VideoSettings settings;

    settings.name = name;
    settings.framerate = 0.33;
    settings.channel = "0";
    return settings;
}

VideoDevice::VideoDevice(const std::string& path) :
deviceImpl_(new VideoDeviceImpl(path))
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
VideoDeviceImpl::getRateList(const std::string& channel, const std::string& size) const
{
    return rateList_;
}

std::vector<std::string>
VideoDeviceImpl::getSizeList(const std::string& channel) const
{
    return sizeList_;
}

std::vector<std::string> VideoDeviceImpl::getChannelList() const
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

}} //ring::video namespace