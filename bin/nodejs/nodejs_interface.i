/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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

//typemap for passing Callbacks
%typemap(in) v8::Local<v8::Function> {
    $1 = v8::Local<v8::Function>::Cast($input);
}

//typemap for handling map of functions
%typemap(in) SWIGV8_VALUE  {
    $1 = $input;
}
%typemap(in) const SWIGV8_VALUE  {
    $1 = $input;
}
%typemap(varin) SWIGV8_VALUE {
  $result = $input;
}
%typemap(varin) const SWIGV8_VALUE {
  $result = $input;
}


%inline %{
/* some functions that need to be declared in *_wrap.cpp
 * that are not declared elsewhere in the c++ code
 */

void init(const SWIGV8_VALUE& funcMap){
    parseCbMap(funcMap);
    uv_async_init(uv_default_loop(), &signalAsync, handlePendingSignals);

    using namespace std::placeholders;
    using std::bind;
    using libjami::exportable_callback;
    using libjami::ConfigurationSignal;
    using libjami::CallSignal;
    using libjami::ConversationSignal;
    using SharedCallback = std::shared_ptr<libjami::CallbackWrapperBase>;
    const std::map<std::string, SharedCallback> callEvHandlers = {
        exportable_callback<CallSignal::StateChange>(bind(&callStateChanged, _1, _2, _3, _4)),
        exportable_callback<CallSignal::IncomingMessage>(bind(&incomingMessage, _1, _2, _3, _4)),
        exportable_callback<CallSignal::IncomingCall>(bind(&incomingCall, _1, _2, _3)),
        exportable_callback<CallSignal::IncomingCallWithMedia>(bind(&incomingCallWithMedia, _1, _2, _3, _4)),
        exportable_callback<CallSignal::MediaChangeRequested>(bind(&mediaChangeRequested, _1, _2, _3))
    };

    const std::map<std::string, SharedCallback> configEvHandlers = {
        exportable_callback<ConfigurationSignal::AccountsChanged>(bind(&accountsChanged)),
        exportable_callback<ConfigurationSignal::AccountDetailsChanged>(bind(&accountDetailsChanged, _1, _2)),
        exportable_callback<ConfigurationSignal::RegistrationStateChanged>(bind(&registrationStateChanged, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::ComposingStatusChanged>(bind(composingStatusChanged, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::ContactAdded>(bind(&contactAdded, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::ContactRemoved>(bind(&contactRemoved, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::ExportOnRingEnded>(bind(&exportOnRingEnded, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::NameRegistrationEnded>(bind(&nameRegistrationEnded, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::RegisteredNameFound>(bind(&registeredNameFound, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::VolatileDetailsChanged>(bind(&volatileDetailsChanged, _1, _2)),
        exportable_callback<ConfigurationSignal::KnownDevicesChanged>(bind(&knownDevicesChanged, _1, _2 )),
        exportable_callback<ConfigurationSignal::IncomingAccountMessage>(bind(&incomingAccountMessage, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::AccountMessageStatusChanged>(bind(&accountMessageStatusChanged, _1, _2, _3, _4, _5 )),
        exportable_callback<ConfigurationSignal::MessageSend>(bind(&logMessage, _1 )),
        exportable_callback<ConfigurationSignal::NeedsHost>(bind(&needsHost, _1, _2 )),
        exportable_callback<ConfigurationSignal::ActiveCallsChanged>(bind(&activeCallsChanged, _1, _2, _3 )),
        //exportable_callback<ConfigurationSignal::ProfileReceived>(bind(&profileReceived, _1, _2, _3, _4 )),
        //exportable_callback<ConfigurationSignal::IncomingTrustRequest>(bind(&incomingTrustRequest, _1, _2, _3, _4, _5 )),
    };

    const std::map<std::string, SharedCallback> dataTransferEvHandlers = {
        exportable_callback<libjami::DataTransferSignal::DataTransferEvent>(bind(&dataTransferEvent, _1, _2, _3, _4, _5)),
    };

    const std::map<std::string, SharedCallback> conversationHandlers = {
        exportable_callback<ConversationSignal::ConversationLoaded>(bind(&conversationLoaded, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::SwarmLoaded>(bind(&swarmLoaded, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::MessagesFound>(bind(&messagesFound, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::MessageReceived>(bind(&messageReceived, _1, _2, _3)),
        exportable_callback<ConversationSignal::SwarmMessageReceived>(bind(&swarmMessageReceived, _1, _2, _3)),
        exportable_callback<ConversationSignal::SwarmMessageUpdated>(bind(&swarmMessageUpdated, _1, _2, _3)),
        exportable_callback<ConversationSignal::ReactionAdded>(bind(&reactionAdded, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::ReactionRemoved>(bind(&reactionRemoved, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::ConversationProfileUpdated>(bind(&conversationProfileUpdated, _1, _2, _3)),
        exportable_callback<ConversationSignal::ConversationRequestReceived>(bind(&conversationRequestReceived, _1, _2, _3)),
        exportable_callback<ConversationSignal::ConversationRequestDeclined>(bind(&conversationRequestDeclined, _1, _2)),
        exportable_callback<ConversationSignal::ConversationReady>(bind(&conversationReady, _1, _2)),
        exportable_callback<ConversationSignal::ConversationRemoved>(bind(&conversationRemoved, _1, _2)),
        exportable_callback<ConversationSignal::ConversationMemberEvent>(bind(&conversationMemberEvent, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::OnConversationError>(bind(&onConversationError, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::ConversationPreferencesUpdated>(bind(&conversationPreferencesUpdated, _1, _2, _3))
    };

    if (!libjami::init(static_cast<libjami::InitFlag>(libjami::LIBJAMI_FLAG_DEBUG)))
        return;

    registerSignalHandlers(configEvHandlers);
    registerSignalHandlers(callEvHandlers);
    registerSignalHandlers(conversationHandlers);
    registerSignalHandlers(dataTransferEvHandlers);
    libjami::start();
}
%}
#ifndef SWIG
/* some bad declarations */
#endif