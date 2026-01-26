/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
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
#pragma once

#include <mfidl.h>
#include <mfapi.h>
#include <mfobjects.h>

#ifndef METHODASYNCCALLBACK
#define METHODASYNCCALLBACK(Parent, AsyncCallback, pfnCallback) \
    class Callback##AsyncCallback : public IMFAsyncCallback \
    { \
    public: \
        Callback##AsyncCallback() \
            : _parent(((Parent*) ((BYTE*) this - offsetof(Parent, m_x##AsyncCallback)))) \
            , _dwQueueID(MFASYNC_CALLBACK_QUEUE_MULTITHREADED) \
        {} \
\
        STDMETHOD_(ULONG, AddRef)() \
        { \
            return _parent->AddRef(); \
        } \
        STDMETHOD_(ULONG, Release)() \
        { \
            return _parent->Release(); \
        } \
        STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) \
        { \
            if (riid == IID_IMFAsyncCallback || riid == IID_IUnknown) { \
                (*ppvObject) = this; \
                AddRef(); \
                return S_OK; \
            } \
            *ppvObject = NULL; \
            return E_NOINTERFACE; \
        } \
        STDMETHOD(GetParameters)(/* [out] */ __RPC__out DWORD * pdwFlags, /* [out] */ __RPC__out DWORD* pdwQueue) \
        { \
            *pdwFlags = 0; \
            *pdwQueue = _dwQueueID; \
            return S_OK; \
        } \
        STDMETHOD(Invoke)(/* [out] */ __RPC__out IMFAsyncResult * pResult) \
        { \
            _parent->pfnCallback(pResult); \
            return S_OK; \
        } \
        void SetQueueID(DWORD dwQueueID) \
        { \
            _dwQueueID = dwQueueID; \
        } \
\
    protected: \
        Parent* _parent; \
        DWORD _dwQueueID; \
\
    } m_x##AsyncCallback;
#endif
