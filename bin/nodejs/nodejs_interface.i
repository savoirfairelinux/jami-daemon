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

/* File : nodejs_interface.i */
%module (directors="1") JamiService

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

%include "managerimpl.i"
%include "callmanager.i"
%include "configurationmanager.i"
%include "presencemanager.i"
%include "callmanager.i"
%include "videomanager.i"
%include "conversation.i"
%include "datatransfer.i"

%header %{
#include "callback.h"
%}

/* Native init wrapper - bypasses SWIG type system to pass Napi::Value directly */
%wrapper %{
static Napi::Value _wrap_native_init(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsObject()) {
        Napi::Error::New(env, "init requires a callback map object").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    initJami(napi_env(env), napi_value(info[0]));
    return env.Undefined();
}
%}

/* Register the native init function during module initialization */
%init %{
do {
  Napi::PropertyDescriptor pd = Napi::PropertyDescriptor::Function("init", _wrap_native_init);
  NAPI_CHECK_MAYBE(exports.DefineProperties({ pd }));
} while (0);
%}

#ifndef SWIG
/* some bad declarations */
#endif
