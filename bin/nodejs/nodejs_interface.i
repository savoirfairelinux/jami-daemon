/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Emeric Vigier <emeric.vigier@savoirfairelinux.com>
 *  Author: Asad Salman <me@asad.co>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* File : nodejs_interface.i */
%module (directors="1") Ringservice

#define SWIG_JAVA_ATTACH_CURRENT_THREAD_AS_DAEMON
%include "typemaps.i"
%include "std_string.i" /* std::string typemaps */
%include "arrays_javascript.i";
%include "carrays.i";
%include "std_map.i";
%include "std_vector.i";
%include "stdint.i";


/* Avoid uint64_t to be converted to BigInteger */
%apply int64_t { uint64_t };

namespace std {

%extend map<string, string> {
    std::vector<std::string> keys() const {
        std::vector<std::string> k;
        k.reserve($self->size());
        for (const auto& i : *$self) {
            k.push_back(i.first);
        }
        return k;
    }
    void setRaw(std::string key, const vector<uint8_t>& value) {
        (*$self)[key] = std::string(value.data(), value.data()+value.size());
    }
    std::vector<uint8_t> getRaw(std::string key) {
        auto& v = $self->at(key);
        return {v.begin(), v.end()};
    }
}
%template(StringMap) map<string, string>;


%extend vector<string> {
  value_type set(int i, const value_type& in) throw (std::out_of_range) {
    const std::string old = $self->at(i);
    $self->at(i) = in;
    return old;
  }
  bool add(const value_type& in) {
    $self->push_back(in);
    return true;
  }
  int32_t size() const {
    return $self->size();
  }
}
%template(StringVect) vector<string>;


%template(VectMap) vector< map<string,string> >;
%template(IntegerMap) map<string,int>;
%template(IntVect) vector<int32_t>;
%template(UintVect) vector<uint32_t>;


%template(Blob) vector<uint8_t>;
%template(FloatVect) vector<float>;
}

/* not parsed by SWIG but needed by generated C files */
%header %{
#include <functional>
%}

/* parsed by SWIG to generate all the glue */
/* %include "../managerimpl.h" */
/* %include <client/callmanager.h> */

%include "managerimpl.i"
%include "callmanager.i"
%include "configurationmanager.i"
%include "presencemanager.i"
%include "callmanager.i"
%include "videomanager.i"
//#include "dring/callmanager_interface.h"

%header %{
#include "callback.h"
%}

//typemap for passing Callbacks
%typemap(in) v8::Local<v8::Function> {
    $1 = v8::Local<v8::Function>::Cast($input);
}

//typemap for handling map of functions
%typemap(in) const v8::Handle<v8::Value>  {
    $1 = $input;
}


%inline %{
/* some functions that need to be declared in *_wrap.cpp
 * that are not declared elsewhere in the c++ code
 */

void init(const v8::Handle<v8::Value> &funcMap){
    parseCbMap(funcMap);
    uv_async_init(uv_default_loop(), &signalAsync, handlePendingSignals);

    using namespace std::placeholders;
    using std::bind;
    using DRing::exportable_callback;
    using DRing::ConfigurationSignal;
    using DRing::CallSignal;
    using SharedCallback = std::shared_ptr<DRing::CallbackWrapperBase>;
    const std::map<std::string, SharedCallback> callEvHandlers = {
        exportable_callback<CallSignal::StateChange>(bind(&callStateChanged, _1, _2, _3)),
        exportable_callback<CallSignal::IncomingMessage>(bind(&incomingMessage, _1, _2, _3)),
        exportable_callback<CallSignal::IncomingCall>(bind(&incomingCall, _1, _2, _3)),
        exportable_callback<CallSignal::NewCallCreated>(bind(&newCallCreated, _1, _2, _3))
    };

    const std::map<std::string, SharedCallback> configEvHandlers = {
        exportable_callback<ConfigurationSignal::AccountsChanged>(bind(&accountsChanged)),
        exportable_callback<ConfigurationSignal::RegistrationStateChanged>(bind(&registrationStateChanged, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::ContactAdded>(bind(&contactAdded, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::ContactRemoved>(bind(&contactRemoved, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::ExportOnRingEnded>(bind(&exportOnRingEnded, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::NameRegistrationEnded>(bind(&nameRegistrationEnded, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::RegisteredNameFound>(bind(&registeredNameFound, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::VolatileDetailsChanged>(bind(&volatileDetailsChanged, _1, _2)),
        exportable_callback<ConfigurationSignal::KnownDevicesChanged>(bind(&knownDevicesChanged, _1, _2 )),
        exportable_callback<ConfigurationSignal::IncomingAccountMessage>(bind(&incomingAccountMessage, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::AccountMessageStatusChanged>(bind(&accountMessageStatusChanged, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::IncomingTrustRequest>(bind(&incomingTrustRequest, _1, _2, _3, _4 )),
    };

    if (!DRing::init(static_cast<DRing::InitFlag>(DRing::DRING_FLAG_DEBUG)))
        return;

    registerConfHandlers(configEvHandlers);
    registerCallHandlers(callEvHandlers);

    DRing::start();
}
%}
#ifndef SWIG
/* some bad declarations */
#endif