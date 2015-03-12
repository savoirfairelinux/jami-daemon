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
#include <comdef.h>

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
        //std::atomic_bool probing_;
        std::thread thread_;
};

#include <iostream>

VideoDeviceMonitorImpl::VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor) :
    monitor_(monitor),
    mutex_(),
    thread_()
{
    /* Enumerate existing devices */
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    std::cerr << "VIDEO DEVICE MONITOR" << std::endl;
    ICreateDevEnum *pDevEnum;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

    IEnumMoniker *pEnum;

    _com_error err(hr);
    LPCTSTR errMsg = err.ErrorMessage();
    std::cerr << errMsg << std::endl;
    if (SUCCEEDED(hr))
    {
       std::cerr << "HR SUCCESS" << std::endl;
        // Create an enumerator for the category.
        hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
        if (hr == S_FALSE)
        {
            std::cerr << "No webcam found." << std::endl;
            hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
        }
        pDevEnum->Release();
        IMoniker *pMoniker = NULL;

    while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
    {
        IPropertyBag *pPropBag;
        HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
        if (FAILED(hr))
        {
            pMoniker->Release();
            continue;
        }

        VARIANT var;
        VariantInit(&var);

        // Get description or friendly name.
        hr = pPropBag->Read(L"Description", &var, 0);
        if (FAILED(hr))
        {
            hr = pPropBag->Read(L"FriendlyName", &var, 0);
        }
        if (SUCCEEDED(hr))
        {
            printf("%S\n", var.bstrVal);
            char tmp[250];
            char DefChar = ' ';
            WideCharToMultiByte(CP_ACP,0,var.bstrVal,-1, tmp,250,&DefChar, NULL);
            try {
                std::cerr << tmp << std::endl;
                monitor_->addDevice(std::string("video=") + std::string(tmp));
            } catch (const std::runtime_error &e) {
            RING_ERR("%s", e.what());
            }
            VariantClear(&var);
        }

        hr = pPropBag->Write(L"FriendlyName", &var);

        // WaveInID applies only to audio capture devices.
        hr = pPropBag->Read(L"WaveInID", &var, 0);
        if (SUCCEEDED(hr))
        {
            printf("WaveIn ID: %d\n", var.lVal);
            VariantClear(&var);
        }

        hr = pPropBag->Read(L"DevicePath", &var, 0);
        if (SUCCEEDED(hr))
        {
            // The device path is not intended for display.
            printf("Device path: %S\n", var.bstrVal);
            // char tmp[250];
            // char DefChar = ' ';
            // WideCharToMultiByte(CP_ACP,0,var.bstrVal,-1, tmp,250,&DefChar, NULL);
            // try {
            //     std::cerr << tmp << std::endl;
            //     monitor_->addDevice(std::string(tmp));
            // } catch (const std::runtime_error &e) {
            // RING_ERR("%s", e.what());
            // }
            VariantClear(&var);
        }
        pPropBag->Release();
        pMoniker->Release();
    }
    }

    //TODO Alert when no device found

    return;
}

void VideoDeviceMonitorImpl::start()
{
    //probing_ = true;
    thread_ = std::thread(&VideoDeviceMonitorImpl::run, this);
}

VideoDeviceMonitorImpl::~VideoDeviceMonitorImpl()
{
    //probing_ = false;
    if (thread_.joinable())
        thread_.join();
}

void VideoDeviceMonitorImpl::run()
{
    RING_ERR("RUN");
    while (true) {
        //TODO: Enable detection of new devices
        sleep(1);
    }
}

VideoDeviceMonitor::VideoDeviceMonitor() :
    preferences_(), devices_(),
    monitorImpl_(new VideoDeviceMonitorImpl(this))
{
    RING_DBG("VIDEO DEVICE MONITOR");
    monitorImpl_->start();
}

VideoDeviceMonitor::~VideoDeviceMonitor()
{}

}} //ring::video namespace