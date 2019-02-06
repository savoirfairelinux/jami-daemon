/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
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
#include <string>
#include <thread>
#include <vector>
#include <cctype>

#include "../video_device_monitor.h"
#include "logger.h"
#include "noncopyable.h"

#include <dshow.h>
#include <dbt.h>
#include <SetupAPI.h>

namespace ring {
namespace video {

constexpr GUID guidCamera = { 0xe5323777, 0xf976, 0x4f5b, 0x9b, 0x55, 0xb9, 0x46, 0x99, 0xc4, 0x6e, 0x44 };

class VideoDeviceMonitorImpl {
public:
    VideoDeviceMonitorImpl(VideoDeviceMonitor* monitor);
    ~VideoDeviceMonitorImpl();

    void start();

private:
    NON_COPYABLE(VideoDeviceMonitorImpl);

    VideoDeviceMonitor* monitor_;

    void run();

    static const constexpr unsigned POLLING_INTERVAL_MILLISECONDS = 200;
    HRESULT enumerateVideoInputDevices(IEnumMoniker **ppEnum);
    std::vector<std::string> enumerateVideoInputDevices();

    mutable std::mutex mutex_;
    bool probing_;
    std::thread thread_;
    HWND hWnd_;
    static LRESULT CALLBACK WinProcCallback(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
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
    SendMessage(hWnd_, WM_DESTROY, 0, 0);
    if (thread_.joinable())
        thread_.join();
}

std::string
getDeviceFriendlyName(PDEV_BROADCAST_DEVICEINTERFACE pbdi)
{
    // (-_-) ?
    std::string friendlyName;
    std::string name = pbdi->dbcc_name;
    name = name.substr(4);
    auto pos = name.find("#");
    name.replace(pos, 1, "\\");
    pos = name.find_last_of("#");
    name = name.substr(0, pos);
    pos = name.find("#");
    name.replace(pos, 1, "\\");
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char c) { return std::toupper(c); }
    );

    DWORD dwFlag = DIGCF_ALLCLASSES;
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&guidCamera, 0, NULL, dwFlag);
    if (INVALID_HANDLE_VALUE == hDevInfo) {
        return {};
    }

    SP_DEVINFO_DATA* pspDevInfoData =
        (SP_DEVINFO_DATA*)HeapAlloc(GetProcessHeap(), 0, sizeof(SP_DEVINFO_DATA));
    pspDevInfoData->cbSize = sizeof(SP_DEVINFO_DATA);

    for (int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, pspDevInfoData); i++) {
        GUID guid;
        guid = pspDevInfoData->ClassGuid;

        DWORD DataT;
        DWORD nSize = 0;
        TCHAR buf[260];

        if (!SetupDiGetDeviceInstanceId(hDevInfo, pspDevInfoData, buf, sizeof(buf), &nSize)) {
            break;
        }

        std::string strBuf(&buf[0]);
        if (strBuf.find(name) != std::string::npos) {
            if (SetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData,
                SPDRP_FRIENDLYNAME, &DataT, (PBYTE)buf, sizeof(buf), &nSize)) {
                friendlyName = std::string(buf);
                break;
            }
        }
    }

    if (pspDevInfoData) {
        HeapFree(GetProcessHeap(), 0, pspDevInfoData);
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);

    return friendlyName;
}

bool
registerDeviceInterfaceToHwnd(HWND hWnd, HDEVNOTIFY *hDeviceNotify)
{
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;

    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = guidCamera;

    *hDeviceNotify = RegisterDeviceNotification(
        hWnd,
        &NotificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );

    if (nullptr == *hDeviceNotify) {
        return false;
    }

    return true;
}

LRESULT CALLBACK
VideoDeviceMonitorImpl::WinProcCallback(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lRet = 1;
    static HDEVNOTIFY hDeviceNotify;
    VideoDeviceMonitorImpl *pThis;

    switch (message) {
    case WM_CREATE:
    {
        auto createParams = reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams;
        pThis = static_cast<VideoDeviceMonitorImpl*>(createParams);
        SetLastError(0);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));

        if (!registerDeviceInterfaceToHwnd(hWnd, &hDeviceNotify)) {
            RING_ERR() << "RegisterDeviceInterfaceToHwnd failed!";
        }
    }
    break;

    case WM_DEVICECHANGE:
    {
        switch (wParam) {
        case DBT_DEVICEREMOVECOMPLETE:
        case DBT_DEVICEARRIVAL:
        {
            PDEV_BROADCAST_DEVICEINTERFACE p = (PDEV_BROADCAST_DEVICEINTERFACE)lParam;
            auto friendlyName = getDeviceFriendlyName(p);
            if (!friendlyName.empty()) {
                RING_DBG() << friendlyName << ((wParam == DBT_DEVICEARRIVAL) ? " plugged" : " unplugged");
                if (pThis = reinterpret_cast<VideoDeviceMonitorImpl*>(GetWindowLongPtr(hWnd, GWLP_USERDATA))) {
                    ((wParam == DBT_DEVICEARRIVAL) ? pThis->monitor_->addDevice(friendlyName) : pThis->monitor_->removeDevice(friendlyName));
                }
            }
        }
        break;
        default:
            break;
        }
        break;
    }
    break;

    case WM_CLOSE:
        UnregisterDeviceNotification(hDeviceNotify);
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        lRet = DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return lRet;
}

void
VideoDeviceMonitorImpl::run()
{

    static const char* className = "Message";
    WNDCLASSEX wx = {};
    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = WinProcCallback;
    wx.hInstance = reinterpret_cast<HINSTANCE>(GetModuleHandle(0));
    wx.lpszClassName = className;
    if (RegisterClassEx(&wx)) {
        hWnd_ = CreateWindowEx(0, className, "devicenotifications", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, this);
    }

    auto captureDeviceList = enumerateVideoInputDevices();
    for (auto node : captureDeviceList) {
        monitor_->addDevice(node);
    }

    if (probing_) {
        MSG msg;
        int retVal;
        while ((retVal = GetMessage(&msg, NULL, 0, 0)) != 0) {
            if (retVal != -1) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                RING_DBG() << "Got Message";
            }
        }
    }

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
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        return {};
    }

    std::vector<std::string> deviceList;

    ICreateDevEnum *pDevEnum;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
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
            auto deviceName = bstrToStdString(var.bstrVal);
            if (!deviceName.empty()) {
                deviceList.push_back(deviceName);
            }
            VariantClear(&var);
        }

        hr = pPropBag->Write(L"FriendlyName", &var);

        pPropBag->Release();
        pMoniker->Release();

        deviceID++;
    }
    pEnum->Release();
    CoUninitialize();

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

}
} // namespace ring::video
