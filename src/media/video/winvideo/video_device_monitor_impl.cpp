/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
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
#include <cerrno>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <stdexcept> // for std::runtime_error
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "../video_device_monitor.h"
#include "logger.h"
#include "noncopyable.h"

#include <dshow.h>
#include <dbt.h>

namespace ring { namespace video {

class VideoDeviceMonitorImpl {
    public:
        /*
        * This is the only restriction to the pImpl design:
        * as the Linux implementation has a thread, it needs a way to notify
        * devices addition and deletion.
        *
        * This class should maybe inherit from VideoDeviceMonitor instead of
        * being its pImpl.
        */
        VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor);
        ~VideoDeviceMonitorImpl();

        void start();

    private:
        NON_COPYABLE(VideoDeviceMonitorImpl);

        VideoDeviceMonitor* monitor_;

        void run();

        mutable std::mutex mutex_;
        bool probing_;
        std::thread thread_;
};

VideoDeviceMonitorImpl::VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor)
    : monitor_(monitor)
    , mutex_()
    , thread_()
{}

void
VideoDeviceMonitorImpl::start()
{
    probing_ = true;
    thread_ = std::thread(&VideoDeviceMonitorImpl::run, this);
}

VideoDeviceMonitorImpl::~VideoDeviceMonitorImpl()
{
    probing_ = false;
    if (thread_.joinable())
        thread_.join();
}

void
VideoDeviceMonitorImpl::run()
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        return;
    }

    while (probing_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(POLLING_INTERVAL_MILLISECONDS));
        auto currentDeviceList = enumerateVideoInputDevices();
        auto deviceList = monitor_->getDeviceList();
        for (auto device : deviceList) {
            auto it = std::find(currentDeviceList.begin(), currentDeviceList.end(), device);
            if (it != currentDeviceList.end()) {
                currentDeviceList.erase(it);
            } else {
                monitor_->removeDevice(device);
            }
        }
        for (auto device : currentDeviceList) {
            monitor_->addDevice(device);
        }
    }

    CoUninitialize();
}

HRESULT
VideoDeviceMonitorImpl::enumerateVideoInputDevices(IEnumMoniker **ppEnum)
{
    ICreateDevEnum *pDevEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if (SUCCEEDED(hr)) {
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, ppEnum, 0);
        if (hr == S_FALSE) {
            hr = VFW_E_NOT_FOUND;
        }
        pDevEnum->Release();
    }
    return hr;
}

std::vector<std::string>
VideoDeviceMonitorImpl::enumerateVideoInputDevices()
{
    std::vector<std::string> deviceList;

    ICreateDevEnum *pDevEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    if (FAILED(hr)) {
        RING_ERR() << "Can't enumerate webcams";
        return {};
    }

    IEnumMoniker *pEnum = nullptr;
    hr = enumerateVideoInputDevices(&pEnum);
    if (FAILED(hr) || pEnum == nullptr) {
        RING_ERR() << "No webcam found";
        return {};
    }

    IMoniker *pMoniker = NULL;
    unsigned deviceID = 0;
    while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {
        IPropertyBag *pPropBag;
        HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
        if (FAILED(hr)) {
            pMoniker->Release();
            continue;
        }

        VARIANT var;
        VariantInit(&var);
        hr = pPropBag->Read(L"Description", &var, 0);
        if (FAILED(hr)) {
            hr = pPropBag->Read(L"FriendlyName", &var, 0);
        }

        if (SUCCEEDED(hr)) {
            int wslen = ::SysStringLen(var.bstrVal);
            if (wslen != 0) {
                std::wstring ws(var.bstrVal, wslen);
                deviceList.push_back(std::string(ws.begin(), ws.end()));
            }
            VariantClear(&var);
        }

        hr = pPropBag->Write(L"FriendlyName", &var);

        pPropBag->Release();
        pMoniker->Release();

        deviceID++;
    }
    pEnum->Release();
    return deviceList;
}

VideoDeviceMonitor::VideoDeviceMonitor()
    : preferences_()
    , devices_()
    , monitorImpl_(new VideoDeviceMonitorImpl(this))
{
    monitorImpl_->start();
}

VideoDeviceMonitor::~VideoDeviceMonitor()
{}

}} // namespace ring::video
